#!/usr/bin/env python3
"""Read-only verification of a native UNISSUED associative sheet-metal export.

Independently recomputes neutral bend stations and checks CSV, DXF and SVG
geometry. Checksums are integrity only: no provenance, refold/press-brake
feasibility, material springback or production release is certified.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

NAMES={'manifest.json','flat-pattern.svg','flat-pattern.dxf','bend-schedule.csv'}
FIELDS={'bend_index','direction','angle_degrees','inside_radius_mm','k_factor',
        'bend_allowance_mm','bend_deduction_mm','bend_station_mm','flat_length_mm','sheet_width_mm'}
HASH=re.compile(r'[0-9a-f]{64}\Z')

def _number(value, lo:float, hi:float, name:str) -> float:
    if isinstance(value,bool):raise ValueError(f'Invalid {name}')
    try:v=float(value)
    except (TypeError,ValueError,OverflowError):raise ValueError(f'Invalid {name}') from None
    if not math.isfinite(v) or not lo<=v<=hi:raise ValueError(f'Invalid {name}')
    return v

def _near(first:float,second:float,tolerance:float=1.2e-6)->bool:
    return abs(first-second)<=tolerance

def _lines(dxf:bytes)->list[tuple[str,tuple[float,float,float,float]]]:
    try:lines=dxf.decode('ascii').splitlines()
    except UnicodeDecodeError:raise ValueError('DXF must be ASCII') from None
    if len(lines)%2:raise ValueError('Malformed DXF group codes')
    groups=list(zip(lines[::2],lines[1::2]))
    if not groups or groups[-1]!=('0','EOF'):raise ValueError('DXF has no EOF')
    found=[];i=0
    while i<len(groups):
        key,value=groups[i]
        if key=='0' and value=='LINE':
            i+=1;attributes={}
            while i<len(groups) and groups[i][0]!='0':
                code,item=groups[i]
                if code in attributes:raise ValueError('Duplicate DXF line coordinate field')
                attributes[code]=item;i+=1
            if not {'8','10','20','11','21','30','31'}<=attributes.keys():
                raise ValueError('Incomplete DXF line')
            if attributes['8'] not in ('OUTLINE','BEND'):
                raise ValueError('Unexpected DXF layer')
            coords=tuple(_number(attributes[k],-1e7,1e7,'DXF coordinate') for k in ('10','20','11','21'))
            if _number(attributes['30'],-1e-12,1e-12,'DXF z') or _number(attributes['31'],-1e-12,1e-12,'DXF z'):
                raise ValueError('DXF is not planar')
            found.append((attributes['8'],coords));continue
        if key=='0' and value not in {'SECTION','ENDSEC','TABLE','ENDTAB','LTYPE','LAYER','EOF'}:
            raise ValueError('Unexpected DXF entity type')
        i+=1
    return found

def _outline(length,width,stations,relief_width,relief_depth):
    xs=[]
    if relief_width:
        for x in stations:xs.append((x-relief_width/2,x+relief_width/2))
    points=[(0.,0.)]
    for a,b in xs:points.extend(((a,0.),(a,relief_depth),(b,relief_depth),(b,0.)))
    points.extend(((length,0.),(length,width)))
    for a,b in reversed(xs):points.extend(((b,width),(b,width-relief_depth),(a,width-relief_depth),(a,width)))
    points.extend(((0.,width),(0.,0.)))
    return points

def _paths_match(actual:list,expected:list)->bool:
    return len(actual)==len(expected) and all(_near(a[0],b[0]) and _near(a[1],b[1]) for a,b in zip(actual,expected))

def verify(folder:Path,*,minimum_center_spacing_mm:float=0.0)->dict:
    folder=Path(folder)
    if not folder.is_dir() or folder.is_symlink() or {p.name for p in folder.iterdir()}!=NAMES:
        raise ValueError('Expected exactly four native sheet-metal package files')
    sizes={'manifest.json':1024*1024,'flat-pattern.svg':2*1024*1024,
           'flat-pattern.dxf':4*1024*1024,'bend-schedule.csv':1024*1024}
    files={}
    for name,limit in sizes.items():
        p=folder/name
        if p.is_symlink() or not p.is_file() or p.stat().st_size>limit or p.stat().st_size==0:
            raise ValueError(f'Missing, linked, empty or oversized: {name}')
        files[name]=p.read_bytes()
    mf=json.loads(files['manifest.json'].decode('utf-8'))
    if not isinstance(mf,dict) or mf.get('format')!='CADWorkbench associative multi-flange package v1':
        raise ValueError('Unsupported linked flat-package manifest')
    for key in ('folded_source_id','flat_body_id','source_geometry_token','linked_source_token'):
        if not isinstance(mf.get(key),str) or not 0<len(mf[key])<=256:
            raise ValueError('Missing saved source identity or revision')
    if mf['source_geometry_token']!=mf['linked_source_token']:
        raise ValueError('Flat blank refers to a stale folded source revision')
    if not isinstance(mf.get('sha256'),dict) or set(mf['sha256'])!=NAMES-{'manifest.json'}:
        raise ValueError('Incomplete linked flat-package checksum inventory')
    for name,digest in mf['sha256'].items():
        if not isinstance(digest,str) or not HASH.fullmatch(digest) or hashlib.sha256(files[name]).hexdigest()!=digest:
            raise ValueError('Linked flat-package checksum mismatch')
    width=_number(mf.get('width_mm'),1e-5,1e6,'sheet width')
    thickness=_number(mf.get('thickness_mm'),1e-5,1e6,'sheet thickness')
    rw=_number(mf.get('relief_width_mm'),0.,1e6,'relief width')
    rd=_number(mf.get('relief_depth_mm'),0.,1e6,'relief depth')
    min_space=_number(minimum_center_spacing_mm,0.,1e6,'shop minimum bend-center distance')
    if (rw==0)!=(rd==0) or rd>=width/2:
        raise ValueError('Invalid relief dimensions')
    flanges=mf.get('straight_flange_lengths_mm')
    bends=mf.get('bends');k=mf.get('per_bend_k_factors')
    if not isinstance(bends,list) or not 1<=len(bends)<=50 or not isinstance(k,list) or len(k)!=len(bends) or not isinstance(flanges,list) or len(flanges)!=len(bends)+1:
        raise ValueError('Folded flange and bend inventories do not match')
    lengths=[_number(v,1e-5,1e6,'flange') for v in flanges]
    length=lengths[0];stations=[];allowances=[];angles=[];radii=[];factors=[];direction_changes=0;last_sign=0
    for i,(bend,rawk) in enumerate(zip(bends,k)):
        if not isinstance(bend,dict):raise ValueError('Invalid bend row')
        angle=_number(bend.get('signed_angle_degrees'),-165,165,'bend angle')
        if abs(angle)<15:raise ValueError('Unsupported bend angle')
        radius=_number(bend.get('inside_radius_mm'),1e-5,1e6,'inside radius')
        factor=_number(rawk,0.,1.,'K-factor')
        sign=1 if angle>0 else -1
        direction_changes+=int(last_sign!=0 and last_sign!=sign);last_sign=sign
        allowance=math.radians(abs(angle))*(radius+factor*thickness)
        station=length+allowance/2
        allowances.append(allowance);stations.append(station)
        angles.append(angle);radii.append(radius);factors.append(factor)
        length+=allowance+lengths[i+1]
    if not math.isfinite(length) or length>1e12:raise ValueError('Invalid developed length')
    if rw and any(x-rw/2<=0 or length-x-rw/2<=0 for x in stations):
        raise ValueError('End relief outside blank')
    if rw and any(stations[i]-stations[i-1]<=rw for i in range(1,len(stations))):
        raise ValueError('Overlapping bend reliefs')
    with io.StringIO(files['bend-schedule.csv'].decode('utf-8-sig'),newline='') as stream:
        reader=csv.DictReader(stream)
        if reader.fieldnames is None or set(reader.fieldnames)!=FIELDS or len(reader.fieldnames)!=len(FIELDS):
            raise ValueError('Invalid bend schedule header')
        rows=list(reader)
    if len(rows)!=len(bends):raise ValueError('Bend schedule count differs from CAD')
    for i,row in enumerate(rows):
        if None in row or any(v is None for v in row.values()):raise ValueError('Malformed bend schedule row')
        if row['bend_index']!=str(i+1) or row['direction']!=('UP' if angles[i]>0 else 'DOWN'):
            raise ValueError('Bend schedule order/direction mismatch')
        checks={'angle_degrees':angles[i],'inside_radius_mm':radii[i],
            'k_factor':factors[i],'bend_allowance_mm':allowances[i],
            'bend_station_mm':stations[i],'flat_length_mm':length,'sheet_width_mm':width}
        for key,value in checks.items():
            if not _near(_number(row[key],-1e12,1e12,key),value,2e-7):
                raise ValueError(f'CAD bend schedule mismatch: {key}')
        setback=(radii[i]+thickness)*math.tan(math.radians(abs(angles[i]))/2)
        expected_deduction=2*setback-allowances[i]
        if not _near(_number(row['bend_deduction_mm'],-1e12,1e12,'bend deduction'),
                     expected_deduction,2e-7):
            raise ValueError('CAD bend deduction differs from the source geometry')
    expected=_outline(length,width,stations,rw,rd)
    lines=_lines(files['flat-pattern.dxf'])
    outline=[row[1] for row in lines if row[0]=='OUTLINE']
    bendlines=[row[1] for row in lines if row[0]=='BEND']
    expectedEdges=list(zip(expected,expected[1:]))
    if len(outline)!=len(expectedEdges) or len(bendlines)!=len(stations):
        raise ValueError('DXF line or bend inventory mismatch')
    for actual,(a,b) in zip(outline,expectedEdges):
        if not _paths_match([(actual[0],actual[1]),(actual[2],actual[3])],[a,b]):
            raise ValueError('DXF blank outline disagrees with saved CAD dimensions')
    for line,station in zip(bendlines,stations):
        if not _paths_match([(line[0],line[1]),(line[2],line[3])],
                            [(station,rd),(station,width-rd)]):
            raise ValueError('DXF bend line disagrees with source')
    if b'<!DOCTYPE' in files['flat-pattern.svg'].upper() or b'<!ENTITY' in files['flat-pattern.svg'].upper():
        raise ValueError('Untrusted SVG document type or entity declaration')
    svg=ET.fromstring(files['flat-pattern.svg'])
    if svg.get('data-document-type')!='folded-sheet-flat-preview' or svg.get('data-bend-count')!=str(len(stations)):
        raise ValueError('Unexpected sheet SVG schema')
    viewport=svg.get('viewBox','').split()
    if len(viewport)!=4 or not _near(_number(viewport[2],0,1e12,'SVG width'),length) or not _near(_number(viewport[3],0,1e12,'SVG height'),width):
        raise ValueError('SVG preview dimensions disagree with DXF')
    paths={child.get('id'):child for child in svg if child.tag.endswith('}path') or child.tag=='path'}
    if set(paths)!={'blank-outline'}|{f'bend-{i+1}' for i in range(len(stations))}:
        raise ValueError('SVG paths or bend identities missing')
    outlineStr=paths['blank-outline'].get('d','')
    tokens=outlineStr.split()
    if len(tokens)!=len(expected) + 1 or tokens[-1]!='Z':
        # The exporter omits the explicit closing point; Z closes to 0,0.
        if len(tokens)!=len(expected) or tokens[-1]!='Z':
            raise ValueError('SVG outline path mismatch')
    actualPoints=[]
    for item in tokens[:-1]:
        if not item or item[0] not in ('M','L'):raise ValueError('Unsupported SVG outline command')
        try:x,y=map(float,item[1:].split(','))
        except ValueError:raise ValueError('Invalid SVG outline point') from None
        actualPoints.append((x,y))
    if not (_paths_match(actualPoints,expected[:-1]) or _paths_match(actualPoints,expected)):
        raise ValueError('SVG blank outline disagrees with DXF')
    for i,(station,angle) in enumerate(zip(stations,angles)):
        element=paths[f'bend-{i+1}']
        if not _near(_number(element.get('data-angle-degrees'),-165,165,'SVG angle'),angle):
            raise ValueError('SVG bend direction mismatch')
        expectedBend=f'M{station:.6f},{rd:.6f} V{width-rd:.6f}'
        if element.get('d')!=expectedBend:raise ValueError('SVG bend path mismatch')
    spacing=[stations[i]-stations[i-1] for i in range(1,len(stations))]
    too_close=[i for i,d in enumerate(spacing,1) if d+1e-9<min_space]
    return {'status':'UNISSUED_FLAT_DEVELOPMENT_REVIEW_REQUIRED','release_approved':False,
            'source_body_id':mf['folded_source_id'],'flat_body_id':mf['flat_body_id'],
            'source_geometry_token':mf['source_geometry_token'],'bend_count':len(stations),
            'developed_length_mm':length,'width_mm':width,'bend_stations_mm':stations,
            'reverse_bend_count':direction_changes,'minimum_bend_center_spacing_mm':min(spacing) if spacing else None,
            'shop_minimum_bend_center_spacing_mm':min_space,
            'too_close_bend_pairs':too_close,'shop_spacing_review_required':bool(too_close),
            'manifest_sha256':hashlib.sha256(files['manifest.json']).hexdigest(),
            'review_required':['Material springback and tool radius','Tool accessibility and fold sequence',
                  'Physical blank/flat pattern and clearance measurement']}

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('folder',type=Path)
    ap.add_argument('--minimum-center-spacing-mm',type=float,default=0.)
    args=ap.parse_args()
    try:print(json.dumps(verify(args.folder,minimum_center_spacing_mm=args.minimum_center_spacing_mm),indent=2));return 0
    except (OSError,ValueError,TypeError,KeyError,ET.ParseError) as exc:
        print(f'FAIL: {exc}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())

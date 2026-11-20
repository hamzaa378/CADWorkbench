#!/usr/bin/env python3
"""Independently verify one exported CADWorkbench multi-part UNISSUED flat job.

Checks each native part's source geometry, DXF/SVG/CSV, the batch index,
and when present the stock plan's hashes and dimensional totals. It is not
nesting optimization, kerf, machine feasibility or engineering approval.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import sys
from verify_linked_flat_package import verify as verify_flat, _number, _near

HEX=re.compile(r'[0-9a-f]{64}\Z')
INDEX=('part_folder','flat_body_id','folded_source_id','svg_sha256','dxf_sha256','bend_csv_sha256')
STOCK=('sheet','flat_body_id','x_mm','y_mm','length_mm','width_mm','rotated_90_deg')
CUT=('flat_body_id','material_area_mm2','relief_area_mm2','exposed_cut_length_mm')


def _csv(raw:bytes,columns:tuple[str,...]):
    import io
    with io.StringIO(raw.decode('utf-8-sig'),newline='') as stream:
        reader=csv.DictReader(stream)
        if tuple(reader.fieldnames or ())!=columns:raise ValueError('Batch CSV columns are unexpected')
        rows=list(reader)
    if any(None in r or any(v is None for v in r.values()) for r in rows):
        raise ValueError('Malformed batch CSV row')
    return rows

def _file(folder:Path,name:str,size_limit:int=8*1024*1024):
    path=folder/name
    if path.is_symlink() or not path.is_file() or not 0<path.stat().st_size<=size_limit:
        raise ValueError('Missing, linked or oversized batch file: '+name)
    return path.read_bytes()

def _hash(raw:bytes,digest:str):
    return isinstance(digest,str) and bool(HEX.fullmatch(digest)) and hashlib.sha256(raw).hexdigest()==digest

def verify(folder:Path,*,minimum_center_spacing_mm:float=0.0)->dict:
    folder=Path(folder)
    if not folder.is_dir() or folder.is_symlink():raise ValueError('Missing or linked sheet-metal batch folder')
    manifest=json.loads(_file(folder,'batch-manifest.json',1024*1024))
    if (not isinstance(manifest,dict) or
        manifest.get('format')!='CADWorkbench multi-part associative sheet-metal job v1'):
        raise ValueError('Unsupported native batch manifest')
    parts=manifest.get('parts')
    if not isinstance(parts,list) or not 2<=len(parts)<=24 or manifest.get('part_count')!=len(parts):
        raise ValueError('Invalid multi-part flat job inventory')
    raw_index=_file(folder,'batch-index.csv',1024*1024)
    if not _hash(raw_index,manifest.get('index_csv_sha256')):raise ValueError('Batch index digest mismatch')
    index=_csv(raw_index,INDEX)
    if len(index)!=len(parts):raise ValueError('Batch part index count mismatch')
    seen_flat=set();seen_paths=set();details=[]
    for row,entry in zip(index,parts):
        if not isinstance(entry,dict) or set(entry)!=(set(INDEX)-{'part_folder'})|{'package_directory'}:
            raise ValueError('Unexpected part entry in native batch manifest')
        if row['part_folder']!=entry['package_directory'] or any(
                row[key]!=entry[key] for key in INDEX if key!='part_folder'):
            raise ValueError('Batch index and manifest disagree')
        name=entry['package_directory'];flat=entry['flat_body_id']
        if not isinstance(name,str) or not re.fullmatch(r'part_[0-9]{3}_[A-Za-z0-9_.-]{1,12}',name):
            raise ValueError('Unsafe or malformed native part directory')
        if name in seen_paths or flat in seen_flat:
            raise ValueError('Duplicate part identity in sheet-metal job')
        seen_paths.add(name);seen_flat.add(flat)
        sub=folder/name
        package=verify_flat(sub,minimum_center_spacing_mm=minimum_center_spacing_mm)
        if package['flat_body_id']!=flat or package['source_body_id']!=entry['folded_source_id']:
            raise ValueError('Batch part folder and CAD source identity disagree')
        for field,file in (('svg_sha256','flat-pattern.svg'),('dxf_sha256','flat-pattern.dxf'),
                           ('bend_csv_sha256','bend-schedule.csv')):
            if not _hash(_file(sub,file),entry[field]):
                raise ValueError('Batch digest disagrees with inner CAD package')
        details.append(package)
    layout=manifest.get('stock_layout')
    expected={'batch-index.csv','batch-manifest.json'}|seen_paths
    stock_summary=None
    if layout is not None:
        if not isinstance(layout,dict):raise ValueError('Invalid stock layout')
        expected.update({'stock-layout.csv','stock-layout.svg','cut-length-summary.csv'})
        raw_stock=_file(folder,'stock-layout.csv',1024*1024)
        raw_svg=_file(folder,'stock-layout.svg',8*1024*1024)
        raw_cut=_file(folder,'cut-length-summary.csv',1024*1024)
        for data,key in ((raw_stock,'stock_layout_csv_sha256'),
                         (raw_svg,'stock_layout_svg_sha256'),
                         (raw_cut,'cut_length_csv_sha256')):
            if not _hash(data,layout.get(key)):
                raise ValueError('Stock planning evidence checksum mismatch')
        sheetw=_number(layout.get('stock_width_mm'),1e-5,1e6,'stock width')
        sheeth=_number(layout.get('stock_height_mm'),1e-5,1e6,'stock height')
        gap=_number(layout.get('gap_mm'),0.,1e6,'stock gap')
        sheetcount=layout.get('sheet_count')
        if type(sheetcount)!=int or not 1<=sheetcount<=24:
            raise ValueError('Invalid stock sheet count')
        rawrows=_csv(raw_stock,STOCK)
        if len(rawrows)!=len(parts) or {r['flat_body_id'] for r in rawrows}!=seen_flat:
            raise ValueError('Stock placement identities disagree with CAD parts')
        rectangles=[]
        for row in rawrows:
            sheet=int(row['sheet'])
            if not 1<=sheet<=sheetcount or row['rotated_90_deg'] not in ('YES','NO'):
                raise ValueError('Invalid stock sheet assignment')
            x=_number(row['x_mm'],0.,1e9,'stock x');y=_number(row['y_mm'],0.,1e9,'stock y')
            w=_number(row['length_mm'],1e-5,1e9,'blank length')
            h=_number(row['width_mm'],1e-5,1e9,'blank width')
            part=next(p for p in details if p['flat_body_id']==row['flat_body_id'])
            nominal_w,nominal_h=part['developed_length_mm'],part['width_mm']
            if row['rotated_90_deg']=='YES':nominal_w,nominal_h=nominal_h,nominal_w
            if not _near(w,nominal_w) or not _near(h,nominal_h):
                raise ValueError('Stock placement dimensions differ from actual flat blank')
            if x+w>sheetw+1e-7 or y+h>sheeth+1e-7:
                raise ValueError('Stock placement exceeds sheet dimensions')
            rectangles.append((sheet,x,y,x+w,y+h))
        if not layout.get('relief_aware_contour_placement',False):
            for i,(sh,x0,y0,x1,y1) in enumerate(rectangles):
                for sh2,a0,b0,a1,b1 in rectangles[i+1:]:
                    if sh==sh2 and min(x1,a1)-max(x0,a0)>-gap+1e-7 and min(y1,b1)-max(y0,b0)>-gap+1e-7:
                        raise ValueError('Stock rectangles violate the declared minimum gap')
        cuts=_csv(raw_cut,CUT)
        if len(cuts)!=len(parts) or {r['flat_body_id'] for r in cuts}!=seen_flat:
            raise ValueError('Cut-length summary missing source bodies')
        reported_area=0.;total_cut=0.;expected_envelope=0.
        for row in cuts:
            area=_number(row['material_area_mm2'],1e-7,1e12,'material area')
            cut=_number(row['exposed_cut_length_mm'],0.,1e12,'cut length')
            removed=_number(row['relief_area_mm2'],0.,1e12,'relief area')
            part=next(p for p in details if p['flat_body_id']==row['flat_body_id'])
            envelope=part['developed_length_mm']*part['width_mm']
            expected_removed=2*part['bend_count']*_relief_area(folder,parts,part['flat_body_id'])
            # A relief rectangle can never ADD material; verify both additive
            # area bookkeeping and the actual source-defined notch areas.
            if not _near(area+removed,envelope,2e-5) or not _near(removed,expected_removed,2e-5):
                raise ValueError('Cut/material area differs from native flat contour')
            expected_envelope+=envelope;reported_area+=area;total_cut+=cut
        if not _near(expected_envelope,_number(layout.get('blank_envelope_area_mm2'),0.,1e14,'envelope area'),2e-5) or not _near(reported_area,_number(layout.get('net_contour_material_area_mm2'),0.,1e14,'net material area'),2e-5) or not _near(total_cut,_number(layout.get('total_exposed_neutral_cut_length_mm'),0.,1e14,'cut length total'),2e-5):
            raise ValueError('Stock layout material totals do not reconcile')
        stock_summary={'sheet_count':sheetcount,'part_count':len(parts),
                       'relief_aware':bool(layout.get('relief_aware_contour_placement')),
                       'material_area_mm2':reported_area,'total_exposed_neutral_cut_length_mm':total_cut}
    if {p.name for p in folder.iterdir()}!=expected:
        raise ValueError('Missing or extra files in sheet-metal batch')
    return {'status':'UNISSUED_MULTI_PART_FLAT_JOB_REVIEW_REQUIRED','release_approved':False,
            'part_count':len(details),'parts':details,'stock_summary':stock_summary,
            'batch_manifest_sha256':hashlib.sha256(_file(folder,'batch-manifest.json')).hexdigest(),
            'review_required':['Independent press-brake and material verification',
                               'Kerf, grain direction and actual nesting/toolpath verification']}

def _relief_area(folder,parts,flat):
    import json
    entry=next(p for p in parts if p['flat_body_id']==flat)
    src=json.loads(_file(folder/entry['package_directory'],'manifest.json',1024*1024))
    return _number(src['relief_width_mm'],0.,1e6,'relief width')*_number(src['relief_depth_mm'],0.,1e6,'relief depth')

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('folder',type=Path);p.add_argument('--minimum-center-spacing-mm',type=float,default=0.)
    args=p.parse_args()
    try:print(json.dumps(verify(args.folder,minimum_center_spacing_mm=args.minimum_center_spacing_mm),indent=2));return 0
    except (OSError,TypeError,ValueError,KeyError,UnicodeError) as exc:
        print(f'FAIL: {exc}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())

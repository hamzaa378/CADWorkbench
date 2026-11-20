#!/usr/bin/env python3
"""Build an external, checksum-verified UNISSUED engineering review evidence folder.

Inputs are verified read-only. Publishes a new directory atomically: any
failure leaves no partial published report. A SHA-256 manifest checks integrity,
not provenance, authenticity, metrology certification or design approval.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from engineering_readiness_workflow import evaluate,render_csv
from engineering_revision_impact import impact
from drawing_revision_diff import compare_drawings
from review_measured_holes import review as review_inspection
from reconcile_drawing_handoff import reconcile

FILES={'readiness.json','actions.csv','revision_impact.json','drawing_delta.json',
       'inspection_review.json','drawing_reconciliation.json','sheet_metal_review.json'}


def _json(value:dict)->bytes:
    return (json.dumps(value,ensure_ascii=False,indent=2,sort_keys=True)+'\n').encode('utf-8')


def build_bundle(current:Path,destination:Path,*,previous:Path|None=None,
                 measured:Path|None=None,drawings:Path|None=None,
                 previous_drawings:Path|None=None,previous_measurements:Path|None=None,
                 sheet_metal_packages:tuple[Path,...]=(),minimum_bend_center_spacing_mm:float=0.0,
                 sheet_metal_batch:Path|None=None)->dict:
    current=Path(current).resolve()
    destination=Path(destination).resolve()
    inputs=[x.resolve() for x in (current,previous,measured,drawings,previous_drawings,previous_measurements,sheet_metal_batch,*sheet_metal_packages)
            if x is not None]
    if (destination.exists() or not destination.parent.is_dir() or
            any(destination==x or x in destination.parents or destination in x.parents for x in inputs)):
        raise ValueError('Evidence destination must be a NEW directory external to all inputs')
    if previous is None and (previous_drawings is not None or previous_measurements is not None):
        raise ValueError('Previous engineering evidence requires previous CAD handoff')
    if (previous_drawings is None)!=(drawings is None) and previous_drawings is not None:
        raise ValueError('Previous drawing comparison requires current drawings')
    # Verify everything before constructing a published directory.
    readiness=evaluate(current,measured_csv=measured,drawing_folder=drawings,
        previous_handoff=previous,previous_drawings=previous_drawings,
        previous_measurements=previous_measurements,
        sheet_metal_packages=tuple(sheet_metal_packages),
        minimum_bend_center_spacing_mm=minimum_bend_center_spacing_mm,
        sheet_metal_batch=sheet_metal_batch)
    bodies={'readiness.json':_json(readiness),'actions.csv':render_csv(readiness).encode('utf-8')}
    if sheet_metal_packages or sheet_metal_batch is not None:
        bodies['sheet_metal_review.json']=_json({'status':'UNISSUED_SHEET_METAL_REVIEW',
            'release_approved':False,'packages':readiness['sheet_metal_summary'],
            'batch':readiness['sheet_metal_batch_summary']})
    if previous is not None:
        bodies['revision_impact.json']=_json(impact(previous,current,
            previous_drawings=previous_drawings,current_drawings=drawings if previous_drawings else None,
            previous_measurements=previous_measurements))
    if drawings is not None:bodies['drawing_reconciliation.json']=_json(reconcile(current,drawings))
    if measured is not None:bodies['inspection_review.json']=_json(review_inspection(current,measured))
    if previous_drawings is not None and drawings is not None:
        bodies['drawing_delta.json']=_json(compare_drawings(previous_drawings,drawings))
    files=[{'path':name,'sha256':hashlib.sha256(payload).hexdigest(),'bytes':len(payload)}
           for name,payload in sorted(bodies.items())]
    manifest={'schema_version':1,'project':'CADWorkbench',
              'status':'UNISSUED_REVIEW_EVIDENCE_ONLY','release_approved':False,
              'source_handoff_manifest_sha256':hashlib.sha256((current/'handoff_manifest.json').read_bytes()).hexdigest(),
              'previous_handoff_manifest_sha256':(hashlib.sha256((Path(previous)/'handoff_manifest.json').read_bytes()).hexdigest()
                   if previous is not None else None),
              'open_action_count':readiness['open_action_count'],'files':files,
              'caution':'Hashes are not signatures. Qualified engineering release and metrology signoff remain separate.'}
    with tempfile.TemporaryDirectory(prefix='.cad_unissued_build_',dir=str(destination.parent)) as temp:
        staging=Path(temp)/'report'
        staging.mkdir()
        for name,payload in bodies.items():(staging/name).write_bytes(payload)
        (staging/'evidence_manifest.json').write_bytes(_json(manifest))
        verify_bundle(staging)
        staging.rename(destination)  # same parent filesystem; atomic if destination absent
    return manifest


def verify_bundle(folder:Path)->dict:
    folder=Path(folder)
    if not folder.is_dir() or folder.is_symlink():raise ValueError('Evidence folder missing or symlinked')
    mf=folder/'evidence_manifest.json'
    if not mf.is_file() or mf.is_symlink() or mf.stat().st_size>1024*1024:
        raise ValueError('Missing or invalid evidence manifest')
    manifest=json.loads(mf.read_text(encoding='utf-8'))
    if (not isinstance(manifest,dict) or manifest.get('schema_version')!=1 or
            manifest.get('status')!='UNISSUED_REVIEW_EVIDENCE_ONLY' or
            manifest.get('release_approved') is not False or
            not isinstance(manifest.get('files'),list)):
        raise ValueError('Invalid or approved evidence manifest')
    declared=set()
    for row in manifest['files']:
        if not isinstance(row,dict) or set(row)!={'path','bytes','sha256'}:
            raise ValueError('Invalid evidence file record')
        name=row['path']
        if name not in FILES or name in declared:
            raise ValueError('Unknown or duplicate evidence payload')
        declared.add(name)
        file=folder/name
        if file.is_symlink() or not file.is_file() or file.stat().st_size>32*1024*1024:
            raise ValueError('Missing or oversized evidence payload')
        raw=file.read_bytes()
        if len(raw)!=row['bytes'] or hashlib.sha256(raw).hexdigest()!=row['sha256']:
            raise ValueError('Evidence payload checksum mismatch')
    if not {'readiness.json','actions.csv'}<=declared:
        raise ValueError('Required integrated review reports missing')
    # A regenerated checksum alone must not conceal inconsistent review rows.
    readiness=json.loads((folder/'readiness.json').read_text(encoding='utf-8'))
    if (not isinstance(readiness,dict) or readiness.get('release_approved') is not False or
        readiness.get('status')!='UNISSUED_INTEGRATED_ENGINEERING_REVIEW_REQUIRED' or
        readiness.get('open_action_count')!=manifest.get('open_action_count') or
        readiness.get('open_action_count')!=len(readiness.get('actions',[])) or
        (folder/'actions.csv').read_bytes()!=render_csv(readiness).encode('utf-8')):
        raise ValueError('Engineering action register disagrees with signed evidence inventory')
    if 'sheet_metal_review.json' in declared:
        flat=json.loads((folder/'sheet_metal_review.json').read_text(encoding='utf-8'))
        if (flat.get('release_approved') is not False or flat.get('packages')!=readiness.get('sheet_metal_summary') or
            flat.get('batch')!=readiness.get('sheet_metal_batch_summary')):
            raise ValueError('Sheet-metal review summary disagrees with integrated readiness')
    elif readiness.get('sheet_metal_summary'):
        raise ValueError('Missing linked sheet-metal review payload')
    if {p.name for p in folder.iterdir()}!=declared|{'evidence_manifest.json'}:
        raise ValueError('Extra or missing evidence files')
    return manifest


def main()->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('current',type=Path)
    parser.add_argument('destination',type=Path)
    parser.add_argument('--previous',type=Path)
    parser.add_argument('--drawings',type=Path)
    parser.add_argument('--previous-drawings',type=Path)
    parser.add_argument('--measured',type=Path)
    parser.add_argument('--previous-measurements',type=Path)
    parser.add_argument('--flat-package',type=Path,action='append',default=[])
    parser.add_argument('--flat-batch',type=Path)
    parser.add_argument('--minimum-bend-center-spacing-mm',type=float,default=0.0)
    parser.add_argument('--verify-only',action='store_true')
    a=parser.parse_args()
    try:
        if a.verify_only:
            result=verify_bundle(a.destination)
        else:
            result=build_bundle(a.current,a.destination,previous=a.previous,
                measured=a.measured,drawings=a.drawings,previous_drawings=a.previous_drawings,
                previous_measurements=a.previous_measurements,
                sheet_metal_packages=tuple(a.flat_package),
                minimum_bend_center_spacing_mm=a.minimum_bend_center_spacing_mm,
                sheet_metal_batch=a.flat_batch)
        print(json.dumps({'status':result['status'],'release_approved':False,
                          'open_action_count':result['open_action_count'],
                          'report_files':len(result['files'])},indent=2))
        return 0
    except (OSError,ValueError,TypeError,KeyError,UnicodeError) as exc:
        print(f'FAIL: {exc}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())

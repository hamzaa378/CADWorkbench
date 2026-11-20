#!/usr/bin/env python3
"""Compare verified CAD revisions and identify evidence invalidated by changes.

An unchanged hole identity/revision/dimension remains a reference match, NOT
approval to reuse old metrology. Changes force remeasurement/evidence review.
No signed handoff or previously entered measurement is modified.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys
from compare_manufacturing_handoffs import compare
from assess_change_control import assess
from verify_manufacturing_handoff import read_csv,verify
from review_measured_holes import review as review_measurements
from review_manufacturing_handoff import safe_output_path
from drawing_revision_diff import compare_drawings

HOLE_FIELDS={'body_id','geometry_token','hole_number','x_mm','y_mm','diameter_mm',
             'z_min_mm','z_max_mm','through_envelope'}


def _holes(folder:Path,manifest:dict) -> dict:
    if manifest['schema_version']!=2:return {}
    data={}
    for row in read_csv(folder,'axial_hole_locations.csv',HOLE_FIELDS):
        key=(row['body_id'],row['hole_number'])
        if key in data:raise ValueError('Duplicate engineering hole identity')
        data[key]=row
    return data


def impact(previous:Path,current:Path,*,previous_drawings:Path|None=None,
           current_drawings:Path|None=None,previous_measurements:Path|None=None) -> dict:
    previous,current=Path(previous),Path(current)
    old_manifest,new_manifest=verify(previous),verify(current)
    change=compare(previous,current)
    assessment=assess(previous,current)
    old,new=_holes(previous,old_manifest),_holes(current,new_manifest)
    holes=[]
    for body,number in sorted(set(old)|set(new)):
        key=(body,number)
        if key not in old:status='ADDED'
        elif key not in new:status='REMOVED'
        elif any(old[key][f]!=new[key][f] for f in HOLE_FIELDS-{'body_id','hole_number'}):
            status='CHANGED'
        else:status='REFERENCE_MATCH'
        changed_fields=sorted(f for f in HOLE_FIELDS-{'body_id','hole_number'}
                              if old.get(key,{}).get(f)!=new.get(key,{}).get(f))
        holes.append({'body_id':body,'hole_number':number,'status':status,
                      'changed_fields':changed_fields,
                      'fresh_inspection_required':status!='REFERENCE_MATCH',
                      'previous_geometry_token':old.get(key,{}).get('geometry_token'),
                      'current_geometry_token':new.get(key,{}).get('geometry_token')})
    prior_measurement_summary=None
    if previous_measurements is not None:
        # Must first establish that earlier measurements were genuinely joined to
        # the PREVIOUS signed CAD revision; do not accept user-supplied old labels.
        previous_review=review_measurements(previous,previous_measurements)
        prior_measurement_summary={
            'previous_measurement_sha256':previous_review['independent_measurement_sha256'],
            'prior_holes_checked':previous_review['checked_holes'],
            'automatically_carried_forward':0,
            'disposition':'PRIOR_RESULTS_REMAIN_HISTORICAL; REVALIDATE_ALL_CURRENT_EVIDENCE',
        }
    if (previous_drawings is None)!=(current_drawings is None):
        raise ValueError('Supply both previous and current drawing exports, or neither')
    drawing_delta=(compare_drawings(previous_drawings,current_drawings)
                   if previous_drawings is not None else None)
    findings=[]
    for h in holes:
        if h['fresh_inspection_required']:
            findings.append({'area':'METROLOGY','code':'HOLE_'+h['status'],
                             'subject':f"{h['body_id']} / {h['hole_number']}",
                             'required_action':'Reestablish datum/feature identity and obtain revision-specific inspection evidence'})
    for entry in change['modified_occurrences']:
        findings.append({'area':'ASSEMBLY','code':'OCCURRENCE_CHANGED','subject':entry['body_id'],
                         'required_action':'Review mating structure, BOM and mechanical interference for changed occurrence fields'})
    for body in change['added_body_ids']+change['removed_body_ids']:
        findings.append({'area':'ASSEMBLY','code':'OCCURRENCE_ADDED_OR_REMOVED','subject':body,
                         'required_action':'Review assembly configuration, drawings and procurement'})
    if drawing_delta:
        for entry in drawing_delta['sheets']:
            if entry['review_required']:
                findings.append({'area':'DRAWINGS','code':entry['code'],'subject':entry['owner'],
                                 'required_action':'Check dimension/GD&T references and sheet validity for this CAD revision'})
    for trig in assessment['review_triggers']:
        findings.append({'area':'CHANGE_CONTROL','code':trig,'subject':'PROJECT',
                         'required_action':'Complete the evidence topics in required_evidence before engineering change approval'})
    findings.sort(key=lambda r:(r['area'],r['code'],r['subject']))
    return {'status':'UNISSUED_REVISION_IMPACT_REVIEW_REQUIRED','release_approved':False,
            'previous_manifest_sha256':hashlib.sha256((previous/'handoff_manifest.json').read_bytes()).hexdigest(),
            'current_manifest_sha256':hashlib.sha256((current/'handoff_manifest.json').read_bytes()).hexdigest(),
            'previous_schema':old_manifest['schema_version'],'current_schema':new_manifest['schema_version'],
            'source_changed':change['source_changed'],'step_changed':change['step_changed'],
            'bom_changed':change['bom_changed'],'review_triggers':assessment['review_triggers'],
            'required_evidence':assessment['required_evidence'],
            'body_changes':{'added':change['added_body_ids'],'removed':change['removed_body_ids'],
                            'modified':change['modified_occurrences']},
            'hole_changes':holes,
            'fresh_inspection_count':sum(x['fresh_inspection_required'] for x in holes),
            'drawing_changes':drawing_delta,
            'prior_measurement_evidence':prior_measurement_summary,
            'actions':findings,'open_action_count':len(findings),
            'caution':'Reference matches and signed input hashes do not prove CAD geometry equivalence, measurement traceability or fabrication approval.'}


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('previous',type=Path)
    parser.add_argument('current',type=Path)
    parser.add_argument('--previous-drawings',type=Path)
    parser.add_argument('--current-drawings',type=Path)
    parser.add_argument('--previous-measurements',type=Path)
    parser.add_argument('--json',type=Path)
    args=parser.parse_args()
    try:
        if args.json:
            out=safe_output_path(safe_output_path(args.json,args.previous),args.current)
            for folder in (args.previous_drawings,args.current_drawings):
                if folder:out=safe_output_path(out,folder)
            if out.exists() or not out.parent.is_dir():raise ValueError('Output must be a new external file')
        result=impact(args.previous,args.current,previous_drawings=args.previous_drawings,
            current_drawings=args.current_drawings,previous_measurements=args.previous_measurements)
        data=json.dumps(result,indent=2,ensure_ascii=False)+'\n'
        if args.json:out.open('x',encoding='utf-8').write(data)
        print(data,end='');return 0
    except (OSError,ValueError,TypeError,KeyError,UnicodeError,StopIteration) as exc:
        print(f'FAIL: {exc}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())

#!/usr/bin/env python3
"""Aggregate verified UNISSUED handoff, metrology and drawing evidence for review.

Does NOT approve or issue engineering drawings or manufacturing packages.
All source artifacts are read-only; JSON/CSV reports must be external and new.
"""
from __future__ import annotations
import argparse
import csv
import io
import json
from pathlib import Path
import sys
from review_manufacturing_handoff import review as review_handoff, safe_cell, safe_output_path
from review_measured_holes import review as review_inspection
from reconcile_drawing_handoff import reconcile


def evaluate(handoff: Path, *, measured_csv: Path | None = None,
             drawing_folder: Path | None = None) -> dict:
    baseline = review_handoff(handoff)  # verifies signed manifest and all source CSVs
    actions = list(baseline['actions'])
    def add(area: str, code: str, subject: str, text: str) -> None:
        actions.append(dict(area=area, severity='REVIEW', code=code,
                            subject=subject, required_action=text))
    inspection = None
    drawing = None
    if measured_csv is None:
        add('METROLOGY', 'INDEPENDENT_MEASUREMENTS_NOT_SUPPLIED', 'HOLE_PATTERN',
            'Complete the independent inspection worksheet and verify against this exact model revision')
    else:
        inspection = review_inspection(handoff, measured_csv)
        for h in inspection['holes']:
            if not h['within_all_entered_limits']:
                add('METROLOGY', 'MEASURED_LIMIT_NOT_MET',
                    f"{h['body_id']} / {h['hole_number']}",
                    'Investigate the entered size, position and mating-pin clearance against approved specifications')
        add('METROLOGY', 'MEASUREMENT_EVIDENCE_SIGNOFF_REQUIRED', 'HOLE_PATTERN',
            'Verify measurement setup, datum alignment, uncertainty and sign the independent inspection results')
    if drawing_folder is None:
        add('DRAWINGS', 'UNISSUED_DRAWINGS_NOT_SUPPLIED', 'DRAWING_BATCH',
            'Export current, unissued drawings and review sheet and annotation revisions')
    else:
        drawing = reconcile(handoff, drawing_folder)
        for issue in drawing['issues']:
            add('DRAWINGS', issue['code'], issue['owner'] or 'MISSING_OWNER',
                'Resolve this drawing-to-CAD source discrepancy before issuing any controlled revision')
        add('DRAWINGS', 'DRAWING_DIMENSION_GDT_REVIEW_REQUIRED', 'DRAWING_BATCH',
            'Review source geometry, dimensions, GD&T, title block and sheet issuance independently')
    add('ENGINEERING', 'MANUFACTURING_RELEASE_REVIEW_REQUIRED', 'ENTIRE_PROJECT',
        'Complete authorized design, manufacturing, inspection, safety and drawing release reviews')
    counts = {area: sum(x['area'] == area for x in actions) for area in
              sorted({x['area'] for x in actions})}
    return dict(status='UNISSUED_INTEGRATED_ENGINEERING_REVIEW_REQUIRED',
                release_approved=False, source_handoff=baseline['source_package'],
                handoff_timestamp=baseline['manifest_timestamp'],
                counts=counts, open_action_count=len(actions),
                inspection_summary=None if inspection is None else {
                    k: inspection[k] for k in ('checked_holes', 'within_entered_limits',
                        'all_within_entered_limits', 'source_handoff_manifest_sha256',
                        'independent_measurement_sha256')},
                drawing_summary=None if drawing is None else {
                    k: drawing[k] for k in ('sheet_count', 'cad_body_count',
                        'review_issue_count', 'source_handoff_manifest_sha256',
                        'drawing_index_sha256')},
                actions=actions)


def render_csv(data: dict) -> str:
    fields = ('area', 'severity', 'code', 'subject', 'required_action')
    stream=io.StringIO(newline='')
    writer=csv.writer(stream,lineterminator='\r\n')
    writer.writerow(fields)
    for item in data['actions']:
        writer.writerow([safe_cell(item[field]) for field in fields])
    return stream.getvalue()


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('handoff',type=Path)
    parser.add_argument('--measured-csv',type=Path)
    parser.add_argument('--drawings',type=Path)
    parser.add_argument('--json',type=Path)
    parser.add_argument('--csv',type=Path)
    args=parser.parse_args()
    try:
        outputs = []
        for path,kind in ((args.json,'json'),(args.csv,'csv')):
            if path is None:continue
            p=safe_output_path(path,args.handoff)
            if args.drawings:p=safe_output_path(p,args.drawings)
            if p.exists() or not p.parent.is_dir():
                raise ValueError('Output already exists or output parent directory is missing')
            outputs.append((p,kind))
        if len({path for path,_ in outputs})!=len(outputs):
            raise ValueError('JSON and CSV outputs must be distinct')
        data=evaluate(args.handoff,measured_csv=args.measured_csv,drawing_folder=args.drawings)
        content=[(path,(json.dumps(data,indent=2,ensure_ascii=False)+'\n') if kind=='json' else render_csv(data))
                 for path,kind in outputs]
        created=[]
        try:
            for path,body in content:
                with path.open('x',encoding='utf-8',newline='') as stream:
                    created.append(path)
                    stream.write(body)
        except OSError:
            for path in created:
                try:path.unlink()
                except OSError:pass
            raise
        print(json.dumps({k:data[k] for k in ('status','release_approved','open_action_count','counts')},indent=2))
        return 0
    except (OSError,ValueError,TypeError,KeyError,UnicodeError) as exc:
        print(f'FAIL: {exc}',file=sys.stderr)
        return 1


if __name__=='__main__':raise SystemExit(main())

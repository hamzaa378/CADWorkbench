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
from engineering_revision_impact import impact as evaluate_revision_impact
from verify_linked_flat_package import verify as verify_flat_package
from verify_linked_flat_batch import verify as verify_flat_batch
from verify_manufacturing_handoff import read_csv, verify as verify_native_handoff


def evaluate(handoff: Path, *, measured_csv: Path | None = None,
             drawing_folder: Path | None = None,
             previous_handoff: Path | None = None,
             previous_drawings: Path | None = None,
             previous_measurements: Path | None = None,
             sheet_metal_packages: tuple[Path, ...] = (),
             minimum_bend_center_spacing_mm: float = 0.0,
             sheet_metal_batch: Path | None = None) -> dict:
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
        if drawing['bodies_without_saved_sheet_in_batch']:
            add('DRAWINGS', 'SHEET_COVERAGE_CONFIRMATION_REQUIRED', 'DRAWING_BATCH',
                'Confirm whether each CAD body without a saved sheet is represented by an approved assembly or multi-body drawing')
        add('DRAWINGS', 'DRAWING_DIMENSION_GDT_REVIEW_REQUIRED', 'DRAWING_BATCH',
            'Review source geometry, dimensions, GD&T, title block and sheet issuance independently')
    if previous_handoff is None and (previous_drawings is not None or previous_measurements is not None):
        raise ValueError('Previous drawing or inspection evidence requires a previous signed CAD handoff')
    revision=None
    if previous_handoff is not None:
        revision=evaluate_revision_impact(previous_handoff,handoff,
            previous_drawings=previous_drawings,current_drawings=drawing_folder if previous_drawings else None,
            previous_measurements=previous_measurements)
        for finding in revision['actions']:
            add(finding['area'],finding['code'],finding['subject'],finding['required_action'])
        add('CHANGE_CONTROL','PREVIOUS_EVIDENCE_IS_NOT_CURRENT_APPROVAL','PROJECT',
            'Verify every required change-control evidence topic; historical inspection is never automatically carried forward')
    batch_report=(verify_flat_batch(sheet_metal_batch,
                  minimum_center_spacing_mm=minimum_bend_center_spacing_mm)
                  if sheet_metal_batch is not None else None)
    batch_summaries=tuple(batch_report['parts']) if batch_report else ()
    flat_summaries=[]
    flat_ids=set()
    # Native handoff has already passed its own integrity checks. Resolve each
    # folded source against the SAME saved assembly revision before trusting a
    # separately exported sheet-metal package in an integrated review.
    manifest=verify_native_handoff(handoff)
    occurrence_ids={row['Body ID'] for row in read_csv(handoff,'occurrence_parts.csv',{'Body ID'})}
    coverage={}
    if manifest['schema_version']==2:
        coverage={r['body_id']:r['geometry_token'] for r in
                  read_csv(handoff,'machining_coverage.csv',{'body_id','geometry_token'})}
    package_reports=[verify_flat_package(package,
            minimum_center_spacing_mm=minimum_bend_center_spacing_mm)
            for package in sheet_metal_packages]
    package_reports.extend(batch_summaries)
    for report in package_reports:
        if report['source_body_id'] not in occurrence_ids:
            raise ValueError('Linked flat package folded source is not in current CAD handoff')
        saved_revision=coverage.get(report['source_body_id'])
        if saved_revision and saved_revision!=report['source_geometry_token']:
            raise ValueError('Linked flat package source revision differs from current CAD handoff')
        report['same_handoff_geometry_revision_verified']=bool(saved_revision)
        report['linked_flat_body_in_handoff']=report['flat_body_id'] in occurrence_ids
        identities=(report['source_body_id'],report['flat_body_id'])
        if identities in flat_ids:
            raise ValueError('Duplicate native linked sheet-metal production package')
        flat_ids.add(identities)
        flat_summaries.append(report)
        add('SHEET_METAL','FLAT_DEVELOPMENT_PHYSICAL_REVIEW_REQUIRED',report['flat_body_id'],
            'Review folded source, blank forming, bend relief, tooling, springback and independent flat geometry measurements')
        if not saved_revision:
            add('SHEET_METAL','FOLDED_REVISION_CROSSCHECK_UNAVAILABLE',report['source_body_id'],
                'Establish an authoritative source geometry revision for this folded sheet')
        if not report['linked_flat_body_in_handoff']:
            add('SHEET_METAL','FLAT_BODY_NOT_IN_HANDOFF',report['flat_body_id'],
                'Confirm that the flat blank corresponds to the current released assembly configuration')
        if report['shop_spacing_review_required']:
            add('SHEET_METAL','BEND_CENTER_SPACING_LIMIT_EXCEEDED',report['flat_body_id'],
                'Review underspaced bend pairs against supplied shop/tooling capabilities')
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
                revision_impact_summary=None if revision is None else {
                    'previous_manifest_sha256':revision['previous_manifest_sha256'],
                    'current_manifest_sha256':revision['current_manifest_sha256'],
                    'fresh_inspection_count':revision['fresh_inspection_count'],
                    'open_action_count':revision['open_action_count'],
                    'review_triggers':revision['review_triggers'],
                    'prior_measurement_evidence':revision['prior_measurement_evidence']},
                sheet_metal_summary=flat_summaries,
                sheet_metal_batch_summary=None if batch_report is None else {
                    k:batch_report[k] for k in ('status','part_count','stock_summary','batch_manifest_sha256')},
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
    parser.add_argument('--previous-handoff',type=Path)
    parser.add_argument('--previous-drawings',type=Path)
    parser.add_argument('--previous-measurements',type=Path)
    parser.add_argument('--flat-package',type=Path,action='append',default=[])
    parser.add_argument('--flat-batch',type=Path)
    parser.add_argument('--minimum-bend-center-spacing-mm',type=float,default=0.0)
    parser.add_argument('--json',type=Path)
    parser.add_argument('--csv',type=Path)
    args=parser.parse_args()
    try:
        outputs = []
        for path,kind in ((args.json,'json'),(args.csv,'csv')):
            if path is None:continue
            p=safe_output_path(path,args.handoff)
            if args.drawings:p=safe_output_path(p,args.drawings)
            if args.previous_handoff:p=safe_output_path(p,args.previous_handoff)
            if args.previous_drawings:p=safe_output_path(p,args.previous_drawings)
            for folder in args.flat_package:p=safe_output_path(p,folder)
            if args.flat_batch:p=safe_output_path(p,args.flat_batch)
            if p.exists() or not p.parent.is_dir():
                raise ValueError('Output already exists or output parent directory is missing')
            outputs.append((p,kind))
        if len({path for path,_ in outputs})!=len(outputs):
            raise ValueError('JSON and CSV outputs must be distinct')
        data=evaluate(args.handoff,measured_csv=args.measured_csv,drawing_folder=args.drawings,
            previous_handoff=args.previous_handoff,previous_drawings=args.previous_drawings,
            previous_measurements=args.previous_measurements,
            sheet_metal_packages=tuple(args.flat_package),
            minimum_bend_center_spacing_mm=args.minimum_bend_center_spacing_mm,
            sheet_metal_batch=args.flat_batch)
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

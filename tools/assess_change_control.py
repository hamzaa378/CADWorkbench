#!/usr/bin/env python3
"""Read-only engineering change impact and evidence planning for two UNISSUED CADWorkbench handoffs.

This tool never issues or certifies designs. It evaluates evidence needed *before*
independent change approval. Run after both handoffs pass the strict verifier.
"""
from __future__ import annotations
import argparse
import csv
import json
from pathlib import Path
import sys

from compare_manufacturing_handoffs import compare
from verify_manufacturing_handoff import read_csv

REQUIRED_BY_TRIGGER = {
    "DESIGN_GEOMETRY_REVIEW": ("model_regeneration", "assembly_interference", "drawing_update", "dimensional_inspection"),
    "PROCUREMENT_AND_BOM_REVIEW": ("bom_reconciliation", "material_specification", "supplier_review"),
    "INSPECTION_PLAN_REVIEW": ("inspection_plan_review", "drawing_update"),
    "OPEN_DOCUMENT_AUDIT_WARNINGS": ("audit_findings_review",),
    "MACHINING_AXIAL_REVIEW": ("hole_location_inspection", "tool_access_review", "fixture_exit_review"),
}


def assess(previous: Path, current: Path) -> dict:
    change = compare(previous, current)
    outstanding = []
    for entry in read_csv(current, 'feature_inspection_requirements.csv',
                          {'body_id', 'feature_id', 'review_status'}):
        if entry['review_status'] == 'REVIEW_REQUIRED':
            outstanding.append({'body_id': entry['body_id'], 'feature_id': entry['feature_id']})
    # Even when only CSV metadata changes, send all modified occurrence fields
    # to engineering review; schema-specific distinctions are not approvals.
    triggers = set(change['review_triggers'])
    if change['added_body_ids'] or change['removed_body_ids'] or change['modified_occurrences']:
        triggers.add('ASSEMBLY_CONFIGURATION_REVIEW')
    if change.get('machining_review_required'):
        triggers.add('MACHINING_AXIAL_REVIEW')
    if change['source_changed'] != change['step_changed']:
        triggers.add('CAD_STEP_DIVERGENCE_REVIEW')
    evidence = set()
    for trigger in triggers:
        evidence.update(REQUIRED_BY_TRIGGER.get(trigger, ()))
    if 'ASSEMBLY_CONFIGURATION_REVIEW' in triggers:
        evidence.update(('assembly_structure_review', 'bom_reconciliation'))
    if 'CAD_STEP_DIVERGENCE_REVIEW' in triggers:
        evidence.update(('cad_step_equivalence', 'model_regeneration'))
    # Explicit conservative disposition: the output is a worklist, not a gate pass.
    return {
        'status': 'UNISSUED_ENGINEERING_REVIEW_REQUIRED',
        'design_approval': False,
        'previous': str(Path(previous).resolve()),
        'current': str(Path(current).resolve()),
        'review_triggers': sorted(triggers),
        'required_evidence': [{'topic': topic, 'state': 'NOT_REVIEWED'} for topic in sorted(evidence)],
        'changed_occurrence_count': len(change['modified_occurrences']) +
                                    len(change['added_body_ids']) + len(change['removed_body_ids']),
        'inspection_items_requiring_review': outstanding,
        'procurement_exceptions': change['new_procurement_exceptions'],
        'audit_warnings': change['new_audit_warnings'],
        'caution': 'Checksums verify files, not CAD/STEP equivalence or manufacturing safety.',
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('previous', type=Path)
    parser.add_argument('current', type=Path)
    parser.add_argument('--json', type=Path, help='Write a new, separate review worklist; never overwrite')
    args = parser.parse_args()
    try:
        result = assess(args.previous, args.current)
        output = json.dumps(result, indent=2, ensure_ascii=False) + '\n'
        if args.json:
            if args.json.resolve().is_relative_to(args.previous.resolve()) or args.json.resolve().is_relative_to(args.current.resolve()):
                raise ValueError('Cannot write review evidence inside an immutable handoff')
            with args.json.open('x', encoding='utf-8') as fh:
                fh.write(output)
        print(output, end='')
    except (OSError, ValueError, KeyError, TypeError, StopIteration, UnicodeError) as exc:
        print(f'FAIL: {exc}', file=sys.stderr)
        return 1
    return 0

if __name__ == '__main__':
    raise SystemExit(main())

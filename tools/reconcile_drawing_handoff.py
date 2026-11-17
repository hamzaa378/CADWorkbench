#!/usr/bin/env python3
"""Read-only reconciliation of separately exported UNISSUED drawings and CAD handoff.

A drawing's source token may be composite for multiple referenced bodies; in
that case token disagreement is unresolved, not asserted to be stale. Neither
matching hashes nor revision tokens constitute an engineering approval.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys
from verify_manufacturing_handoff import verify as verify_handoff, read_csv
from verify_unissued_drawings import verify as verify_drawings
from review_manufacturing_handoff import safe_output_path


def reconcile(handoff: Path, drawings: Path) -> dict:
    handoff, drawings = Path(handoff), Path(drawings)
    manifest = verify_handoff(handoff)
    count = verify_drawings(drawings)
    with (drawings / "drawing_index.csv").open(encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))
    bodies = read_csv(handoff, "occurrence_parts.csv", {"Body ID"})
    all_ids = {row["Body ID"] for row in bodies}
    revision = {}
    if manifest["schema_version"] == 2:
        for row in read_csv(handoff, "machining_coverage.csv", {"body_id", "geometry_token"}):
            revision[row["body_id"]] = row["geometry_token"]
    findings = []
    seen_owners = set()
    seen_numbers = set()
    for row in rows:
        owner, number = row["sheet_owner_id"], row["drawing_number"]
        if not owner or owner not in all_ids:
            findings.append({"code": "DRAWING_OWNER_NOT_IN_CAD_HANDOFF", "owner": owner,
                             "drawing_number": number})
        if owner in seen_owners:
            findings.append({"code": "DUPLICATE_SHEET_OWNER", "owner": owner,
                             "drawing_number": number})
        if not number.strip() or number in seen_numbers:
            findings.append({"code": "MISSING_OR_DUPLICATE_DRAWING_NUMBER", "owner": owner,
                             "drawing_number": number})
        seen_owners.add(owner)
        seen_numbers.add(number)
        current = row["current_geometry_token"]
        if not current:
            findings.append({"code": "DRAWING_SOURCE_TOKEN_MISSING", "owner": owner,
                             "drawing_number": number})
        elif owner in revision and current != revision[owner]:
            findings.append({"code": "TOKEN_DIFFERENCE_OR_COMPOSITE_SHEET_REVIEW", "owner": owner,
                             "drawing_number": number,
                             "explanation": "Drawing token differs from owner-only CAD token; composite sources may explain it"})
    # Neither additional bodies without drawings nor matching token prove a
    # missing drawing error or a released print. Report uncovered bodies only.
    return {
        "status": "UNISSUED_CROSS_PACKAGE_DRAWING_REVIEW_REQUIRED",
        "release_approved": False,
        "source_handoff_manifest_sha256": hashlib.sha256((handoff/"handoff_manifest.json").read_bytes()).hexdigest(),
        "drawing_index_sha256": hashlib.sha256((drawings/"drawing_index.csv").read_bytes()).hexdigest(),
        "sheet_count": count,
        "cad_body_count": len(all_ids),
        "bodies_without_saved_sheet_in_batch": sorted(all_ids - seen_owners),
        "review_issue_count": len(findings),
        "issues": findings,
        "limitations": [
            "This check does not inspect geometry or determine dimension or GD&T correctness",
            "A drawing source token may represent multiple bodies; mismatches require model review",
            "Unissued drawing previews and handoffs are not approved for fabrication",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("handoff", type=Path)
    parser.add_argument("drawing_folder", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    try:
        if args.json:
            out = safe_output_path(args.json, args.handoff)
            out = safe_output_path(out, args.drawing_folder)
            if out.exists() or not out.parent.is_dir():
                raise ValueError("Report path already exists or parent does not exist")
        report = reconcile(args.handoff, args.drawing_folder)
        if args.json:
            with out.open("x", encoding="utf-8") as f:
                json.dump(report, f, indent=2, ensure_ascii=False)
                f.write("\n")
        print(json.dumps({k: report[k] for k in ("status", "release_approved", "sheet_count",
                                                  "review_issue_count")}, indent=2))
        return 0
    except (ValueError, TypeError, KeyError, OSError, UnicodeError) as e:
        print(f"FAIL: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

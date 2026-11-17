#!/usr/bin/env python3
"""Compare two independently verified CADWorkbench engineering handoffs.

Reports occurrence-level changes by stable body ID. No design approval,
geometry identity, or automatic revision certification is implied.
"""
from __future__ import annotations
import argparse
from collections import Counter
import csv
import json
from pathlib import Path
import sys

from verify_manufacturing_handoff import verify, read_csv


def occurrence_index(folder: Path) -> dict[str, dict[str, str]]:
    with (folder / "occurrence_parts.csv").open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or "Body ID" not in reader.fieldnames:
            raise ValueError("Occurrence CSV must contain Body ID")
        result = {}
        for row in reader:
            if None in row or not row.get("Body ID"):
                raise ValueError("Malformed or empty occurrence body ID")
            body_id = row["Body ID"]
            if body_id in result:
                raise ValueError("Duplicate body IDs make revision comparison ambiguous")
            # Item numbers are view order, not design identity.
            result[body_id] = {k: v for k, v in row.items() if k != "Item"}
    return result


def register_delta(previous: Path, current: Path, filename: str,
                   fields: tuple[str, ...]) -> tuple[list[dict], list[dict]]:
    """Change counts by stable engineering identity, not volatile BOM row order."""
    def inventory(folder: Path) -> Counter:
        rows = read_csv(folder, filename, set(fields))
        return Counter(tuple(row[field] for field in fields) for row in rows)
    old, new = inventory(previous), inventory(current)

    def emit(delta: Counter) -> list[dict]:
        return [{**dict(zip(fields, identity)), "count": count}
                for identity, count in sorted(delta.items()) if count > 0]
    return emit(new - old), emit(old - new)


def compare(previous: Path, current: Path) -> dict:
    old_manifest, new_manifest = verify(previous), verify(current)
    before, after = occurrence_index(previous), occurrence_index(current)
    procurement_added, procurement_resolved = register_delta(
        previous, current, "procurement_exceptions.csv", ("part_number", "exception_code", "next_action"))
    inspection_added, inspection_removed = register_delta(
        previous, current, "feature_inspection_requirements.csv",
        ("body_id", "feature_id", "category", "requirement", "review_status"))
    stale_inspection_count = sum(
        row["review_status"] == "REVIEW_REQUIRED"
        for row in read_csv(current, "feature_inspection_requirements.csv", {"review_status"}))
    added = sorted(set(after) - set(before))
    removed = sorted(set(before) - set(after))
    changed = []
    for body in sorted(set(before) & set(after)):
        keys = sorted(set(before[body]) | set(after[body]))
        differences = [name for name in keys if before[body].get(name) != after[body].get(name)]
        if differences:
            changed.append({"body_id": body, "fields": differences,
                            "before": {key: before[body].get(key) for key in differences},
                            "after": {key: after[body].get(key) for key in differences}})
    source_changed = source_digest(old_manifest) != source_digest(new_manifest)
    step_changed = file_digest(old_manifest, "assembly.step") != file_digest(new_manifest, "assembly.step")
    bom_changed = file_digest(old_manifest, "bill_of_materials.csv") != file_digest(new_manifest, "bill_of_materials.csv")
    geometry_schedule_changed = (file_digest(old_manifest, "body_schedule.csv") !=
                                 file_digest(new_manifest, "body_schedule.csv"))
    machining_added, machining_removed = ([], [])
    if old_manifest["schema_version"] == 2 and new_manifest["schema_version"] == 2:
        machining_added, machining_removed = register_delta(
            previous, current, "axial_hole_locations.csv",
            ("body_id", "hole_number", "x_mm", "y_mm", "diameter_mm", "z_min_mm", "z_max_mm", "through_envelope"))
    old_hole_digest = next((row["sha256"] for row in old_manifest["files"]
                            if row["path"] == "axial_hole_locations.csv"), None)
    new_hole_digest = next((row["sha256"] for row in new_manifest["files"]
                            if row["path"] == "axial_hole_locations.csv"), None)
    old_coverage_digest = next((row["sha256"] for row in old_manifest["files"]
                                if row["path"] == "machining_coverage.csv"), None)
    new_coverage_digest = next((row["sha256"] for row in new_manifest["files"]
                                if row["path"] == "machining_coverage.csv"), None)
    machining_changed = (old_hole_digest != new_hole_digest or
                         old_coverage_digest != new_coverage_digest)
    review_triggers = []
    if machining_changed:
        review_triggers.append("MACHINING_AXIAL_REVIEW")
    if source_changed or step_changed or geometry_schedule_changed:
        review_triggers.append("DESIGN_GEOMETRY_REVIEW")
    if bom_changed or procurement_added or procurement_resolved:
        review_triggers.append("PROCUREMENT_AND_BOM_REVIEW")
    if inspection_added or inspection_removed or stale_inspection_count:
        review_triggers.append("INSPECTION_PLAN_REVIEW")
    if new_manifest["audit_warnings"]:
        review_triggers.append("OPEN_DOCUMENT_AUDIT_WARNINGS")
    return {
        "status": "UNISSUED_CHANGE_REVIEW_REQUIRED",
        "previous": str(Path(previous).resolve()),
        "current": str(Path(current).resolve()),
        "added_body_ids": added,
        "removed_body_ids": removed,
        "modified_occurrences": changed,
        "step_changed": step_changed,
        "source_changed": source_changed,
        "bom_changed": bom_changed,
        "geometry_schedule_changed": geometry_schedule_changed,
        "procurement_issues_added": procurement_added,
        "procurement_issues_resolved": procurement_resolved,
        "inspection_requirements_added_or_changed": inspection_added,
        "inspection_requirements_removed_or_changed": inspection_removed,
        "inspection_geometry_review_required": stale_inspection_count,
        "review_triggers": review_triggers,
        "machining_review_changed": machining_changed,
        "axial_holes_added_or_changed": machining_added,
        "axial_holes_removed_or_changed": machining_removed,
        "machining_review_required": new_manifest.get("machining_review_required"),
        "old_procurement_exceptions": old_manifest["procurement_exceptions"],
        "new_procurement_exceptions": new_manifest["procurement_exceptions"],
        "old_audit_warnings": old_manifest["audit_warnings"],
        "new_audit_warnings": new_manifest["audit_warnings"],
    }


def file_digest(manifest: dict, name: str) -> str:
    return next(row["sha256"] for row in manifest["files"] if row["path"] == name)


def source_digest(manifest: dict) -> str:
    return next(row["sha256"] for row in manifest["files"] if row["path"].startswith("source_design."))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("previous", type=Path)
    parser.add_argument("current", type=Path)
    parser.add_argument("--json", type=Path, help="Optional JSON change review output")
    args = parser.parse_args()
    try:
        report = compare(args.previous, args.current)
        result = json.dumps(report, ensure_ascii=False, indent=2)
        if args.json:
            # Never overwrite an existing review without explicit manual action.
            with args.json.open("x", encoding="utf-8") as output:
                output.write(result + "\n")
        print(result)
    except (OSError, ValueError, KeyError, StopIteration, UnicodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

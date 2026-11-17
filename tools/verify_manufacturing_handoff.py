#!/usr/bin/env python3
"""Strict, read-only integrity verifier for CADWorkbench UNISSUED engineering handoffs.

No approval, authenticity, geometric validity or manufacturing suitability
is inferred from SHA-256 matching; this is a file-integrity check only.

Usage: python tools/verify_manufacturing_handoff.py <handoff-folder>
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import re
import sys
from pathlib import Path

HASH = re.compile(r"[a-f0-9]{64}\Z")
MANDATORY = {
    "assembly.step", "bill_of_materials.csv", "occurrence_parts.csv",
    "body_schedule.csv", "document_audit.csv", "procurement_exceptions.csv",
    "feature_inspection_requirements.csv", "READ_ME_BEFORE_FABRICATION.txt",
}
EXPECTED_KEYS = {
    "schema_version", "application", "status", "generated_utc", "body_count",
    "bom_lines", "audit_errors", "audit_warnings", "procurement_exceptions",
    "inspection_requirements", "files",
}
MACHINING_FILES = {"machining_coverage.csv", "axial_hole_locations.csv"}
MACHINING_KEYS = {"machining_holes", "machining_review_required"}
MACHINING_STATUSES = {
    "SURFACE_NOT_APPLICABLE", "NOT_EVALUATED", "PARTIAL_CYLINDERS_EXCLUDED",
    "INSPECTED_AXIAL_ONLY", "INSPECTED_NO_AXIAL_HOLES",
}
MAX_JSON_BYTES = 1024 * 1024


def read_csv(folder: Path, filename: str, required: set[str]) -> list[dict[str, str]]:
    """Reject incomplete or malformed CSV records before trusting table joins."""
    with (folder / filename).open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        fields = reader.fieldnames
        if not fields or len(fields) != len(set(fields)) or not required.issubset(fields):
            raise ValueError(f"Missing or duplicate CSV columns in {filename}")
        rows = list(reader)
    if any(None in row or any(value is None for value in row.values()) for row in rows):
        raise ValueError(f"Malformed CSV row in {filename}")
    return rows


def validate_engineering_links(folder: Path, manifest: dict) -> None:
    """Cross-check native CAD exports with procurement and inspection registers."""
    occurrences = read_csv(folder, "occurrence_parts.csv", {"Body ID", "Item", "Quantity"})
    schedule = read_csv(folder, "body_schedule.csv", {"Body ID"})
    bom = read_csv(folder, "bill_of_materials.csv", {"Item", "Part number", "Quantity"})
    issues = read_csv(folder, "procurement_exceptions.csv",
                      {"item_number", "part_number", "quantity", "exception_code"})
    inspections = read_csv(folder, "feature_inspection_requirements.csv",
                           {"body_id", "feature_id", "category", "review_status", "requirement"})
    body_ids = [row["Body ID"] for row in occurrences]
    schedule_ids = [row["Body ID"] for row in schedule]
    if (not all(body_ids) or len(body_ids) != len(set(body_ids)) or
            not all(schedule_ids) or len(schedule_ids) != len(set(schedule_ids)) or
            set(body_ids) != set(schedule_ids)):
        raise ValueError("Duplicate, missing or inconsistent body IDs")
    if len(occurrences) != manifest["body_count"] or len(schedule) != manifest["body_count"]:
        raise ValueError("Body count differs between manifest, occurrences and body schedule")
    if any(row["Quantity"] != "1" for row in occurrences):
        raise ValueError("An occurrence must represent exactly one physical body")
    try:
        quantities = [int(row["Quantity"]) for row in bom]
        bom_items = {int(row["Item"]): row for row in bom}
    except (ValueError, TypeError) as exc:
        raise ValueError("Invalid BOM item or quantity") from exc
    if (any(q <= 0 for q in quantities) or sum(quantities) != manifest["body_count"] or
            len(bom_items) != len(bom) or not all(key > 0 for key in bom_items)):
        raise ValueError("BOM quantities or item identities are inconsistent with occurrences")
    # BOM grouping must account for each physical occurrence by its Item.
    # Total quantity matching alone would allow a substituted or missing part
    # to go unnoticed when an unrelated line happened to compensate.
    from collections import Counter
    try:
        observed_items = Counter(int(row["Item"]) for row in occurrences)
    except (ValueError, TypeError) as exc:
        raise ValueError("Invalid occurrence BOM item") from exc
    if (any(item <= 0 for item in observed_items) or
            set(observed_items) != set(bom_items) or
            any(observed_items[item] != int(bom_items[item]["Quantity"])
                for item in bom_items)):
        raise ValueError("Occurrence item groups do not match BOM quantities")
    for row in issues:
        try:
            item = int(row["item_number"])
            quantity = int(row["quantity"])
        except (ValueError, TypeError) as exc:
            raise ValueError("Invalid procurement exception item or quantity") from exc
        source = bom_items.get(item)
        if (source is None or quantity != int(source["Quantity"]) or
                row["part_number"] != source["Part number"] or
                not row["exception_code"].strip()):
            raise ValueError("Procurement exception does not match a BOM item")
    for row in inspections:
        if row["body_id"] not in set(body_ids) or not row["requirement"].strip():
            raise ValueError("Inspection requirement references a missing body or is blank")
        if row["review_status"] not in {"SOURCE_GEOMETRY_CURRENT", "REVIEW_REQUIRED"}:
            raise ValueError("Invalid inspection geometry review status")


def validate_machining_review(folder: Path, manifest: dict) -> None:
    """Verify 100% body coverage, identity, revisions, and conservative hole counts.

    A matching hole table is NEVER evidence of feasible drilling or manufacture.
    """
    coverage = read_csv(folder, "machining_coverage.csv",
                        {"body_id", "geometry_token", "status", "hole_count",
                         "excluded_cylinders", "reason"})
    holes = read_csv(folder, "axial_hole_locations.csv",
                     {"body_id", "geometry_token", "hole_number", "x_mm", "y_mm",
                      "diameter_mm", "z_min_mm", "z_max_mm", "through_envelope"})
    occurrences = read_csv(folder, "occurrence_parts.csv", {"Body ID"})
    known_ids = {row["Body ID"] for row in occurrences}
    coverage_ids = {row["body_id"] for row in coverage}
    if (len(coverage) != manifest["body_count"] or len(coverage_ids) != len(coverage)
            or coverage_ids != known_ids):
        raise ValueError("Machining coverage does not match every saved body")
    by_id = {}
    total = 0
    review = 0
    for row in coverage:
        body, token, status = row["body_id"], row["geometry_token"], row["status"]
        if status not in MACHINING_STATUSES:
            raise ValueError("Invalid machining coverage state")
        try:
            count = int(row["hole_count"])
            excluded = int(row["excluded_cylinders"])
        except (TypeError, ValueError) as exc:
            raise ValueError("Invalid machining coverage count") from exc
        if (count < 0 or excluded < 0 or str(count) != row["hole_count"] or
                str(excluded) != row["excluded_cylinders"]):
            raise ValueError("Invalid machining coverage count")
        if status in {"SURFACE_NOT_APPLICABLE", "NOT_EVALUATED"} and (count or excluded):
            raise ValueError("Uninspected machining body claims inspected holes")
        if status == "INSPECTED_AXIAL_ONLY" and (not count or excluded or not token):
            raise ValueError("Inspected holes have invalid count, exclusions, or revision")
        if status == "INSPECTED_NO_AXIAL_HOLES" and (count or excluded or not token):
            raise ValueError("Inspected empty body has invalid machining counts")
        if status == "PARTIAL_CYLINDERS_EXCLUDED" and (not excluded or not token):
            raise ValueError("Partial machining review has no excluded cylinders or revision")
        if status in {"NOT_EVALUATED", "PARTIAL_CYLINDERS_EXCLUDED"}:
            review += 1
            if not row["reason"].strip():
                raise ValueError("Machining review item has no explanation")
        total += count
        by_id[body] = (token, count, status)
    if (manifest["machining_holes"] != total or
            manifest["machining_review_required"] != review or len(holes) != total):
        raise ValueError("Machining hole/review totals do not agree with manifest")
    used_numbers = set()
    counts = {body: 0 for body in by_id}
    for row in holes:
        body = row["body_id"]
        if body not in by_id or row["geometry_token"] != by_id[body][0] or not row["geometry_token"]:
            raise ValueError("Hole record references missing body or wrong geometry revision")
        try:
            num = int(row["hole_number"])
            measurements = [float(row[name]) for name in
                            ("x_mm", "y_mm", "diameter_mm", "z_min_mm", "z_max_mm")]
        except (TypeError, ValueError) as exc:
            raise ValueError("Malformed machining hole measurement") from exc
        from math import isfinite
        if (num <= 0 or not all(isfinite(v) and abs(v) < 1e6 for v in measurements)
                or not 0 < measurements[2] < 1e6 or
                measurements[4] <= measurements[3] or
                row["through_envelope"] not in {"YES", "NO"}):
            raise ValueError("Invalid hole dimension or classification")
        identity = (body, num)
        if identity in used_numbers:
            raise ValueError("Duplicate axial-hole identifier")
        used_numbers.add(identity)
        counts[body] += 1
    if any(counts[body] != by_id[body][1] for body in by_id):
        raise ValueError("Body machining hole counts are inconsistent")
    for body, (_, count, _) in by_id.items():
        if {num for ident, num in used_numbers if ident == body} != set(range(1, count + 1)):
            raise ValueError("Nonconsecutive machining hole numbers")


INSPECTION_TEMPLATE = "hole_position_inspection_TEMPLATE.csv"
INSPECTION_TEMPLATE_FIELDS = {
    "body_id", "geometry_token", "hole_number", "nominal_x_mm",
    "nominal_y_mm", "nominal_diameter_mm", "measured_datum_x_mm",
    "measured_datum_y_mm", "measured_projected_x_mm",
    "measured_projected_y_mm", "projected_height_mm", "actual_diameter_mm",
    "minimum_diameter_mm", "maximum_diameter_mm",
    "diametral_position_tolerance_mm", "material_condition",
    "maximum_pin_diameter_mm", "inspection_status",
}

def validate_inspection_template(folder: Path) -> None:
    """Cross-check every unmeasured design row against the signed CAD holes.

    A completed worksheet is external evidence, not something the CAD exporter
    can attest to. Reject measured or approved values inside the unissued handoff.
    """
    # Reject undisclosed columns (for example, a spoofed approval field) even
    # when the required blank metrology columns are present.
    with (folder / INSPECTION_TEMPLATE).open(encoding="utf-8-sig", newline="") as stream:
        declared = next(csv.reader(stream), [])
    if len(declared) != len(INSPECTION_TEMPLATE_FIELDS) or set(declared) != INSPECTION_TEMPLATE_FIELDS:
        raise ValueError("Inspection template contains an unexpected or missing column")
    rows = read_csv(folder, INSPECTION_TEMPLATE, INSPECTION_TEMPLATE_FIELDS)
    holes = read_csv(folder, "axial_hole_locations.csv", {
        "body_id", "geometry_token", "hole_number", "x_mm", "y_mm", "diameter_mm"})
    design = {(r["body_id"], r["hole_number"]): r for r in holes}
    if len(design) != len(holes) or len(rows) != len(holes):
        raise ValueError("Inspection template does not cover every CAD design hole")
    seen: set[tuple[str, str]] = set()
    blanks = INSPECTION_TEMPLATE_FIELDS - {
        "body_id", "geometry_token", "hole_number", "nominal_x_mm",
        "nominal_y_mm", "nominal_diameter_mm", "inspection_status"}
    for row in rows:
        key = (row["body_id"], row["hole_number"])
        if key in seen or key not in design:
            raise ValueError("Inspection template contains duplicate or unknown hole identities")
        seen.add(key)
        source = design[key]
        if (row["geometry_token"] != source["geometry_token"] or
            row["nominal_x_mm"] != source["x_mm"] or
            row["nominal_y_mm"] != source["y_mm"] or
            row["nominal_diameter_mm"] != source["diameter_mm"]):
            raise ValueError("Inspection template does not match signed CAD geometry and revision")
        if row["inspection_status"] != "UNMEASURED" or any(row[c] for c in blanks):
            raise ValueError("Unissued CAD inspection template contains fabricated or entered measurement data")


def verify(folder: Path) -> dict:
    folder = Path(folder)
    if not folder.is_dir() or folder.is_symlink():
        raise ValueError("Missing handoff directory or directory is a symlink")
    mf = folder / "handoff_manifest.json"
    if not mf.is_file() or mf.is_symlink() or mf.stat().st_size > MAX_JSON_BYTES:
        raise ValueError("Missing, linked or oversized handoff manifest")
    try:
        manifest = json.loads(mf.read_text(encoding="utf-8"))
    except (ValueError, UnicodeError) as exc:
        raise ValueError("Invalid UTF-8 JSON manifest") from exc
    if not isinstance(manifest, dict) or manifest.get("schema_version") not in (1, 2):
        raise ValueError("Unknown or incomplete handoff manifest schema")
    schema = manifest["schema_version"]
    if set(manifest) != (EXPECTED_KEYS | MACHINING_KEYS if schema == 2 else EXPECTED_KEYS):
        raise ValueError("Unknown or incomplete handoff manifest schema")
    if (manifest["application"] != "CADWorkbench"
            or manifest["status"] != "UNISSUED_REVIEW_REQUIRED"):
        raise ValueError("Unsupported schema or incorrect approval state")
    for field in ("body_count", "bom_lines", "audit_errors", "audit_warnings",
                  "procurement_exceptions", "inspection_requirements") + (tuple(MACHINING_KEYS) if schema == 2 else ()):
        value = manifest[field]
        if type(value) is not int or value < 0:
            raise ValueError(f"Invalid manifest quantity: {field}")
    if (manifest["body_count"] <= 0 or manifest["bom_lines"] <= 0
            or manifest["audit_errors"] != 0):
        raise ValueError("Handoff has no design, no BOM, or outstanding audit errors")
    if not isinstance(manifest["generated_utc"], str) or not manifest["generated_utc"].endswith("Z"):
        raise ValueError("Invalid UTC timestamp")
    records = manifest["files"]
    if not isinstance(records, list) or not 9 <= len(records) <= 100:
        raise ValueError("Unexpected handoff file inventory")
    expected = {"handoff_manifest.json"}
    seen = set()
    source_count = 0
    for row in records:
        if not isinstance(row, dict) or set(row) != {"path", "sha256", "bytes"}:
            raise ValueError("Malformed file record")
        name, sha, size = row["path"], row["sha256"], row["bytes"]
        if (not isinstance(name, str) or not name.isascii() or
                name in (".", "..") or not re.fullmatch(r"[A-Za-z0-9_.-]{1,120}", name)):
            raise ValueError("Unsafe handoff filename")
        if name in seen or name == "handoff_manifest.json":
            raise ValueError("Duplicate file record")
        seen.add(name)
        if name.startswith("source_design."):
            source_count += 1
        if not isinstance(sha, str) or not HASH.fullmatch(sha):
            raise ValueError(f"Invalid SHA-256: {name}")
        if type(size) is not int or not 0 < size <= 10**11:
            raise ValueError(f"Invalid file size: {name}")
        path = folder / name
        if not path.is_file() or path.is_symlink():
            raise ValueError(f"Missing or linked file: {name}")
        actual_bytes = 0
        hasher = hashlib.sha256()
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(128 * 1024), b""):
                actual_bytes += len(block)
                hasher.update(block)
        if actual_bytes != size or hasher.hexdigest() != sha:
            raise ValueError(f"Size/hash mismatch: {name}")
        expected.add(name)
    mandatory = MANDATORY | MACHINING_FILES if schema == 2 else MANDATORY
    if source_count != 1 or not mandatory.issubset(seen):
        raise ValueError("Missing source design or required engineering outputs")
    actual = {entry.name for entry in folder.iterdir()}
    if actual != expected:
        raise ValueError(f"Unexpected or missing files: {sorted(actual ^ expected)}")
    if any(entry.is_symlink() or not entry.is_file() for entry in folder.iterdir()):
        raise ValueError("Symlinks and nested directories are not allowed")
    # Independent cross-check: a forged summary count cannot be trusted even
    # when all files were rehashed. Enforce CSV inventory consistency.
    csvs = {
        "bill_of_materials.csv": "bom_lines",
        "procurement_exceptions.csv": "procurement_exceptions",
        "feature_inspection_requirements.csv": "inspection_requirements",
    }
    for csv_file, field in csvs.items():
        with (folder / csv_file).open(encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.reader(stream))
        if not rows or not rows[0] or len(rows) - 1 != manifest[field]:
            raise ValueError(f"Inconsistent {field} row count")
    with (folder / "document_audit.csv").open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or "Severity" not in reader.fieldnames:
            raise ValueError("Document audit CSV lacks severity column")
        findings = list(reader)
    if any(row.get("Severity") not in {"ERROR", "WARNING", "INFO"} or None in row
           for row in findings):
        raise ValueError("Unknown or malformed document audit severity")
    errors = sum(1 for row in findings if row["Severity"] == "ERROR")
    warnings = sum(1 for row in findings if row["Severity"] == "WARNING")
    if errors != 0 or warnings != manifest["audit_warnings"]:
        raise ValueError("Audit findings and manifest counters do not agree")
    validate_engineering_links(folder, manifest)
    if schema == 2:
        validate_machining_review(folder, manifest)
        if INSPECTION_TEMPLATE in {entry["path"] for entry in manifest["files"]}:
            validate_inspection_template(folder)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    args = parser.parse_args()
    try:
        record = verify(args.folder)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    print(f"PASS: {len(record['files'])} files, sizes, hashes, CSV counts and unissued status verified; "
          "NOT a manufacturing approval")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

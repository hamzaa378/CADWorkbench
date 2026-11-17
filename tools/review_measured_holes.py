#!/usr/bin/env python3
"""Offline, read-only, independent hole measurements against signed CAD geometry.

The original UNISSUED CAD package is verified, but results are NOT certified
metrology, datum alignment, an engineering sign-off, or fabrication approval.
CSV rows MUST be independently measured and stored OUTSIDE the signed handoff.
The numerical policy mirrors src/core/HolePositionInspection.h; keep both tested.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import sys

from verify_manufacturing_handoff import (
    INSPECTION_TEMPLATE, INSPECTION_TEMPLATE_FIELDS, read_csv, verify,
)
from review_manufacturing_handoff import safe_output_path, safe_cell

MEASURED_NUMBERS = (
    "measured_datum_x_mm", "measured_datum_y_mm",
    "measured_projected_x_mm", "measured_projected_y_mm",
    "projected_height_mm", "actual_diameter_mm", "minimum_diameter_mm",
    "maximum_diameter_mm", "diametral_position_tolerance_mm",
    "maximum_pin_diameter_mm",
)
NOMINAL_FIELDS = ("body_id", "geometry_token", "hole_number",
                  "nominal_x_mm", "nominal_y_mm", "nominal_diameter_mm")
MAX_CSV_SIZE = 16 * 1024 * 1024


def bounded_number(value: str, field: str) -> float:
    # Keep exactly the numeric limits of the native inspection core.
    if not value or value.strip() != value:
        raise ValueError(f"Missing or padded measured number: {field}")
    try:
        number = float(value)
    except ValueError as exc:
        raise ValueError(f"Invalid measured number: {field}") from exc
    if not math.isfinite(number) or abs(number) > 1e9:
        raise ValueError(f"Nonfinite or excessive measured number: {field}")
    return number


def review(handoff: Path, measurements: Path) -> dict:
    handoff = Path(handoff)
    measurements = Path(measurements)
    manifest = verify(handoff)  # Verifies all signed handoff files BEFORE comparison.
    if manifest["schema_version"] != 2:
        raise ValueError("Hole inspection requires a v2 native CAD handoff")
    signed_files = {entry["path"] for entry in manifest["files"]}
    if INSPECTION_TEMPLATE not in signed_files:
        raise ValueError("Handoff lacks the signed nominal inspection template")
    if manifest["machining_holes"] < 1 or manifest["machining_holes"] > 1000:
        raise ValueError("Review requires 1-1000 signed CAD hole records")
    if not measurements.is_file() or measurements.is_symlink():
        raise ValueError("Independent measurement CSV missing or is a symlink")
    safe_output_path(measurements, handoff)  # Never mistake signed data for independent evidence.
    if measurements.stat().st_size > MAX_CSV_SIZE:
        raise ValueError("Independent measurement CSV exceeds size limit")
    with measurements.open(encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        if (not reader.fieldnames or len(reader.fieldnames) != len(INSPECTION_TEMPLATE_FIELDS)
                or set(reader.fieldnames) != INSPECTION_TEMPLATE_FIELDS):
            raise ValueError("Measured CSV must have precisely the signed template columns")
        rows = list(reader)
    if any(None in row or any(value is None for value in row.values()) for row in rows):
        raise ValueError("Malformed or truncated measured CSV row")
    design = read_csv(handoff, INSPECTION_TEMPLATE, INSPECTION_TEMPLATE_FIELDS)
    if len(rows) != len(design):
        raise ValueError("Every signed CAD hole requires exactly one independent measurement")
    identity = {}
    for row in rows:
        if row["inspection_status"] != "MEASURED":
            raise ValueError("Measurement status must be MEASURED, not approved or unmeasured")
        key = row["body_id"], row["hole_number"]
        if key in identity:
            raise ValueError("Duplicate independent measurement identity")
        identity[key] = row
    report_rows = []
    worst_utilization = 0.0
    accepted = 0
    rejected_size = rejected_position = rejected_fit = 0
    for nominal in design:
        key = nominal["body_id"], nominal["hole_number"]
        row = identity.get(key)
        if row is None or any(row[f] != nominal[f] for f in NOMINAL_FIELDS):
            raise ValueError("Measured hole missing, stale, or nominal CAD design/revision changed")
        body_id = nominal["body_id"]
        token = nominal["geometry_token"]
        composite = f"{len(body_id)}:{body_id}:{nominal['hole_number']}"
        if (not body_id or len(body_id) > 128 or not token or len(token) > 256
                or len(composite) > 128):
            raise ValueError("CAD identity is too long for the native metrology model")
        vals = {f: bounded_number(row[f], f) for f in MEASURED_NUMBERS}
        condition = row["material_condition"]
        if condition not in {"RFS", "MMC"}:
            raise ValueError("Material condition must be exactly RFS or MMC")
        x = bounded_number(nominal["nominal_x_mm"], "nominal_x_mm")
        y = bounded_number(nominal["nominal_y_mm"], "nominal_y_mm")
        height, actual = vals["projected_height_mm"], vals["actual_diameter_mm"]
        lower, upper = vals["minimum_diameter_mm"], vals["maximum_diameter_mm"]
        positional = vals["diametral_position_tolerance_mm"]
        pin = vals["maximum_pin_diameter_mm"]
        if (height < 0 or height > 1e6 or actual <= 0 or lower <= 0
                or lower > upper or upper > 1e6 or positional < 0 or positional > 1e6
                or pin < 0 or pin > 1e6):
            raise ValueError("Invalid hole size, projection height, tolerance, or pin limits")
        if height == 0 and (vals["measured_datum_x_mm"] != vals["measured_projected_x_mm"]
                            or vals["measured_datum_y_mm"] != vals["measured_projected_y_mm"]):
            raise ValueError("A zero-height axis requires identical measured datum and projected points")
        radial = max(math.hypot(vals["measured_datum_x_mm"] - x, vals["measured_datum_y_mm"] - y),
                     math.hypot(vals["measured_projected_x_mm"] - x,
                                vals["measured_projected_y_mm"] - y))
        measured_position = 2 * radial
        bonus = max(0.0, actual - lower) if condition == "MMC" else 0.0
        allowed = positional + bonus
        virtual = lower - positional
        clearance = actual - pin - measured_position if pin > 0 else 0.0
        if not all(map(math.isfinite, (measured_position, allowed, virtual, clearance))):
            raise ValueError("Inspection calculation overflow")
        size_pass = lower <= actual <= upper
        position_pass = measured_position <= allowed
        fit_pass = pin <= 0 or clearance >= 0
        is_accepted = size_pass and position_pass and fit_pass
        accepted += int(is_accepted)
        rejected_size += int(not size_pass)
        rejected_position += int(not position_pass)
        rejected_fit += int(not fit_pass)
        worst_utilization = max(worst_utilization,
                                measured_position / allowed if allowed > 0 else
                                (math.inf if measured_position > 0 else 0.0))
        report_rows.append({
            "body_id": nominal["body_id"], "hole_number": nominal["hole_number"],
            "geometry_token": nominal["geometry_token"],
            "measured_diametral_position_mm": measured_position,
            "material_bonus_mm": bonus, "allowed_diametral_position_mm": allowed,
            "virtual_condition_diameter_mm": virtual,
            "minimum_pin_clearance_mm": clearance if pin > 0 else None,
            "size_within_entered_limits": size_pass,
            "position_within_entered_limits": position_pass,
            "mating_pin_evaluated": pin > 0,
            "pin_clearance_within_entered_limits": fit_pass if pin > 0 else None,
            "within_all_entered_limits": is_accepted,
        })
    if len(identity) != len(design):
        raise ValueError("Unknown independent measurement identity")
    return {
        "status": "UNISSUED_INDEPENDENT_INSPECTION_REVIEW_REQUIRED",
        "release_approved": False,
        "source_handoff_manifest_sha256": hashlib.sha256((handoff / "handoff_manifest.json").read_bytes()).hexdigest(),
        "independent_measurement_sha256": hashlib.sha256(measurements.read_bytes()).hexdigest(),
        "source_handoff_generated_utc": manifest["generated_utc"],
        "checked_holes": len(design), "within_entered_limits": accepted,
        "rejected_size": rejected_size, "rejected_position": rejected_position,
        "rejected_pin_fit": rejected_fit,
        "all_within_entered_limits": accepted == len(design),
        "worst_position_utilization": worst_utilization if math.isfinite(worst_utilization) else None,
        "worst_position_utilization_unbounded": not math.isfinite(worst_utilization),
        "limitations": ["Entered datums and tolerances are externally supplied and unverified",
                        "CAD axial-hole inventory excludes some hole geometries",
                        "No datum alignment, CMM traceability, GD&T certification, or manufacturing approval"],
        "holes": report_rows,
    }


def render_csv(report: dict) -> str:
    fields = ("body_id", "hole_number", "geometry_token", "measured_diametral_position_mm",
              "material_bonus_mm", "allowed_diametral_position_mm",
              "virtual_condition_diameter_mm", "minimum_pin_clearance_mm",
              "size_within_entered_limits", "position_within_entered_limits",
              "mating_pin_evaluated", "pin_clearance_within_entered_limits",
              "within_all_entered_limits")
    out = io.StringIO(newline="")
    writer = csv.writer(out, lineterminator="\r\n")
    writer.writerow(fields)
    for row in report["holes"]:
        writer.writerow([safe_cell("" if row[k] is None else row[k]) for k in fields])
    return out.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("handoff", type=Path)
    parser.add_argument("measured_csv", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--csv", type=Path)
    args = parser.parse_args()
    try:
        outputs = [(safe_output_path(p, args.handoff), kind)
                   for p, kind in ((args.json, "json"), (args.csv, "csv")) if p]
        if len({p for p, _ in outputs}) != len(outputs) or any(
            p.exists() or not p.parent.is_dir() or p.is_symlink() for p, _ in outputs
        ):
            raise ValueError("Review outputs must be distinct new paths in existing directories")
        data = review(args.handoff, args.measured_csv)
        # Prepare all reports in memory and make partial-output cleanup
        # explicit when a later filesystem operation fails.
        contents = [(p, json.dumps(data, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
                     if kind == "json" else render_csv(data)) for p, kind in outputs]
        created = []
        try:
            for p, body in contents:
                with p.open("x", encoding="utf-8", newline="") as f:
                    created.append(p)
                    f.write(body)
        except OSError:
            for p in created:
                try:
                    p.unlink()
                except OSError:
                    pass
            raise
        print(json.dumps({k: data[k] for k in
            ("status", "release_approved", "checked_holes", "within_entered_limits",
             "all_within_entered_limits")}, indent=2))
        return 0
    except (OSError, ValueError, KeyError, TypeError, UnicodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

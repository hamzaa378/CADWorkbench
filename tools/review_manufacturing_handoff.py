#!/usr/bin/env python3
"""Create a human engineering punch-list from a VERIFIED, UNISSUED CAD handoff.

Never treats file integrity or empty exception lists as fabrication approval.
Outputs are written OUTSIDE the immutable handoff folder, without overwrite.
"""
from __future__ import annotations
import argparse
import csv
import io
import json
from pathlib import Path
import sys

from verify_manufacturing_handoff import read_csv, verify


def review(folder: Path) -> dict:
    manifest = verify(folder)
    folder = Path(folder)
    actions = []

    def add(area: str, severity: str, code: str, subject: str, action: str) -> None:
        actions.append(dict(area=area, severity=severity, code=code,
                            subject=subject, required_action=action))

    for row in read_csv(folder, "document_audit.csv", {"Severity", "Body ID", "Code", "Suggested action"}):
        if row["Severity"] in {"WARNING", "ERROR"}:
            add("MODEL_AUDIT", row["Severity"], row["Code"], row["Body ID"],
                row["Suggested action"] or "Review the audit finding")
    for row in read_csv(folder, "procurement_exceptions.csv",
                        {"part_number", "exception_code", "next_action"}):
        add("PROCUREMENT", "REVIEW", row["exception_code"], row["part_number"],
            row["next_action"])
    for row in read_csv(folder, "feature_inspection_requirements.csv",
                        {"body_id", "feature_id", "category", "review_status", "requirement"}):
        needs_refresh = row["review_status"] == "REVIEW_REQUIRED"
        add("INSPECTION", "REVIEW", "REVALIDATE_GEOMETRY" if needs_refresh else "PLAN_INSPECTION",
            f"{row['body_id']} / {row['feature_id']}", row["requirement"])

    areas = {area: sum(item["area"] == area for item in actions)
             for area in ("MODEL_AUDIT", "PROCUREMENT", "INSPECTION")}
    return {
        "status": "UNISSUED_ENGINEERING_REVIEW_REQUIRED",
        "source_package": str(folder.resolve()),
        "manifest_timestamp": manifest["generated_utc"],
        "body_count": manifest["body_count"],
        "bom_lines": manifest["bom_lines"],
        "open_action_count": len(actions),
        "action_counts": areas,
        "actions": actions,
        "release_approved": False,
    }


def safe_cell(value: str) -> str:
    value = str(value)
    if value.strip().startswith(("=", "+", "-", "@")):
        value = "'" + value
    return value


def render_csv(report: dict) -> str:
    buffer = io.StringIO(newline="")
    writer = csv.writer(buffer, lineterminator="\r\n")
    writer.writerow(["area", "severity", "code", "subject", "required_action"])
    for item in report["actions"]:
        writer.writerow([safe_cell(item[field]) for field in
                         ("area", "severity", "code", "subject", "required_action")])
    return buffer.getvalue()


def safe_output_path(output: Path, package: Path) -> Path:
    output = output.resolve()
    package = package.resolve()
    if output == package or package in output.parents:
        raise ValueError("Review output must be outside the immutable handoff folder")
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--json", type=Path, help="Create JSON review report outside handoff directory")
    parser.add_argument("--csv", type=Path, help="Create spreadsheet-safe action list outside handoff directory")
    args = parser.parse_args()
    try:
        result = review(args.folder)
        # Validate all paths BEFORE making either output. 'x' prevents overwrites.
        requested = [(args.json, "json"), (args.csv, "csv")]
        outputs = [(safe_output_path(path, args.folder), kind) for path, kind in requested if path]
        if len({path for path, _ in outputs}) != len(outputs):
            raise ValueError("JSON and CSV paths must differ")
        if any(path.exists() or not path.parent.is_dir() for path, _ in outputs):
            raise ValueError("Output already exists or its parent directory is missing")
        for path, kind in outputs:
            body = json.dumps(result, indent=2, ensure_ascii=False) + "\n" if kind == "json" else render_csv(result)
            with path.open("x", encoding="utf-8", newline="") as stream:
                stream.write(body)
        print(json.dumps({"status": result["status"], "open_action_count": result["open_action_count"],
                          "action_counts": result["action_counts"], "release_approved": False}, indent=2))
        return 0
    except (OSError, ValueError, TypeError, KeyError, UnicodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

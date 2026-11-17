#!/usr/bin/env python3
"""Read-only static ancestry audit for a saved CADWorkbench .cadscene file.

It NEVER interprets B-rep geometry or proves that witnesses still exist.  The
CAD kernel must revalidate all reported paths on a real Windows rebuild.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import tempfile

TRANSFORM = {"Translate", "Rotate X", "Rotate Y", "Rotate Z", "Rotate arbitrary axis",
             "Uniform scale", "Mirror XY", "Mirror XZ", "Mirror YZ", "Mirror arbitrary plane"}
PATTERN = {"Linear pattern X", "Linear pattern Y", "Linear pattern Z", "Linear pattern vector",
           "Circular pattern Z", "Circular pattern arbitrary axis"}
BOOLEAN = {"Fuse branches", "Subtract branches", "Intersect branches"}
MODIFIER = {"Fillet selected edge", "Chamfer selected edge", "Fillet all edges", "Chamfer all edges"}
TRACKED = TRANSFORM | PATTERN | BOOLEAN | MODIFIER


def has_witness(parameters: dict) -> bool:
    for prefix in ("edge_witness_", "face_anchor_witness_", "curved_witness_"):
        if all(isinstance(parameters.get(prefix + axis), (int, float)) and
               not isinstance(parameters.get(prefix + axis), bool) for axis in "xyz"):
            return True
    return False


def audit(data: dict) -> dict:
    if not isinstance(data, dict) or data.get("units") != "mm" or not isinstance(data.get("parts"), list):
        raise ValueError("Expected a saved CADWorkbench scene with units=mm and a parts array")
    findings = []
    checked = 0
    for part in data["parts"]:
        if not isinstance(part, dict):
            raise ValueError("Malformed part object")
        history = part.get("interleaved_history")
        if not isinstance(history, dict):
            continue
        steps = history.get("steps", [])
        if not isinstance(steps, list) or len(steps) > 40:
            raise ValueError("Invalid interleaved history step count")
        previous = "@base"
        parents = {}
        nodes = {}
        for step in steps:
            if not isinstance(step, dict) or not isinstance(step.get("id"), str):
                raise ValueError("Malformed history step")
            sid = step["id"]
            if not sid or sid == "@base" or sid in nodes:
                raise ValueError("Duplicate or empty history node")
            inputs = step.get("inputs", [previous])
            if not isinstance(inputs, list) or any(not isinstance(i, str) or
                (i != "@base" and i not in nodes) for i in inputs):
                raise ValueError("Unknown or forward parent in history")
            nodes[sid] = step
            if step.get("kind") == "Shape" and step.get("operation") in TRACKED:
                parents[sid] = list(dict.fromkeys(inputs))
            source = step.get("reference_source")
            if source is not None:
                checked += 1
                entry = {"body_id": part.get("id"), "body_name": part.get("name"),
                         "feature": sid, "reference_source": source,
                         "input_stage": inputs[0] if len(inputs) == 1 else None}
                params = step.get("parameters", {})
                if not isinstance(source, str) or source not in nodes and source != "@base":
                    entry.update(status="BLOCKED", reason="Reference source stage is missing")
                elif not isinstance(params, dict) or not has_witness(params):
                    entry.update(status="BLOCKED", reason="Saved physical face/edge witness is missing")
                elif len(inputs) != 1:
                    entry.update(status="BLOCKED", reason="Referenced modifier requires exactly one input")
                else:
                    target = inputs[0]
                    routes = []
                    def visit(cur: str, reverse: list[str], active: set[str]):
                        if len(routes) > 32:
                            return
                        if cur == source:
                            routes.append(list(reversed(reverse)));return
                        if cur in active or len(reverse) > 40:
                            return
                        for parent in parents.get(cur, []):
                            visit(parent, reverse + [cur], active | {cur})
                    if target == source:
                        routes = [[]]
                    else:
                        visit(target, [], set())
                    if not routes or len(routes) > 32:
                        entry.update(status="BLOCKED", reason="Unsupported, missing or excessive ancestry routes")
                    else:
                        stages = [st for route in routes for st in route]
                        kinds = {nodes[st].get("operation") for st in stages}
                        entry.update(status="KERNEL_REQUIRED" if kinds & (BOOLEAN | MODIFIER | PATTERN) else "TRACKED_TRANSFORM",
                                     path_count=len(routes), paths=routes,
                                     reason="Every route still requires current-kernel witness validation")
                findings.append(entry)
            previous = sid
    return {"schema": 1, "scope": "static saved-lineage preflight (not geometric certification)",
            "checked_references": checked,
            "blocked_references": sum(item["status"] == "BLOCKED" for item in findings),
            "references": findings}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scene", type=Path, help="Saved .cadscene file")
    parser.add_argument("--output", type=Path, help="Optional JSON report path")
    parser.add_argument("--strict", action="store_true", help="Exit 1 on blocked static references")
    args = parser.parse_args()
    try:
        if args.scene.stat().st_size > 96 * 1024 * 1024:
            raise ValueError("Scene file is above the supported 96 MiB audit limit")
        report = audit(json.loads(args.scene.read_text(encoding="utf-8")))
        result = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            # The scanner must not partially overwrite a previous valid report.
            fd, tmp = tempfile.mkstemp(dir=args.output.parent, prefix=".cad_audit_", suffix=".tmp")
            try:
                with os.fdopen(fd, "w", encoding="utf-8") as handle:
                    handle.write(result)
                os.replace(tmp, args.output)
            finally:
                if os.path.exists(tmp):os.unlink(tmp)
        else: print(result, end="")
        return 1 if args.strict and report["blocked_references"] else 0
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
        parser.exit(2, f"Lineage audit failed: {exc}\n")

if __name__ == "__main__":
    raise SystemExit(main())

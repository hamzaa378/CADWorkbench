"""Sprint 17 UI workflow runner. A dedicated VM is the safe way to leave the host usable.

pywinauto drives the *interactive desktop of the machine running this script*.
There is intentionally no claim that it can share the host user's active mouse.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import datetime
from pathlib import Path

from ui_workflow_engine import run_workflows, validate_scenarios


def require_ui_permission(*, in_isolated_vm: bool, allow_active_desktop: bool) -> None:
    """UIA cannot share a live Windows desktop; require explicit session choice."""
    if in_isolated_vm == allow_active_desktop:
        raise ValueError("Select one desktop-control mode: VM guest or explicitly active desktop")


def cli(argv=None):
    parser = argparse.ArgumentParser(description="CADWorkbench UI tests: menu, Ctrl+K fallback, nonblocking missing features")
    parser.add_argument("--exe", type=Path, help="Built Windows CADWorkbench.exe")
    parser.add_argument("--suite", type=Path, default=Path(__file__).with_name("sprint17_ui_scenarios.json"))
    parser.add_argument("--report-dir", type=Path, help="New empty evidence directory")
    parser.add_argument("--only", nargs="*", help="Only these scenario IDs")
    parser.add_argument("--list", action="store_true", help="List scenarios without launching a GUI")
    parser.add_argument("--validate", action="store_true", help="Validate scenario definitions on any OS")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--in-isolated-vm", action="store_true",
                           help="Acknowledge running inside a dedicated Windows VM (host mouse stays free)")
    selection.add_argument("--allow-active-desktop", action="store_true",
                           help="Explicitly permit controlling the current Windows desktop")
    args = parser.parse_args(argv)
    try:
        definitions = json.loads(args.suite.read_text(encoding="utf-8"))
        if definitions.get("schema") != "cad-ui-scenarios-v1":
            parser.error("Unsupported scenario schema")
        scenarios = definitions["scenarios"]
        validate_scenarios(scenarios)
    except (ValueError, KeyError, OSError) as exc:
        parser.error(f"Invalid scenario definitions: {exc}")
    if args.list:
        for sc in scenarios:
            print(f"{sc['id']:31} {sc.get('description', '')}")
        return 0
    if args.validate:
        print(f"VALID: {len(scenarios)} UI scenarios; no desktop was accessed")
        return 0
    try:
        require_ui_permission(in_isolated_vm=args.in_isolated_vm,
                              allow_active_desktop=args.allow_active_desktop)
    except ValueError as exc:
        parser.error(str(exc))
    if os.name != "nt":
        parser.error("Interactive Windows GUI tests must run on Windows, preferably inside a VM")
    if args.exe is None or not args.exe.is_file():
        parser.error("Provide --exe with an existing Windows CADWorkbench.exe")
    from windows_ui_backend import WindowsCadUI
    base = args.report_dir or (Path.home() / "Downloads" / "CADWorkbench_UI_Evidence" /
                               datetime.now().strftime("%Y%m%d_%H%M%S_%f"))
    chosen = set(args.only or [])
    if chosen and not chosen.issubset({s['id'] for s in scenarios}):
        parser.error("Unknown scenario id: " + ", ".join(sorted(chosen-{s['id'] for s in scenarios})))
    mode = "isolated-vm-guest" if args.in_isolated_vm else "active-desktop-explicit"
    print("RUN MODE:", mode)
    print("NOTE: GUI actions control the desktop of the running Windows session.")
    try:
        results = run_workflows(lambda name,path: WindowsCadUI(args.exe.resolve(), name, path),
                                scenarios, base, selected=chosen or None, execution_mode=mode)
    except ValueError as exc:
        parser.error(str(exc))
    print("\nCADWorkbench UI evidence:", base)
    for result in results:
        print(f"{result.name:<33} {result.status:<21} {result.detail}")
    print("HTML:", base / "report.html")
    print("JSON:", base / "report.json")
    if any(result.status == "FAIL" for result in results):
        return 2
    if any(result.status in ("AUTOMATION_BLOCKED", "PRECONDITION_BLOCKED") for result in results):
        return 3
    if any(result.status in ("NOT_AVAILABLE", "DISCOVERED") for result in results):
        return 4  # Report produced; not a pass/fail of CAD geometry.
    return 0

if __name__ == "__main__":
    sys.exit(cli())

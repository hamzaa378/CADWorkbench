"""CADWorkbench UI workflow scheduler: strict evidence, graceful missing commands.

The runner is deliberately separate from the 60 manual engineering-acceptance
cases. Command search is a fallback, not a test of menu consistency. A command
being discoverable does not mean its engineering behavior is correct.
"""
from __future__ import annotations

import html
import json
import re
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any, Callable


class NotAvailable(RuntimeError):
    """Command absent from both its expected menu and the live Ctrl+K catalog."""


class AutomationBlocked(RuntimeError):
    """GUI driver could not establish the result; not a CAD functional failure."""


class PreconditionBlocked(RuntimeError):
    """Required test artifact or setup could not be established."""


class WorkflowFailure(RuntimeError):
    """Observed CAD result contradicts an explicit scenario assertion."""


STATES = {"PASS", "FAIL", "NOT_AVAILABLE", "AUTOMATION_BLOCKED",
          "PRECONDITION_BLOCKED", "DISCOVERED"}
_ALLOWED_OPERATIONS = {"command", "command_probe", "accept_dialog", "select_body",
                       "assert_bodies", "save_file", "open_file", "export_file",
                       "assert_dialog", "close_dialog", "assert_window_title", "assert_scene"}
_ASSERT_OPERATIONS = {"assert_bodies", "save_file", "open_file", "export_file", "assert_dialog", "assert_window_title", "assert_scene"}
_ID = re.compile(r"[a-zA-Z][a-zA-Z0-9_-]{1,63}\Z")


def normalized_label(text: str) -> str:
    text = re.sub(r"\s*\[\s*(Ctrl|Alt|Shift|F\d+)[^]]*\]$", "", text, flags=re.I)
    text = re.sub(r"\(\s*(Ctrl|Alt|Shift|F\d+)[^)]*\)$", "", text, flags=re.I)
    return re.sub(r"\s+", " ", text.replace("&", "").strip()).casefold()


def exact_palette_match(display: str, expected: str) -> bool:
    """Do not trigger unrelated fuzzy hits from the command palette."""
    display = re.sub(r"^\s*[★*]\s*", "", display).strip()
    head = re.split(r"\s+[·•]\s+", display, maxsplit=1)[0]
    return normalized_label(head) == normalized_label(expected)


def validate_scenarios(scenarios: list[dict[str, Any]]) -> None:
    if not isinstance(scenarios, list) or not scenarios:
        raise ValueError("Scenario suite must contain a nonempty list")
    seen = set()
    for scenario in scenarios:
        if not isinstance(scenario, dict) or not _ID.fullmatch(str(scenario.get("id", ""))):
            raise ValueError("Scenario id must be a safe short alphanumeric identifier")
        ident = scenario["id"]
        if ident in seen:
            raise ValueError(f"Duplicate scenario id: {ident}")
        seen.add(ident)
        steps = scenario.get("steps")
        if not isinstance(steps, list) or not steps:
            raise ValueError(f"Scenario {ident} has no steps")
        for step in steps:
            if not isinstance(step, dict) or step.get("op") not in _ALLOWED_OPERATIONS:
                raise ValueError(f"Scenario {ident}: unknown workflow operation")
            if step["op"] in ("command", "command_probe"):
                menu = step.get("menu")
                if not isinstance(menu, list) or len(menu) < 2 or not all(isinstance(x, str) and x for x in menu):
                    raise ValueError(f"Scenario {ident}: command menu must be a nonempty path")
                if step.get("label") != menu[-1]:
                    raise ValueError(f"Scenario {ident}: command label disagrees with menu path")


@dataclass
class StepEvidence:
    operation: str
    status: str
    detail: str = ""
    route: str = ""
    duration_seconds: float = 0


@dataclass
class ScenarioEvidence:
    name: str
    status: str
    detail: str
    steps: list[StepEvidence] = field(default_factory=list)
    screenshot: str = ""
    duration_seconds: float = 0


def _safe_output(workspace: Path, name: str) -> Path:
    path = Path(name)
    if path.is_absolute() or len(path.parts) != 1 or path.name in ("", ".", "..") or ":" in name or "\\" in name:
        raise WorkflowFailure("Unsafe test output path")
    return workspace / path


def perform_step(backend: Any, step: dict[str, Any], workspace: Path) -> tuple[str, str]:
    op = step["op"]
    if op in ("command", "command_probe"):
        route = backend.command(step["menu"], step["label"], step.get("query", step["label"]),
                                execute=(op == "command"))
        return route, ("Executed" if op == "command" else "Discovered") + f" {step['label']} via {route}"
    if op == "accept_dialog":
        backend.accept_dialog(step["title"], step.get("fields", []), button=step.get("button", "OK"))
        return "dialog", f"Confirmed {step['title']}"
    if op == "select_body":
        backend.select_body(step.get("index", 0), extend=step.get("extend", False))
        return "browser", f"Selected body row {step.get('index', 0)}"
    if op == "assert_bodies":
        want = int(step["count"])
        if want < 0:
            raise WorkflowFailure("Invalid body count assertion")
        value = backend.wait_body_count(want) if hasattr(backend, "wait_body_count") else backend.body_count()
        if value != want:
            raise WorkflowFailure(f"Expected {want} bodies; observed {value}")
        return "browser", f"Verified {value} bodies"
    if op in ("save_file", "export_file"):
        output = _safe_output(workspace, step["name"])
        backend.save_file(str(output), step.get("title", "Save project"))
        if not output.is_file() or output.stat().st_size < int(step.get("min_bytes", 24)):
            raise WorkflowFailure(f"Output missing or too small: {output.name}")
        if output.suffix.lower() == ".svg":
            import xml.etree.ElementTree as ET
            try:
                root = ET.parse(output).getroot()
            except ET.ParseError as exc:
                raise WorkflowFailure(f"SVG is malformed: {exc}") from exc
            if root.tag.split("}")[-1].lower() != "svg":
                raise WorkflowFailure("Output XML is not SVG")
        return "file", f"Verified {output.name} ({output.stat().st_size} bytes)"
    if op == "assert_scene":
        source = _safe_output(workspace, step["name"])
        if not source.is_file():
            raise PreconditionBlocked(f"Native scene missing: {source.name}")
        if source.stat().st_size > 30_000_000:
            raise WorkflowFailure("Scene exceeds bounded automation JSON size")
        try:
            data = json.loads(source.read_text(encoding="utf-8"))
        except (ValueError, UnicodeError) as exc:
            raise WorkflowFailure(f"Saved CAD scene is not valid JSON: {exc}") from exc
        if not isinstance(data, dict) or data.get("units") != "mm" or not isinstance(data.get("parts"), list):
            raise WorkflowFailure("Saved scene does not have expected CADWorkbench mm/parts structure")
        checks = step.get("checks", [])
        if not checks:
            raise WorkflowFailure("assert_scene requires one or more expected values")
        for check in checks:
            path = check["path"]
            item = data
            try:
                for segment in path.split("."):
                    item = item[int(segment)] if isinstance(item, list) else item[segment]
            except (IndexError, KeyError, ValueError, TypeError) as exc:
                raise WorkflowFailure(f"Missing persisted CAD scene field: {path}") from exc
            expected = check["equals"]
            if isinstance(expected, (int, float)) and not isinstance(expected, bool):
                if not isinstance(item, (int, float)) or isinstance(item, bool) or abs(item-expected) > float(check.get("tolerance",1e-7)):
                    raise WorkflowFailure(f"Persisted {path}: expected {expected}, observed {item}")
            elif item != expected:
                raise WorkflowFailure(f"Persisted {path}: expected {expected!r}, observed {item!r}")
        return "native scene JSON", f"Verified {len(checks)} persisted fields in {source.name}"
    if op == "open_file":
        output = _safe_output(workspace, step["name"])
        if not output.is_file():
            raise PreconditionBlocked(f"Saved input missing: {output.name}")
        backend.open_file(str(output))
        return "file", f"Opened {output.name}"
    if op == "assert_dialog":
        value = backend.dialog_text(step["title"])
        required = step.get("contains", "")
        if required and required.casefold() not in value.casefold():
            raise WorkflowFailure(f"Dialog {step['title']} missing expected text: {required}")
        return "dialog", f"Verified {step['title']} dialog"
    if op == "close_dialog":
        backend.accept_dialog(step["title"], button=step.get("button", "Cancel"))
        return "dialog", f"Closed {step['title']}"
    if op == "assert_window_title":
        actual = backend.window_title()
        required = step["contains"]
        if required.casefold() not in actual.casefold():
            raise WorkflowFailure(f"Title does not contain {required!r}: {actual!r}")
        return "window", f"Verified window title: {required}"
    raise ValueError(f"Unsupported operation: {op}")


def run_workflows(backend_factory: Callable, scenarios: list[dict[str, Any]],
                  report_dir: Path, *, selected: set[str] | None = None,
                  execution_mode: str = "active-desktop") -> list[ScenarioEvidence]:
    validate_scenarios(scenarios)
    if selected and not selected.issubset({s["id"] for s in scenarios}):
        raise ValueError("Unknown scenario id")
    report_dir.mkdir(parents=True, exist_ok=True)
    if any(report_dir.iterdir()):
        raise ValueError("Report directory must be empty; never overwrite existing evidence")
    results: list[ScenarioEvidence] = []
    for scenario in scenarios:
        if selected and scenario["id"] not in selected:
            continue
        start = time.monotonic()
        workspace = report_dir / scenario["id"]
        workspace.mkdir()
        steps = []
        status = "PASS" if any(step["op"] in _ASSERT_OPERATIONS for step in scenario["steps"]) else "DISCOVERED"
        detail = "Observed workflow assertions passed" if status == "PASS" else "Command was discoverable; behavior not verified"
        screenshot = ""
        backend = None
        current_step = "launch"
        try:
            backend = backend_factory(scenario["id"], workspace)
            backend.start()
            for step in scenario["steps"]:
                current_step = step["op"]
                tick = time.monotonic()
                route, info = perform_step(backend, step, workspace)
                steps.append(StepEvidence(current_step, "PASS", info, route,
                                          round(time.monotonic()-tick, 3)))
        except (NotAvailable, AutomationBlocked, PreconditionBlocked, WorkflowFailure) as exc:
            status = {NotAvailable:"NOT_AVAILABLE", AutomationBlocked:"AUTOMATION_BLOCKED",
                      PreconditionBlocked:"PRECONDITION_BLOCKED", WorkflowFailure:"FAIL"}[type(exc)]
            detail = str(exc)
            steps.append(StepEvidence(current_step, status, detail))
        except Exception as exc:
            status, detail = "AUTOMATION_BLOCKED", f"Driver exception: {type(exc).__name__}: {exc}"
            steps.append(StepEvidence(current_step, status, detail))
        finally:
            if backend is not None:
                try:
                    img = workspace / "final_state.png"
                    if backend.screenshot(img):
                        screenshot = str(img.relative_to(report_dir))
                except Exception:
                    pass
                try:
                    backend.stop()
                except Exception:
                    pass
        results.append(ScenarioEvidence(scenario["id"], status, detail, steps, screenshot,
                                        round(time.monotonic()-start, 3)))
        write_report(report_dir, results, execution_mode=execution_mode)
    return results


def write_report(directory: Path, results: list[ScenarioEvidence], *, execution_mode: str = "active-desktop") -> None:
    summary = {state: sum(result.status == state for result in results) for state in sorted(STATES)}
    payload = {"schema": "cad-ui-workflows-v2", "mode": execution_mode,
               "manual_acceptance": False, "summary": summary,
               "results": [asdict(result) for result in results]}
    output = directory / "report.json"
    tmp = directory / "report.json.tmp"
    tmp.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    tmp.replace(output)
    rows = []
    for result in results:
        details = "<ul>" + "".join(f"<li>{html.escape(s.operation)}: {html.escape(s.status)}"
                                     f" — {html.escape(s.detail)} ({html.escape(s.route)})</li>"
                                     for s in result.steps) + "</ul>"
        link = f'<a href="{html.escape(result.screenshot, quote=True)}">Screenshot</a>' if result.screenshot else ""
        rows.append(f"<tr><td>{html.escape(result.name)}</td><td>{html.escape(result.status)}</td>"
                    f"<td>{html.escape(result.detail)}{details}</td><td>{link}</td></tr>")
    page = ("<!doctype html><html lang='en'><meta charset='utf-8'><title>CADWorkbench UI Regression</title>"
            "<style>body{font:15px system-ui;max-width:1200px;margin:30px auto;color:#172235}"
            "table{border-collapse:collapse;width:100%}td,th{border:1px solid #bfc8d2;padding:10px;vertical-align:top}"
            "th{background:#ecf2f6}</style><h1>CADWorkbench UI workflow evidence</h1>"
            "<p>Menu first; Ctrl+K fallback. NOT_AVAILABLE means absent from both routes, not UI inconsistency. "
            "AUTOMATION_BLOCKED means no verified result, not a CAD failure. DISCOVERED means command found only. "
            "These results do not replace engineering acceptance.</p>"
            "<table><tr><th>Workflow</th><th>Status</th><th>Evidence</th><th>Screenshot</th></tr>"
            + "".join(rows) + "</table></html>")
    temp = directory / "report.html.tmp"
    temp.write_text(page, encoding="utf-8")
    temp.replace(directory / "report.html")

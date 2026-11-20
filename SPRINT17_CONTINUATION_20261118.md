# Sprint 17 — UI regression orchestration continuation (2026-11-18)

## Starting point and preservation

- Baseline: complete GitHub-ready `CADWorkbench_v170_Sprint16_CTestTargetCorrected_GitHub_READY_20261118.zip`, all **697 files preserved** in this Sprint 17 source distribution. Existing `src/`, `CMakeLists.txt`, native tests and C++ functionality are unchanged.
- User-reported November 18 Windows baseline: **CADWorkbench.exe Release compiled under MSVC; 130/130 native CTests passed; 1,050/1,050 Python/source tests passed**, automated result `AUTOMATED_PASS`. 60 manual engineering cases are still unapproved. Initial user-driven UI smoke test reported 8/8 PASS (not the formal acceptance matrix).
- Sprint 17 adds isolated Python UI automation and noninteractive regression launchers. No claim of native Windows or VM UIA execution in this workspace. Python/source total after additions: **1,075/1,075 locally passed** using `python -m unittest discover -s tests -p '*.py' -q`.
- Existing named source manifest SHA256SUMS.txt and SOURCE_MANIFEST_SHA256.txt are regenerated to include Sprint 17 additions. Use `tools/preflight_major_build.py` and `tools/audit_full_source.py` for source checks. Always verify native MSVC build after C++ changes; none were made for Sprint 17.

## Added modules and ownership

- `automation/ui_workflow_engine.py` — scenario state machine, strict visible/file/persisted JSON assertions, nonfatal unavailable behavior, per-step evidence, HTML/JSON reports, report-directory safety.
- `automation/windows_ui_backend.py` — pywinauto Windows UIA driver. Menu first, Ctrl+K exact live command palette fallback, dialog and file chooser interactions, body browser observation, screenshots. Windows-only; still requires real guest or active-desktop verification.
- `automation/sprint17_ui_scenarios.json` — 16 independent workflows: 6 executable smoke workflows and 10 discovery-only probes. Clearly separate discovered commands from functional passes.
- `automation/runner.py` — cross-platform validation/listing and Windows guarded GUI session entrypoint; explicit VM guest or active-desktop permission required.
- `RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1` — guest/foreground wrapper with evidence directories.
- `RUN_SPRINT17_BACKGROUND_REGRESSION_WINDOWS.ps1` — detached noninteractive CTest and Python regressions for host while user works; does not drive GUI.
- `APPLY_SPRINT17_UI_AUTOMATION_TO_VERIFIED_SPRINT16.ps1` — non-destructive eight-file overlay into existing Sprint16 build tree, with SHA256 copying verification. Does not replace executable/Qt DLLs/CMake/source manifests.
- `tests/test_sprint17_ui_automation.py` — 25 fake driver, report safety, missing command behavior, scenario validation, and permission guard tests.
- `SPRINT17_UI_AUTOMATION_GUIDE.md` — exact fresh Windows PowerShell commands, VM setup expectations, reported states and honest limitations.

## Expected outcomes and next gates

- `PASS`: independent UI workflow action was completed and an explicit observable assertion passed.
- `DISCOVERED`: command found in menu or Ctrl+K but no behavior verified (never count as engineering pass).
- `NOT_AVAILABLE`: absent or unavailable in menu and exact live search; continue automatically, no menu-inconsistency classification.
- `AUTOMATION_BLOCKED`: UIA/edit control/session unable to prove outcome. Do **not** call an application feature failed.
- `PRECONDITION_BLOCKED`: test setup cannot establish required model/scene/file.
- `FAIL`: an explicit geometry/body-count/native-scene assertion differs.

The first Windows guest runs must validate QDoubleSpinBox accessibility, native QFileDialog controls, selected body visibility in Qt QTreeWidget, exact QAction palette matching and screenshot capture. Use `--only box_save_reopen parametric_solid folded_geometry` for early VM triage. Record regressions and preserve screenshot evidence. Do not manipulate a person's active mouse/keyboard without `-AllowActiveDesktop`. The preferred safe workflow is Windows VM guest GUI automation while the host laptop remains available; a second virtual desktop within one Windows login session is *not* equivalent isolation.

## Future substantial integration

Prioritize replacing fragile ordinal dialog field addressing with stable accessible widget IDs, adding native geometry assertions for assembly drive transforms and geometry export, collecting stress/performance traces and auto-attaching evidence to the 60-case acceptance workbook **only when criteria match**. User-review of actual engineering results remains necessary. Future work should not reintroduce extra GUI-only features absent from physical usage requirements or modify baseline C++ merely for a headless test mode without native verification.

## 2026-11-18 Qt dialog automation repair checkpoint

- User's initial real Windows UI result: 7 `AUTOMATION_BLOCKED`, 7 `DISCOVERED`, zero `FAIL`; menu actions worked but Qt dialogs (Create Box, parametric extrusion, folded sheet metal, engineering preflight, Ctrl+K) were not returned by the original process-filtered UIA window enumeration. Screenshots failed because Pillow/PIL was absent.
- New `automation/qt_dialog_discovery.py` supplements UIA with PID/title-verified Qt HWND discovery and main-window exclusion. Repaired `automation/windows_ui_backend.py` uses direct-handle UIA, Win32 fallback, and focus-verified TAB/Enter input for QFormLayout controls; it refuses to type into unrelated windows. File dialogs and command search use the same safety checks. Pillow screenshot fallback captures only the CAD window.
- New `APPLY_SPRINT17_QT_DIALOG_FIX_TO_VERIFIED_SPRINT16.ps1` safely overlays the changed Sprint 17 Python and PowerShell files onto the same compiled Sprint 16 Windows baseline. It verifies the 695 original Sprint 16 source hashes and expected previous overlay file hashes, backs up old overlay files, does not change C++, CADWorkbench.exe, build, or source manifest.
- `RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1` now checks Pillow, win32gui and pywinauto pre-launch; standard Python 3.13 virtual environment recommended rather than free-threaded Python 3.14t.
- **Live Windows behavior of new driver is NOT VERIFIED** here. First run `-Only box_save_reopen,parametric_solid`, inspect generated report and screenshots, and only then run all 16 scenarios. No engineering acceptance claim is made. Existing Sprint 16 MSVC/130 native baseline remains unchanged.
- Local full Python test inventory using `python -m unittest discover -s tests -p '*.py' -q`: 1088/1088 PASS (13 new Qt discovery/fallback tests).

# Current Qt6 modal dialog update (2026-11-18)

The original UIA-only runner could click **Create > Box...** correctly but could not see the Qt `QDialog` via `Desktop.windows(process=...)`. The subsequent Qt-modal overlay adds verified Win32 HWND discovery and foreground-safe keyboard field entry, plus Pillow screenshots. **Apply `APPLY_SPRINT17_QT_DIALOG_FIX_TO_VERIFIED_SPRINT16.ps1`** to an existing Sprint 17 overlay; the older installer intentionally refuses replacement of an already installed UI driver. See `SPRINT17_QT_DIALOG_FIX_20261118.md`. This Qt-modal driver still requires its first live Windows retest.

---

# Sprint 17 — CADWorkbench UI automation (source add-on)

## Two execution modes: no misleading promises about Windows desktops

1. **Background native/Python regression** — safe to run on your normal laptop while doing other work. Uses existing CTest and Python tests; does **not** click the CAD GUI. The computer may be slower while these tests use CPU/RAM. Run `RUN_SPRINT17_BACKGROUND_REGRESSION_WINDOWS.ps1 -Detach` from a built CADWorkbench checkout.
2. **Interactive Qt/Win32 UI workflow automation** — the test runner must control the active desktop **of the Windows session in which it runs**. To keep using the host laptop, run this **inside an already configured Windows virtual machine** with a logged-in interactive guest desktop. You can minimize the VM *window on the host*. The guest must remain logged in and unlocked, with the app displayed and access to its files. A host screen lock, switching desktops, or a disconnected/locked guest can block GUI automation. Running it on the host without a VM is permitted only with `-AllowActiveDesktop` and will interfere with the host mouse/keyboard. There is no supported no-VM hidden UIA/desktop mode.

The `-InIsolatedVM` switch is a **user attestation**, not a VM detector. Do not use it on your normal desktop just to bypass the guard. Running a separate headless CTest is not the same as automating GUI clicks.

## What is implemented

- Python UIA driver using `pywinauto` (Windows only), launches a fresh CADWorkbench instance per scenario; no reused modeling state.
- Every command resolution first attempts its specified menu path; if absent/inaccessible, it opens **Ctrl+K** and searches the live QAction catalog. It **requires an exact enabled command label**, never clicks a fuzzy/unrelated result.
- If both routes are absent: record `NOT_AVAILABLE`, include a screenshot if available, close that test process and continue with the **next scenario**. This is **not** an application UI inconsistency failure.
- Observable assertions for six first-pass workflows: box save/reopen, constrained extrusion, folded-sheet creation, undo/redo, named-dimension editing with persisted JSON, and integrated readiness dialog. The other 10 scenarios are **command discovery** only and are truthfully labeled `DISCOVERED` (never PASS just because a menu action exists).
- Scenario result classes: `PASS`, `FAIL` (observable engineering assertion mismatch), `NOT_AVAILABLE` (not found in menu or Ctrl+K), `AUTOMATION_BLOCKED` (UIA/session/driver), `PRECONDITION_BLOCKED`, `DISCOVERED`.
- Unique file-output workspaces, JSON and HTML reports, optional screenshot, per-step route and timing. The report explicitly does **not** count as any of the 60 formal engineering acceptance cases.
- No coordinate-based macros, OCR, injected C++ backdoors, or feature implementation changes. The verified Sprint 16 CAD source remains unchanged.

**Limitation:** Windows UIA exposure varies by Qt/Windows setup. Some QDoubleSpinBox controls or the Design Browser may not expose stable edit controls; such cases are `AUTOMATION_BLOCKED`, *not* false CAD failures. Actual successful UIA runs and full workflows must be verified on the user's Windows VM. Avoid claiming this suite fully tests drawing geometry, assembly collision, or 60 formal cases.

## A. Install in your existing Sprint 16 Windows folder (no CAD rebuild)

From a fresh Windows PowerShell window, after downloading the complete Sprint 17 ZIP to Downloads:

```powershell
Set-Location "$env:USERPROFILE\Downloads"
$zip = 'CADWorkbench_v170_Sprint17_UIAutomation_BackgroundSafe_GitHub_Source_20261118.zip'
if (!(Test-Path -LiteralPath $zip)) { throw "ZIP missing: $zip" }
$dest = 'CADWorkbench_Sprint17_Automation_Source'
if (Test-Path -LiteralPath $dest) { throw 'Extract destination already exists; choose a fresh folder' }
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$source = Join-Path (Resolve-Path $dest).Path 'CADWorkbench'
$verified = Join-Path (Get-Location) 'CADWorkbench_Sprint16_New_Test_20261118_151507\CADWorkbench'
& (Join-Path $source 'APPLY_SPRINT17_UI_AUTOMATION_TO_VERIFIED_SPRINT16.ps1') -ExistingRoot $verified
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Overlay failed' }
```

Overlay copies **only eight automation/test files**. It deliberately does not copy changed main sources, update the original Sprint 16 manifest, overwrite build artifacts, or touch saved designs.

## B. Run noninteractive regressions in the background on your laptop

In a fresh PowerShell window:

```powershell
$root = "$env:USERPROFILE\Downloads\CADWorkbench_Sprint16_New_Test_20261118_151507\CADWorkbench"
Set-Location $root
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT17_BACKGROUND_REGRESSION_WINDOWS.ps1 -Detach -Parallel 4
```

This starts a **separate hidden PowerShell process** for existing CTests + Python tests and returns a process ID. You can keep working. Logs, XML and `summary.json` appear under `<root>\build\background_regressions\<timestamp>`. Check the newest report:

```powershell
$reports = Join-Path $root 'build\background_regressions'
$latest = Get-ChildItem $reports -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($latest) { Get-Content (Join-Path $latest.FullName 'summary.json') -ErrorAction SilentlyContinue }
```

If no `summary.json` exists yet, the independent process is still executing or exited before writing results. Inspect `ctest.log` and `python.log` in that directory. Native tests must already be built. This background mode does not drive menus or mouse clicks.

## C. Run GUI tests in a Windows VM, leaving the host free

- Create and configure a **Windows 11 VM** (for example, Hyper-V or VirtualBox). Keep the guest interactive desktop **logged in and unlocked** throughout the run. On the guest, install Python and run `py -3 -m pip install pywinauto`. Obtain a working `CADWorkbench.exe` and its Qt/OCCT runtime dependencies **inside the guest** (build there or copy a previously deployed working Windows distribution). Merely copying an EXE without its DLLs is insufficient.
- Copy this source/automation folder into the VM. In the **guest PowerShell**, navigate to the unpacked `CADWorkbench` folder and run:

```powershell
$root = 'C:\CADWorkbench'   # change to the actual folder INSIDE the VM
$exe = 'C:\CADWorkbench\build\Release\CADWorkbench.exe' # change to real deployed guest exe
Set-Location $root
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1 -ExePath $exe -InIsolatedVM
```

The test takes focus *inside the guest* while running. Minimize the VM window on the **host** to keep working on your laptop. The runner writes HTML/JSON and screenshots inside guest Downloads/CADWorkbench_UI_Evidence. VM GPU/OpenGL and guest UI accessibility differences may block some tests.

For a first, shorter run inside the VM:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1 -ExePath $exe -InIsolatedVM -Only box_save_reopen,parametric_solid,folded_geometry
```

If you have no VM, you can run on the normal desktop only after explicitly agreeing to foreground mouse/focus control:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1 -ExePath $exe -AllowActiveDesktop
```

Do not use the computer for other interactive work during an active-desktop GUI run. A second virtual desktop in the **same logged-in Windows session** is not a supported substitute for a VM.

## D. Inspect what is covered without opening any GUI (any OS)

```powershell
python .\automation\runner.py --validate
python .\automation\runner.py --list
```

Expected 16 scenarios. The 10 `probe_*` scenarios are discoverability only and must not be counted as engineering acceptance. Native CTest and Python failures remain genuine regressions even if the UI wrapper records a feature as unavailable.

## Developer handoff

The runner is in `automation/runner.py`, UI driver in `automation/windows_ui_backend.py`, state/evidence engine in `automation/ui_workflow_engine.py`, scenarios in `automation/sprint17_ui_scenarios.json`, tests in `tests/test_sprint17_ui_automation.py`. Extend scenarios with explicit visible/geometry assertions rather than counting actions as PASS. Preserve command search as fallback and keep independent scenarios running after NOT_AVAILABLE. Keep the 60 human acceptance cases distinct from this smoke suite.

# Sprint 17 Qt modal dialog automation repair — 2026-11-18

## Source of the change
User-provided Windows JSON report (`report(1).json`) from `active-desktop-explicit` shows 7 `AUTOMATION_BLOCKED`, 7 `DISCOVERED`, zero CAD `FAIL`: Create > Box, constrained rectangle extrusion, folded sheet-metal and Inspect > integrated readiness were successfully invoked via menus, but `Desktop(backend='uia').windows(process=...)` returned only the CADWorkbench main window. Ctrl+K popup likewise was not exposed through the same UIA enumeration. Screenshots were empty and the run printed `PIL does not seem to be installed`.

## Scope and fix
1. `automation/qt_dialog_discovery.py` enumerates Windows-native top-level **and** child HWNDs of the CAD process using pywin32, with exact title, PID, visibility and main-window exclusion checks. Qt6 QDialogs may have valid HWNDs without being returned by UIA top-level enumeration.
2. `automation/windows_ui_backend.py` first uses UIA, then resolves a **verified named CAD modal** by HWND, attempts UIA on its HWND, falls back to Win32 wrapper and finally to focus-verified keyboard tab navigation for QFormLayout's ordered fields. Never sends blind keyboard input to an unverified window.
3. Save/Open file chooser fallback sends filename via Alt+N only if the correct CAD process dialog is active. Ctrl+K command palette uses the same verified modal discovery; if command results remain inaccessible, it stays `AUTOMATION_BLOCKED`, never fabricates `NOT_AVAILABLE` or `PASS`.
4. `RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1` checks pywinauto, pywin32, and Pillow **before** opening CAD. Screenshot has window-only Pillow crop fallback. No CAD engine, native build or GUI C++ changes.
5. `tests/test_sprint17_qt_dialog_fix.py` adds pure unit tests for safe window choice, process/title isolation, keyboard fallback, preserved comma-separated sheet-metal inputs, and no false-positive engineering-dialog assertion.
6. `APPLY_SPRINT17_QT_DIALOG_FIX_TO_VERIFIED_SPRINT16.ps1` updates only the Sprint 17 automation overlay on a verified Sprint 16 source tree. It validates baseline source manifest and expected old-overlay fingerprints, creates backups, and never modifies CADWorkbench.exe, C++ source, or build tools.

## Verification scope
Portable tests validate discovery and fallback dispatch **logic** only. Actual Qt6 dialog HWND enumeration, focus movement, keyboard input and Pillow screenshot output **require live Windows testing**. This change is not a claim that the 6 executable GUI scenarios now pass. Confirm by running `-Only box_save_reopen,parametric_solid` first, reviewing generated `report.json`, then run all 16. `DISCOVERED` is not `PASS`. Manual engineering acceptance is separate.

## Installation using the existing working Sprint 16 folder
From a fresh PowerShell window, with corrected ZIP in Downloads:

```powershell
Set-Location "$env:USERPROFILE\Downloads"
$zip='CADWorkbench_v170_Sprint17_QtDialogFixed_GitHub_Source_20261118.zip'
$root=Join-Path (Get-Location) 'CADWorkbench_Sprint16_New_Test_20261118_151507\CADWorkbench'
$dest='Sprint17_QtDialogFix_'+(Get-Date -Format yyyyMMdd_HHmmss)
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$src=Join-Path (Join-Path (Get-Location) $dest) 'CADWorkbench'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $src 'APPLY_SPRINT17_QT_DIALOG_FIX_TO_VERIFIED_SPRINT16.ps1') -ExistingRoot $root
if ($LASTEXITCODE -ne 0) { throw 'Qt dialog fix install failed' }
$python=Join-Path $env:USERPROFILE 'Downloads\CADWorkbench_UI_Python313\Scripts\python.exe'
& $python -m pip install pillow
if ($LASTEXITCODE -ne 0) { throw 'Screenshot dependency install failed' }
Set-Location $root
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1' -ExePath (Join-Path $root 'build\Release\CADWorkbench.exe') -Python $python -AllowActiveDesktop -Only box_save_reopen,parametric_solid
```

For uninterrupted active-desktop testing, stay in an unlocked private environment; the app is interactive and will take focus. If a test still reports `AUTOMATION_BLOCKED`, attach the latest JSON report. Win32 handle/title details are included in the failure text where discovery cannot confirm a dialog.

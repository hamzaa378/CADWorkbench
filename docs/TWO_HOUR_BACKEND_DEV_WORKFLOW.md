# CADWorkbench: focused backend coding, then complete regressions

This workflow is for a developer who wants to code for about two hours before running the expensive Windows regressions. The time is a suggested work block, not a timer or a guarantee that software can run unattended. **Frontend functionality stays frozen**; the previously reported `contextGrid` redeclaration has already been fixed in this base snapshot.

## Preconditions

- Extract the new **full corrected source ZIP** into a fresh folder. Preserve your last passing Windows build separately.
- Windows: Visual Studio with the C++ workload, CMake/CTest, Qt/OpenCascade installed via the supplied vcpkg manifest at `C:\vcpkg`, Python 3.12 or compatible and PowerShell.
- Use the project directory containing `CMakeLists.txt` as the current directory.
- Do not run the existing 130 tests and Python suite as a substitute for the first clean source-manifest check.

## Start (clean Windows PowerShell)

```powershell
$downloads=Join-Path $env:USERPROFILE 'Downloads'
$zip=Join-Path $downloads 'CADWorkbench_v170_BackendBuildFix_2HourDevFoundation_PRELIMINARY_20261116_GitHub_Source.zip'
$dest=Join-Path $downloads 'CADWorkbench_Backend_2Hour_Workspace'
if(Test-Path -LiteralPath $dest){throw 'Choose a new destination to protect existing work.'}
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$root=Join-Path $dest 'CADWorkbench_v170_BackendBuildFix_2HourDevFoundation_PRELIMINARY_20261116'
Set-Location $root
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\START_TWO_HOUR_DEV_SESSION.ps1 -Label 'Topology, assembly or regeneration'
```

A clean manifest is checked and a session snapshot is written under `build/dev_sessions/`. It is ignored by Git and not part of the source package. The developer can now edit backend code without expensive test execution. `python tools/dev_session.py status --root .` shows modified files.

## End the coding block: mandatory complete regressions

```powershell
Set-Location (Join-Path (Join-Path $env:USERPROFILE 'Downloads') 'CADWorkbench_Backend_2Hour_Workspace\CADWorkbench_v170_BackendBuildFix_2HourDevFoundation_PRELIMINARY_20261116')
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\FINISH_TWO_HOUR_DEV_SESSION.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4 -SetupPythonEnv
```

`stage` records every edited, newly added or deleted **source** file, rejects UI modifications during this backend-only workflow, and refreshes both SHA256 source manifests together. This is an explicit developer step, not a way to evade integrity checks. The existing audit script still builds all native C++ targets, runs 130/130 CTests, runs the full Python/source suite, generates 60-scenario acceptance records, and returns **3** when all automated tests pass but manual acceptance is pending. The `finish` step requires verified JUnit/CTest/Python evidence written after the staged snapshot and prevents declaring success if the source changed after testing.

When native compilation fails, or any test fails, the wrapper stops and **retains the active development session**. Fix the error and rerun the finishing command. No tests are silently skipped, and no failure counts as success.

A passed session writes `build/dev_sessions/SESSION_REPORT.md` and `completed_session.json`. These reports are useful as development evidence but **do not confer engineering acceptance or manufacturing certification**. The native 130/130 and Python suite are required; manual scenarios remain a separate release gate.

## Practical coding habits

Work on one focused backend feature at a time; add a new regression assertion with the feature rather than retrospectively. Use small, reviewable units in `src/core/` and corresponding tests under `tests/`. Avoid modifying Qt UI while the frontend is frozen. Commit once the complete regression gate passes. Keep `build/`, `.venv*`, temporary project files and other generated binaries outside source manifests. Re-run the audit if *any source* changes after the staged run. A two-hour work block is a workflow preference, not permission to ship untested code.

## Limitations

There is no automatic Windows compilation in the Linux source-packaging environment. The backend source must still pass MSVC/Qt/OpenCascade builds on Windows. A failing GUI compilation cannot be declared successful because the standalone C++ or Python tests pass. Engineering acceptance remains 0/20 per workstream until recorded separately.

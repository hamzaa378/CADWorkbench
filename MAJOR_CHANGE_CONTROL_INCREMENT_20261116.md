# CADWorkbench v1.70 — Major-build readiness and engineering change control

**Preliminary source increment, 16 November 2026. Not a production release or a native Windows pass.**

## Implemented changes

1. **Fix confirmed MSVC build blocker:** In `tests/associative_features_v159_tests.cpp` rename the second `faceIndex` local to `witnessedFaceIndex`. This resolves the precise C2374/C2086 name collision reported in the November 15 Windows log. No regression is skipped or disabled.
2. **Per-item BOM reconciliation:** Beyond checking the overall total, compare the quantities of each BOM item against the actual occurrence items; fail closed if grouped counts disagree, even when total physical body counts match.
3. **Document audit schema fail-closed:** A handoff with an unknown audit severity now fails verification, instead of silently ignoring it.
4. **Engineering change-control assessment:** `tools/assess_change_control.py` independently verifies both UNISSUED handoffs, identifies CAD/STEP output divergence, impacted assembly occurrences, outstanding inspections, and creates a list of required review evidence marked NOT_REVIEWED. Never approves design automatically.
5. **Major-build source preflight:** `tools/preflight_major_build.py` checks the source integrity manifest and specifically guards against the known MSVC test declaration regression. It does not replace MSVC/CMake/CTest.

## Regression evidence in this environment

- 684/684 Python/source/reference tests passed, including six new targeted scenarios.
- 19/19 portable C++ executables passed.
- The full 130 registered native Windows CTests are **not claimed passed for this increment**.
- The 60 manual GUI acceptance scenarios are **NOT_RUN**.

## Complete clean Windows verification (PowerShell)

Extract the FULL source ZIP into a NEW directory. Do not overwrite any running Windows build or old baseline.

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$zip = Join-Path $downloads 'CADWorkbench_v170_ChangeControl_PRELIMINARY_20261116_GitHub_Source.zip'
$dest = Join-Path $downloads 'CADWorkbench_ChangeControl_MajorBuild_Test'
if (!(Test-Path -LiteralPath $zip)) { throw 'Full source ZIP missing from Downloads' }
if (Test-Path -LiteralPath $dest) { throw 'Destination exists; choose a new folder' }
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$root = Join-Path $dest 'CADWorkbench_v170_ChangeControl_PRELIMINARY_20261116'
Set-Location $root
py -3 .\tools\preflight_major_build.py --root .
if ($LASTEXITCODE -ne 0) { throw 'Prebuild source integrity failed' }
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Target: **130/130 native CTests**, **684/684 Python/source tests**, application builds and starts. Native success with 60 scenarios NOT_RUN may still return audit exit 3, not a production release gate pass.

## Engineering change control

After two independent UNISSUED handoff packages have been created and verified:

```powershell
py -3 .\tools\assess_change_control.py 'C:\path\old_handoff' 'C:\path\new_handoff' --json "$env:USERPROFILE\Downloads\change_review_new.json"
```

Never put the JSON output inside either immutable handoff folder; never overwrite an earlier review. The generated evidence states are NOT_REVIEWED, and geometry comparisons based on digests alone do **not** demonstrate geometric equivalence.

## Release gate

Continue preserving the last verified Windows Fix 2 baseline. No production installer, release approval, manufacturing sign-off, or 85% feature certification is implied by this snapshot.

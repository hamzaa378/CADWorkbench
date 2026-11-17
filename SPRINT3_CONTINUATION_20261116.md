# CADWorkbench v1.70 — Sprint 3 backend engineering and regression continuation

**Source checkpoint:** 2026-11-16. **Status:** PRELIMINARY SOURCE ONLY. **UI feature work remains frozen.**

## Start here / what is grounded

This **single combined source ZIP** extends the exact `BackendSprint2_CoreReliability_PRELIMINARY_20261116_GitHub_Source.zip` archive, not an inferred newer working tree. The uploaded Windows transcript `Pasted text(20261116-123230).txt` built `CADWorkbench.exe`, then ran 130 actual native CTests: **129 PASSED, 1 FAILED**. Failed case: `cadcore_universal_feature_tree` (CTest #45), check 70, "Witnessed unequal-size two-edge blend did not rebuild". Its failed full-audit exit is 1. Warnings about deprecated OCCT 8 types are not this failure. Previous versions that passed 130/130 are a **separate** known baseline, not evidence for this checkpoint.

The C++ root cause was found in *both* `Fillet selected edges` and `Chamfer selected edges`: `member_N_size` was incorrectly carried into the strict `resolveStableEdgeAnchor` witness record, which checks exact field cardinality. Both branches now extract `memberSize` before removing `edge_index` AND `size` from the anchor, then use the extracted value in the kernel builder. Duplicate physical edge detection, strict witnessed-edge resolution, validation, atomic rollback, undo/redo, and save/reload coverage remain intact. The native failing test now includes the operation type and error text if the group cannot be built. **The corrected C++ has not been rebuilt or run under Windows/MSVC in this environment**, so this fix is source-level + reference-kernel tested, *not* a verified Windows repair.

## Added backend engineering capabilities

1. `src/core/ToleranceRescuePlan.h`: uniform what-if tolerance tightening, preserving the exact nominal dimensions, direction of every chain term, and a user-supplied minimum manufacturing scaling factor. Computes the maximum allowable scale needed to meet a bilateral worst-case fit envelope, the corresponding revised tolerance terms, and an explicit cannot-meet decision when a nominal dimension is already outside bounds or manufacturing tightening is insufficient. Refuses deviation intervals that do not straddle nominal. Transactional output. This is **advisory calculation only**, not a shop-capability or quality-control certification. Depends on the existing `AssemblyToleranceBudget.h` and `WorkshopEngineering.cpp`.
2. `src/core/DrawingReferenceAudit.h`: deterministic revision consistency review of geometry and metadata tokens per annotation/source body. Detects deleted/unavailable bodies, stale geometry, stale metadata and invalid or duplicate identifiers; retains prior report on invalid input. Reports only **eligible for engineering review**, never issued/approved. Not yet wired into the UI or manufacturing export gate.
3. `tools/diagnose_build.py`: diagnoses the specific grouped-edge CTest failure with a source-focused hint, even when the full audit has other failed tests; a warning cannot be promoted to PASS. Source preflight now catches regression where per-member machining size is sent to strict edge resolution.

## Local, independently run verification — NEW source

| Gate | Result | Meaning |
| --- | --- | --- |
| Python/source/reference-kernel tests | **812/812 PASS** | Includes 6 new actual OCP unequal-size two- and three-edge kernel tests, 12 grouped-edge source contracts and 3 new failure-diagnostic cases |
| Portable C++ regression programs | **30/30 COMPILED + PASS** | Previously 25/25; 5 genuinely new executables covering tolerance what-if, drawings revision audit, 4,200 tolerance property assertions, 1,008 sheet-metal assertions and 2,500-reference drawing stress |
| Existing native Windows CTest inventory | **130 preserved** | Registration is intact; **NEW checkpoint NOT YET RUN on Windows** |
| New C++ MSVC + Qt + OpenCascade full build | **NOT RUN** | This Linux build environment does not supply the Windows production toolchain |
| Manual P/A/D engineering acceptance | **0/20 each NOT_RUN** | No release/95%-target claim |

Detailed local command outputs are in `qa/local_python_source_tests.log`, `qa/local_portable_cpp_tests.log`, and `qa/uploaded_sprint2_failure_diagnosis.json`. The latter is analysis of the **old failing Windows transcript**, not a log from the corrected source.

The original 130-test Windows gate was deliberately **not inflated by duplicating CTest names**. New C++ executable coverage grew from 25 to 30; Python inventory grew from 791 to 812. CMake includes both header-only engineering modules in `cadcore`. GitHub portable CI discovers the extra executables automatically.

## Self-contained Windows verification from a FRESH PowerShell

Save the combined ZIP in Downloads, then paste the commands in order. No existing working directory is assumed; nothing overwrites the older baseline:

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$archive = Join-Path $downloads 'CADWorkbench_v170_BackendSprint3_EngineeringQA_COMBINED_PRELIMINARY_20261116.zip'
if (!(Test-Path -LiteralPath $archive)) { throw "Missing archive: $archive" }
$destination = Join-Path $downloads 'CADWorkbench_Sprint3_Independent_Windows'
if (Test-Path -LiteralPath $destination) { throw "Choose a NEW destination: $destination" }
Expand-Archive -LiteralPath $archive -DestinationPath $destination
$root = Join-Path $destination 'CADWorkbench_v170_BackendSprint3_EngineeringQA_PRELIMINARY_20261116'
Set-Location -LiteralPath $root
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT3_WINDOWS_AUDIT.ps1 -VcpkgRoot 'C:cpkg' -Parallel 4
Write-Host "Audit process exit: $LASTEXITCODE"
```

The script preflights source hashes and the witness fix, configures/compiles the native Windows app with the existing vcpkg/Qt/OCCT setup, runs **all 130** CTests and the **812+** Python inventory, generates a comprehensive failure report, and then **replays the formerly failing CTest #45** for explicit diagnosis. Expected success behavior: 130/130 native passed; 812+ Python tests passed; `backend_diagnosis.json` says `AUTOMATED_PASS`. **Audit exit 3** means automated checks passed but manual engineering acceptance remains pending; an exit of 1 signals failure. Keep the console transcript and `build/v170_comprehensive_audit/` directory for the next iteration. A stand-alone focused CTest pass cannot override a failing full audit.

To run only the independent source and C++-portable checks (on machines with Python, g++ or clang++ and installed Python geometry requirements):

```powershell
py -3 -m unittest discover -s tests -p '*.py' -q
py -3 tools\run_portable_regressions.py --jobs 3
py -3 tools\verify_130_tests.py
```

For fresh Windows builds, vcpkg/Qt 6/OpenCascade 8, Visual Studio C++ build tools, CMake and Python are required. The audit's `-SetupPythonEnv` path installs the pinned project-local Python requirements; it does not change global packages.

## Release discipline and next backend sprint

* Do **not** replace the earlier fully verified baseline with this preliminary checkpoint before the user's **real** Windows build and 130/130 CTests pass. A source-level verification or OCP reference test cannot assert Qt `SceneDocument` save/rebuild correctness.
* Keep all UI feature work frozen. The two new APIs are forward-compatible *planning services*, not integrated drawing approval or shop-certified modules.
* Next priorities: prove #45's two native fillet/chamfer paths on Windows; run full audit; add OCCT lineage split/merge identity tests and failure injection around reconstruction; wire revision-gate checks into unissued drawing generation in a later separately tested step; expand motion/clearance and machining tolerance scenario verification. Report P/A/D 60-scenario results only from recorded manual evidence.
* Preserve the unchanged 130 native CTest registration contract until an **intentional**, end-to-end migration updates every hardcoded 130-test audit tool, Windows runner, Python contract and acceptance matrix. Increasing counts without updating the evidence tools would break the quality gate.

## Archive provenance

`SHA256SUMS.txt` and `SOURCE_MANIFEST_SHA256.txt` contain hashes of all delivery files except those two manifest files. Verify from the source root using `py -3 tools/preflight_major_build.py --root .`. The combined ZIP is the only requested delivery; no separate patch, debug-only package, or Win32 binary is implied.

# CADWorkbench v1.70 — Backend reliability, diagnostics & real engineering

**PRELIMINARY source candidate, 2026-11-16.** Frontend feature development remains frozen. No `src/ui/*` feature edits in this increment. The native UI build receives only target-scoped `/bigobj` through `CMakeLists.txt` to address reported MSVC C1128. Do **not** declare a Windows-verified release until the new build and native tests run there.

## What the user's Windows log establishes

The earlier `contextGrid` redeclaration error had been resolved. The new native compile reached `src/ui/MainWindow.cpp`, then failed with **error C1128** (`number of sections exceeded object file format limit: compile with /bigobj`). The complete native test inventory was **NOT RUN**, not 0 test failures. Source/Python tests completed; the full audit exited 1. Compiler C4996 and C4456 warnings did not cause this specific build failure. The user's proven automated 130/130 native baseline is still the earlier PersistentGeometry WinCTest47 version, *not* this new source branch.

## Implemented this increment

1. **MSVC unblock:** `CADWorkbench` target in `CMakeLists.txt` uses `/bigobj` exclusively with MSVC; no UI feature code changed. Preflight now requires that compile flag.
2. **Deterministic incremental rebuild reasons:** `FeatureRebuildDiagnostics.h/.cpp` is compiled into `cadcore` and the existing native feature-dependency test. It reports exact `REUSED`, `NEW_FEATURE`, `MISSING_CACHE`, `DEFINITION_CHANGED`, `DEPENDENCY_CHANGED`, `BASE_REVISION_CHANGED`, `EXTERNAL_REVISION_CHANGED`, and `UPSTREAM_RECOMPUTED` outcomes. A failed plan must leave the provided plan and explanations untouched. This is **planning only**, not proof of valid B-rep output.
3. **Engineering assembly fit tolerance budget:** `AssemblyToleranceBudget.h` composes the existing dimensional tolerance engine with explicit acceptable minimum/maximum fit limits, worst-case manufacturing margins, ranked asymmetric contributing dimensions, and a labeled advisory RSS half-width. Validates bad measurements / bounds, rolls back outputs on failure. Integrated in the existing workshop native tolerance CTest target and added as a portable C++ suite. No claim of statistical process capability or certified ISO fit.
4. **Build failure diagnostics:** `tools/diagnose_build.py --log <console-log> --out <json>` classifies C1128, C2374/C2086/C2371, C1001, C1060, link errors, CMake errors, CTest failures, Python failures, missing test evidence and explicit audit failures. It returns nonzero for incomplete/failed evidence and **does not mistake the synthetic `Native 130/130` Python audit fixture printouts for native CTest evidence**.
5. **Two-hour validation workflow:** `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1` wraps the original full build + 130 native CTests + all Python tests and emits `build/v170_comprehensive_audit/backend_diagnosis.json`. `FINISH_TWO_HOUR_DEV_SESSION.ps1` now invokes this wrapper before it can mark a coding block complete. `scripts/RUN_BACKEND_FAST_CHECKS.ps1` builds and runs the narrow dependency/tolerance native tests when CMake is already configured.
6. **Faster portable feedback:** `tools/run_portable_regressions.py --jobs 3` enables bounded parallel compilation and still fails if any one test fails; GitHub portable CI adopts it. No native Windows tests removed or skipped.

## Engineer/operator command sequence — from a new Windows PowerShell window

Download and unpack the **full new source ZIP** into an unused folder under Downloads; the folder inside the ZIP is the project root. Navigate to that project root. From there:

```powershell
py -3 .\tools\preflight_major_build.py --root .
# Must pass before any build; source archive manifests are checked.
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4 -SetupPythonEnv
Write-Host "Final automated audit exit: $LASTEXITCODE"
# 0 = automation and acceptance evidence; 3 = automated PASS but manual acceptance pending.
# Other exit = failure, inspect build\v170_comprehensive_audit\backend_diagnosis.json
```

On **existing modified checkouts**, run the original development session `stage` command before the audit to refresh the two source manifests. Never manually weaken expected results or skip registered CTests. The fast checker may be used during coding, but does **not** replace the full audit after coding.

## Next high-value two-hour coding-session backlog (run and test sequentially)

| Priority | Engineering deliverable | Required automated acceptance | What remains beyond 2h session |
| --- | --- | --- | --- |
| P0 | Boolean split/merge persistent face/edge witness conflict diagnostics | Real OCCT split/merge fixtures; ambiguous successors rejected; save/reopen, retarget, undo/redo | General automatic topology matching requires broader geometry cases |
| P0 | Source-linked mate frame reattachment on geometry revision | Mate references persist across edits; invalid face IDs fail without mutation | All mate types plus loop closure |
| P0 | Regeneration transaction isolation under multi-source edits | Failure injection between upstream child and downstream join; byte-identical rollback | Multi-threaded B-rep evaluation requires thread-safety review |
| P1 | Assembly tolerance chain to mating clearance cross-check | Guaranteed worst-case vs nominal; millimetres and sign correct; near-limit fixtures | Full geometry-coupled GD&T analysis |
| P1 | Motion trajectory sampled clearance and stop conditions | Joint limit, parameter limits, contact fixtures; deterministic schedules | Swept-envelope and dynamic safety certification |
| P1 | Associative drawing dimension stale-source gate | Revision-token mismatch refuses issued dimension; unaffected dims persist | Complete drawing drafting and standards |
| P1 | Multi-flange bend relief and flattened/folded net validation | OCCT refold volume and shape checks, error rollback | Shop-validated manufacturing rules |
| P1 | Large-model DAG incremental performance contract | Rebuild only affected branches on 1K and 10K-node fixtures | Real UI speed and OCCT memory measurement |
| P2 | C1128 long-term fix: split huge MainWindow translation unit | No feature regression and 130/130 Windows CTests | Held until frontend freeze is deliberately lifted |
| P2 | Geometry corpus and deterministic seed/serialization diagnostics | Reopen model, compare B-rep and source revisions | Real-world vendor CAD interoperability |
| P2 | Fault-injection test catalog and automatic repro bundle | Reproducible regression per cause and no tests hidden | Windows crash-dump triage |
| P2 | Engineering acceptance of 20 P / 20 A / 20 D scenarios | 19/20 each (95% goal) plus all critical scenarios | Independent domain review before production release |

The goal is **short, substantial backend coding passes followed by real regression evidence**, not artificial implementation percentages. Historical estimates P~85%, A~90%, D~81% are provisional; current manual scores P/A/D are all 0/20 NOT_RUN. A native pass is not signoff for real engineering use.

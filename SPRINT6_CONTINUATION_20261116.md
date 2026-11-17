# CADWorkbench v1.70 — Backend Sprint 6: topology, atomic regeneration, and drawing revision reliability

**Checkpoint:** 16 November 2026. **Delivery:** complete, combined **PRELIMINARY SOURCE** candidate; **NOT** a compiled Windows binary, accepted production release, or verified full engineering system.

## User requirements and baseline

This increment builds directly on Sprint 5 (motion safety). The user requests broad, detailed backend work on persistent face/edge identity through Boolean splits and merges, atomic feature-history recovery, revision-sensitive drawing references, with thorough error checks **before** a costly Windows build. UI feature development remains frozen; preserve existing working CAD, shape-history, assembly, sheet metal, and native test coverage.

Historical evidence supplied by user: the Sprint 2 Windows native build had 129/130 CTests passed, with `cadcore_universal_feature_tree` failing at the unequal-size two-edge blend witness (CTest #45). The Sprint 3 source contains a proposed fix; it has **not been re-verified on native Windows**. Sprint 5 is the baseline used here. All 130 CTest registrations are preserved in Sprint 6; **none was executed in this Linux editing environment**.

## Engineering changes, source files, invariants

### A. Topology identity through Boolean split/merge

- `src/core/ReferenceRouteConsensus.h`: cross-branch, kernel-independent reference-route agreement. Requires 1–32 valid routes, a finite physical design witness, non-empty named descriptor fields, matching field keys, equal discrete topology IDs and numerically consistent continuous descriptors. Invalid, missing, ambiguous and nonfinite witnesses **fail closed**. Implements a diagnostic without mutating the model.
- `src/core/SceneDocument.cpp`: real interleaved history replay now calls `lineage::agreeReferenceRoutes` across independently resolved routes. It cannot accept divergent branch evidence as the same persistent edge or face.
- `src/core/BooleanReferenceLineage.h/.cpp`: `rebaseFeaturesThroughBoolean` takes a batch of up to 40 downstream features through the same Boolean history and stages every rebound. Detects duplicate/missing feature IDs. If *any* descendant cannot be proven from the final OCCT B-rep and the original witnessed location, **the caller's whole output remains unchanged**. Supports the selected edge and face categories already recognized by the single-feature OCCT rebase path. This is not a universal topological naming theorem; ambiguous/deleted topology still needs user re-selection.
- `tests/face_lineage_v170_tests.cpp`: native OCCT test cases added for dual edge/face rebase on a real Boolean and output preservation for bad/duplicate references. Requires native build and execution to verify.

### B. Atomic feature-history rebuild and preview

- `src/core/AtomicRebuildGuard.h`: independent commit-time journal invariant checker. Preserves body count and identity; rejects duplicate IDs, missing roots, unchanged revision tokens applied to changed B-reps, and inconsistent staged bodies. The public changed-ID output is updated only on success.
- `src/core/SceneDocument.h/.cpp`: a shared detached `stageShapeFeatureTreesBatch` rebuilds every root and dependent without modifying the live scene; copies required persistent and articulated-mate context; validates the combined postconditions; `previewShapeFeatureTreesBatch` returns a **read-only** preview of changed bodies without saving an undo checkpoint; `setShapeFeatureTreesBatch` commits once after staging every root and dependent. Failed regeneration is discarded, preserving the previous model and undo history. Source leaves existing per-root `setShapeFeatureTree` behavior unchanged.
- `tests/shape_feature_tree_tests.cpp`: native cases for valid read-only preview, invalid rollback, null output destination, and unchanged geometry revisions. Actual OCCT/Qt run is pending.

### C. Revision-sensitive drawings

- `src/core/SceneDocument.h/.cpp`: `DrawingSheetSourceStatus` now reports specific `staleAnnotationIds`; both saved edge dimensions and manual GD&T frames must reference the **current source body geometry revision**. Refreshing the sheet's aggregate geometry and metadata fingerprints **does not** silently approve annotation-level stale references.
- `src/core/DocumentAudit.cpp`: emits blocking `DRAWING_ANNOTATION_REVISION_STALE` findings with annotation IDs and remediation instructions. Existing whole-sheet staleness and missing source-body findings remain.
- `tests/document_audit_tests.cpp`: native regression checks translated geometry, aggregate-refresh attempt, a still-stale GD&T source, audit error, and undo. Actual native execution is pending.
- Stale drawings require engineering review/re-attachment; the software does not automatically prove a dimension still applies or re-issue signed manufacturing drawings.

## Pre-build detection, regression expansion

- Added two freestanding portable C++ test programs, `tests/reference_route_consensus_portable_tests.cpp` (180,017 assertions) and `tests/atomic_rebuild_guard_portable_tests.cpp` (75,014 assertions). Both passed independently with **GCC** and **Clang**, strict C++20 warnings enabled.
- `tools/run_portable_regressions.py` now registers **35 standalone compiled executables** (from 33), all passing for both GCC and Clang. Note standalone C++ executables are **not** the same as Qt/OpenCascade native CTests.
- Added `tests/test_sprint6_integrated_source_contract.py` and updated prior source contracts for the refactored paths. Full Python/source/OpenCascade-reference suite: **859/859 passed**, up from 847.
- `tools/audit_full_source.py`: checks all Python source syntax, CMake-referenced files, source integrity, case-folded filenames, merge conflicts, portable-test list, and 130 native test registration preservation. **395** source/script files and **83,652** lines scanned; **no static audit problems** on the completed source.
- `.github/workflows/portable-regressions.yml`: adds a second independent Clang job in addition to GCC.
- `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1`: expected full Python test inventory updated to 859.
- `RUN_SPRINT6_WINDOWS_STAGED_BUILD.ps1`: Windows-only fail-fast integration script. It runs manifest/static/source tests **before** configure, builds `cadcore` and the four directly relevant native regression binaries **before** the large GUI translation unit, then runs four focused native CTests. Unless `-TriageOnly` is selected, proceeds to the full existing Windows audit. `-TriageOnly` intentionally returns code 4 after a successful *partial* triage so it cannot be confused with a full release pass. This runner is not tested in Windows here.
- `CMakeLists.txt`: new headers explicitly listed; **130 unique native CTest registrations retained**. No saved-project schema migration and no new UI features.

### Local test evidence (Linux editing environment)

| Check | Verified result | Source evidence |
| --- | --- | --- |
| Python/source/reference suite | **859/859 PASS** | `qa/sprint6_python_all.log` |
| Compiled portable GCC suites | **35/35 PASS** | `qa/sprint6_gcc_35.log` |
| Compiled portable Clang suites | **35/35 PASS** | `qa/sprint6_clang_35.log` |
| Static full-tree inspection | **PASS**, 395 files, 83,652 lines | `qa/sprint6_static_audit.json` |
| Full native Qt/OpenCascade/MSVC build | **NOT RUN** | Deferred to user's consolidation gate |
| 130 Windows native CTests | **NOT RUN** | Only CMake registration checked |
| Manual engineering acceptance | **0/60 NOT_RUN** | Parametric, assembly, drawing/sheet-metal cases require human review |

**Critical caution:** historical synthetic audit-fixture strings inside Python test output may include `Native 130/130 passed`. They are fixture text, **not** real CTest evidence. The only honest native verification status for this candidate is NOT RUN.

## Risk review / limitations

1. `src/core/SceneDocument.cpp` remains about **13,778 lines**: high compilation and change-risk hotspot. The earlier `/bigobj` risk for `MainWindow.cpp` is addressed in CMake source but not rechecked with MSVC after this increment. Major native compilation gates cannot be simulated using portable tests.
2. Topology references intentionally fail closed if multiple Boolean candidates cannot be distinguished. Arbitrary topology name persistence across all Boolean/fillet/shell/sweep variations is not solved. Need actual OCCT regression fixtures for chained split->merge->fillet and real-world geometry.
3. The atomic batch preview verifies staged B-reps and revision IDs, not all downstream drafting/mate semantics. No proof of automatic face identity across arbitrary user edits.
4. Drawing freshness checks source revision tokens, **not** exact reattachment of a displaced edge after re-topology. Manual review still required.
5. Checkpoint cancellation, external process termination, disk I/O failure and OS crash are distinct from in-memory transactional rollback and are not covered by this new API.
6. Current baseline native regression #45 from Sprint 2 must be executed and must pass; do not infer that a source patch is sufficient.
7. Native engineering release gate stays closed until 130/130 native CTests, full source suite, artifact hashes and manual acceptance have real evidence.

## Fast, complete Windows verification from FRESH PowerShell

Place the **combined Sprint 6 ZIP** in Downloads. Start a **new PowerShell** session. Use a new destination, never overwrite the last passing baseline.

```powershell
$downloads=Join-Path $env:USERPROFILE 'Downloads'
$zip=Join-Path $downloads 'CADWorkbench_v170_BackendSprint6_TopologyRecovery_COMBINED_PRELIMINARY_20261116.zip'
$dest=Join-Path $downloads 'CADWorkbench_Sprint6_Windows_Verification'
if(!(Test-Path -LiteralPath $zip)){throw "Archive missing: $zip"}
if(Test-Path -LiteralPath $dest){throw "Destination exists; select a clean test directory: $dest"}
Expand-Archive -LiteralPath $zip -DestinationPath $dest
Set-Location (Join-Path $dest 'CADWorkbench_v170_BackendSprint6_TopologyRecovery_PRELIMINARY_20261116')
# Expected prerequisites: VS 2022/MSVC, CMake, Qt, and the matching vcpkg manifest in C:\vcpkg.
# Fast prebuild source/manifest check, then native backend-only verification:
py -3 .\tools\preflight_major_build.py --root .
if($LASTEXITCODE -ne 0){throw 'Preflight failed'}
py -3 .\tools\audit_full_source.py --root .
if($LASTEXITCODE -ne 0){throw 'Source audit failed'}
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT6_WINDOWS_STAGED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 2
Write-Host "Native audit exit code: $LASTEXITCODE"
# If the full native audit fails, inspect: build\v170_comprehensive_audit\backend_diagnosis.json
```

**Exit semantics:** full audit exit 0 = pass including available signoff evidence; exit 3 may mean all automated checks passed but manual engineering evidence remains pending; all failures and incomplete logs remain failures. For `-TriageOnly`, exit 4 means only targeted native checks were completed and is intentionally not a full-pass signal.

## Next release-gate actions

1. On Windows: execute fail-fast native compile and focused CTests, then whole build + all 130 CTests. Capture complete build/CTest logs, not a 10-line summary.
2. Check OCCT split-merge history on named production-like solids; measure face/edge retention, conflicting candidates and undo persistence. Test revised source geometry plus dependent sketch, assembly mates, and drawing dimensions.
3. Manual acceptance: execute 20 parametric, 20 assembly, and 20 drawing/sheet-metal scenarios with screenshots/artifacts and pass/fail evidence; historical criterion is >=19/20 *per track* plus all critical cases before claiming >=95%.
4. Only after real Windows verification, consider splitting `SceneDocument.cpp` into compilation units, keeping public API and serialization stable. Preserve validated rollback baseline.

The complete source ZIP is the single handover artifact; all original Sprint 5 files are preserved, and changed files and test logs are contained in that ZIP.

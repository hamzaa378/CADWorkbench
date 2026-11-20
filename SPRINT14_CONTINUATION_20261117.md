# CADWorkbench v1.70 — Sprint 14 coordinated authoring and drawing recovery

**Release classification: PRELIMINARY SOURCE — Windows MSVC/Qt/OpenCascade not yet executed for Sprint 14.** The last Windows-automated-verified user baseline remains Sprint 11 (`CADWorkbench.exe`, 130/130 native CTests, 952/952 Python tests). Sprint 12–14 native builds have not been substantiated. Manual engineering acceptance remains **0/60 formally recorded**, all 18 critical workflows require real UI evidence. Do not mark manufacturing outputs production-approved.

## Deadline allocation and actual scope

The requested **70% integration / 20% reliability & performance / 10% missing capabilities** split is an effort emphasis, not a claim that these percentages of the product are complete. Rather than more unconnected geometry helpers, Sprint 14 integrates existing production B-rep regeneration and sheet references into authoring workflows.

### 1. Coordinated parametric edit, evaluated-alternative promotion (integration + performance)

- `SceneDocument::previewNamedParameterBatch` now uses `setCoordinatedParametricRevision` when there are 2–12 sources. This stages **all final parameter changes before rebuilding linked descendants**, and checks actual OCCT solids/volumes. One-source requests retain `setNamedParameters` semantics. A proposal that only becomes geometrically valid after two simultaneous changes can be assessed without a transient invalid intermediate body.
- `SceneDocument::setNamedParameterBatch` uses the same final-state transaction for 2–12 bodies and single-source parameter editing for one body. It no longer performs redundant full preview + N sequential commits + another preview/commit. The target engine applies one checkpoint after the final valid geometry and descendant propagation. **No measured timing speedup is claimed.**
- `SceneDocument::designGeometryFingerprint()` SHA-256 includes each body's ID, saved B-rep geometry revision token, current parametric token, active saved configuration, parameters, formulas, dimension links, and suppressed features. A formula-only change cannot silently reuse an old evaluated alternative just because its B-rep shape is unchanged.
- `SceneDocument::applyParametricDesignAlternative` takes the chosen `ParametricDesignAlternative` and the fingerprint recorded at evaluation time. Refuses invalid/unnamed/empty/stale candidates, calls existing checked single/dual/multi source regeneration, and never edits live bodies on failure.
- `MainWindow::exploreParametricDesignAlternatives` now records the fingerprint before evaluation and uses this checked commit path only after the existing confirm dialog. Linked dependent solids and persisted geometry revisions remain protected by the production engine.

**Native tests**: `tests/cadcore_tests.cpp` adds accepted alternative → changed dependent, stale fingerprint refusal, undo/redo returning to the original geometry fingerprint. The tests run inside the existing `cadcore_parametric_foundation` CTest; its CTest name is unchanged.

### 2. Atomic cross-sheet drawing recovery (integration + reliability)

- Shared `prepareReviewedDrawingEdge` applies the existing **real OCCT stable-edge witness resolution and projected measurement preview**, including special handling for full-circle references; it refuses a stale index-only legacy link. This helper is used by both the old one-at-a-time API and the new batch API.
- `SceneDocument::previewDrawingEdgeReviewBatch` is read-only. `SceneDocument::reviewDrawingEdgeReviewBatch` reviews selected witnessed edge dimensions across **1–40 saved drawing sheets / at most 200 distinct annotations**, first validating *every* reference. Failure of any one prevents every commit; success creates exactly **one undo checkpoint**. A fully unchanged selection creates no checkpoint. It never sets a GD&T review token or blanket source revision.
- The detached proposal now copies the entire scene only **once per batch**, rather than once per dimension. Expensive geometry checks execute only when an engineer explicitly requests review.
- `SceneDocument::buildDrawingRecoveryCsv` and `exportDrawingRecoveryCsv` generate a cross-sheet list of sheet, edge-dimension and GD&T revision states, including missing bodies and annotation review needs. This is spreadsheet-safe and exported using `QSaveFile`; tokens alone never prove geometric or human verification. It reports **NOT ISSUED**.
- File menu: **Review witnessed drawing edge revisions across sheets...** (scans one selected sheet or all saved sheets; fails closed if any witness invalid, asks before one undoable commit) and **Export all saved drawing revision issues CSV...**. Both use the existing backend methods.

**Native tests**: `tests/document_audit_tests.cpp` adds two saved sheets with edge, legacy index-only and GD&T annotations, edited source solids, failed multi-sheet preview and commit rollback, successful witnessed-only review, single undo/redo, two save/reopen cycles and recovery CSV export. The tests run inside the existing `cadcore_document_audit` CTest.

### 3. Source integrity, build gates, regression inventory

- New `tests/test_sprint14_integrated_authoring.py` has 16 source/integration contracts for the exact new wiring, stale-guard, copy-once, limits, approval boundaries and native test presence. Old Sprint 8 and three-workstream source-contract tests were updated to verify the shared geometry checker / one-pass coordinated engine rather than relying on the old inefficient code layout. Tests were not omitted.
- The original 130 native CTest registrations and `tests/BASELINE_130_NATIVE_CTEST_NAMES.txt` remain unchanged. All older source, test, QA evidence and documentation files from Sprint 13 are retained.
- `RUN_SPRINT14_WINDOWS_TARGETED_BUILD.ps1` performs source manifest preflight, static audit, CTest inventory check, new Python contract tests, CMake configure, and compiles `cadcore`, `cadcore_tests`, `document_audit_tests`, `shape_feature_tree_tests`, **and CADWorkbench.exe** (to catch MainWindow.cpp integration). It runs focused `cadcore_document_audit`, `cadcore_parametric_foundation`, `cadcore_v170_atomic_batch_edit`, `cadcore_universal_feature_tree`. A targeted **exit 4** means only those checks succeeded. With `-FullAudit`, it additionally runs the complete application/full 130 native CTests, entire Python suite, evidence classifier and manual gate through the existing wrapper.
- The full regression diagnostics' expected Python count has been updated to **1,030**. `tools/audit_full_source.py` correctly flags the large GUI unit as a maintainability/compilation risk, not as frozen code.

## Locally executed (Linux; not native Windows verification)

- `python -m unittest discover -s tests -p '*.py' -q`: **1,030/1,030 PASS** (some negative-test fixture output prints 'FAIL:' intentionally; unittest's final result is `OK`).
- `python tools/run_portable_regressions.py --compiler g++ --jobs 4`: **42/42 PASS**.
- `python tools/run_portable_regressions.py --compiler clang++ --jobs 4`: **42/42 PASS**, separately completed.
- `python tools/audit_full_source.py --root .`: PASS, no reported source-path, CMake registration, syntax or CTest integrity problems.
- `python tools/verify_130_tests.py`: PASS.
- **Not executed here:** MSVC/Qt/OpenCascade compile of new methods, native CTest behaviors or actual GUI manual acceptance. GCC/Clang portable tests are not a substitute for these gates.

## Exact Windows commands (fresh PowerShell; needs a downloaded Sprint 14 ZIP)

```powershell
Set-Location "$env:USERPROFILE\Downloads"
$zip = 'CADWorkbench_v170_Sprint14_CoordinatedCAD_Integration_GitHub_Source_PRELIMINARY_20261117.zip'
if (!(Test-Path -LiteralPath $zip)) { throw 'Sprint 14 ZIP is missing' }
$dest = Join-Path (Get-Location) ("CADWorkbench_Sprint14_Test_" + (Get-Date -Format 'yyyyMMdd_HHmmss'))
Expand-Archive -LiteralPath $zip -DestinationPath $dest
Set-Location (Join-Path $dest 'CADWorkbench')
$log = Join-Path "$env:USERPROFILE\Downloads" 'CADWorkbench_Sprint14_Targeted.log'
Start-Transcript -LiteralPath $log -Force
try {
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT14_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4
  $exit = $LASTEXITCODE
} finally { Stop-Transcript }
Write-Host "TARGETED EXIT: $exit | LOG: $log"
# Only after targeted compile/tests pass, run full release automated gate:
# powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT14_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4 -FullAudit
```

Do **not** rerun tests on top of Sprint 11's source/build directory. Preserve Sprint 11's build + audit as rollback until Sprint 14's full Windows gate is successful. The full audit can exit `3` when all automated gates pass while manual scenarios remain unrecorded.

## Known limitations and next highest-value work

- The revised multi-sheet API reviews only edge dimensions with verified captured geometric witnesses. It does **not** reattach arbitrary complex face/edge topology or review GD&T truth, drawing validity, or issued fabrication drawings.
- Concurrent C++ code editing during a user's build is unsupported. Make subsequent changes in a new extracted/project tree.
- The native `SceneDocument.cpp` (~14k lines) and the UI translation unit are high-cost MSVC build risks. Refactor shared state, persistent topology, drawing pipelines and revision services only after the new build baseline is confirmed.
- Missing end-to-end validation: exact rotation-aware collision/clearance, advanced assembly mate UI, true geometric refold roundtrips, larger sheet-metal corner relief/cutback networks, robust associative drawing update when an anchor disappears, STEP/DXF third-party validation, 60 manual UI acceptance tests.
- No feature is manufacturing-certified. Review packages remain unissued. Formal acceptance target remains at least 19/20 passed in each of parametric, assembly and drawings/sheet-metal, with all critical scenarios passed.

## Ownership and file inventory

Changed: `src/core/SceneDocument.cpp`, `src/core/SceneDocument.h`, `src/ui/MainWindow.cpp`, `tests/document_audit_tests.cpp`, `tests/cadcore_tests.cpp`, `tests/test_sprint8_topology_drawing_contract.py`, `tests/v170_large_workflow_expansion_contract.py`, `tests/v170_three_workstream_batch_package_contract.py`, `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1`, `tools/audit_full_source.py`, `README.md`, `GITHUB_UPLOAD_GUIDE.md`.

New: `tests/test_sprint14_integrated_authoring.py`, `RUN_SPRINT14_WINDOWS_TARGETED_BUILD.ps1`, this continuation guide, and `qa/SPRINT14_LOCAL_VERIFICATION_20261117.txt`.

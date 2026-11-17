# CADWorkbench v1.70 — Sprint 7 backend continuation

**Checkpoint:** preliminary source only, 16 November 2026. **Native Windows/Qt/OCCT compilation was deliberately deferred**. The previous Sprint 6 ZIP remains the rollback source. Do not overwrite the known passing Windows executable merely because this source passes portable tests.

## Scope and real defects corrected

### 1. Direct machining was bypassing dependent-geometry rebuild

The prior `SceneDocument.cpp` committed several direct machining results with `checkpoint()` followed by direct replacement in `parts_`. A linked descendant could retain old geometry and revision stamps after editing its parent. A failed descendant rebuild could not roll back because no attempt was made to rebuild it at all.

**Correction:** one shared `SceneDocument::commitMachiningShape(...)` validates source identity and expected revision; refuses editing linked Booleans, linked sketches, in-place results and associative flat blanks; checks positive valid OCCT shape and **measurable net material removal**; stages body changes in a separate vector; calls `propagateLinkedBodies` BEFORE `checkpoint()`; and publishes source and descendants together only after successful regeneration. Recoverable OCCT and C++ exceptions now generate an error instead of bubbling out of this path.

The 12 corrected entry points are:

- Single operations: `commitDovetailGroove`, `commitTSlotGroove`, `commitRoundedSlot`, `commitKeyholeSlot`, `commitRoundedSlotPattern`.
- Repeated machining: `holePattern`, `circularHolePattern`, `circularCountersinkPattern`, `circularCounterborePattern`, `linearCounterborePattern`, `linearCountersinkPattern`, `linearKeyholeSlotPattern`.

Fillet, chamfer and single-hole commits already staged descendants in the previous baseline and were deliberately left unchanged.

**Remaining limitation:** the volume test confirms material is removed **in net**; it is not a full geometric proof that the result is a Boolean subset of the source. Positive native CTest behavior and actual engineering scenarios are still required.

### 2. Feature-batch drawing impact preview

`SceneDocument::previewShapeFeatureTreesBatch(...)` now calculates new sheet-level and per-annotation stale status on its **detached** staged scene. Report fields: `newly_stale_sheets`, `newly_stale_annotations`, `review_sheet_ids`, and `review_annotation_ids`; the prior roots/changed/body_ids/READ_ONLY fields remain. This does not reattach geometry, acknowledge stale drawing links or issue a manufacturing drawing.

### 3. Native regression expansion WITHOUT removing legacy tests

`tests/machining_transaction_v170_tests.h` defines OCCT-backed cases for five different direct machining commits, stale and invalid preview rejection, forbidden editing of derived bodies, failed intersection-dependent regeneration preserving original revision state, linked-child propagation and undo/redo, and a repeated hole-grid operation. These functions are called from the **existing** `cadcore_dovetail_groove` and `cadcore_circular_hole_pattern` CTest routes in `tests/cadcore_tests.cpp`.

**All 130 original native test names remain unchanged.** A proposed separate 133-test registration was rejected because 53 historical source tests and multiple release/audit gates explicitly require the original 130 registrations. Their constraints have not been disabled or weakened. The new native cases require an actual Windows build to run.

### 4. Low-cost native build triage

`RUN_SPRINT7_WINDOWS_STAGED_BUILD.ps1` runs Python source validation, preflight, CMake configuration, core and selected native test target builds, then focused topology, drawing, interleaved history, Dovetail and hole-pattern CTests **before compiling the full GUI**. Use `-TriageOnly` to stop after the early native tests (exit 4 denotes expected triage-only completion, not full release signoff). Without `-TriageOnly` it invokes the existing comprehensive regression workflow.

## Actual local evidence

- Python/source tests: **875/875 passed**, up from 859 in Sprint 6. Sixteen new static contracts are in `tests/test_sprint7_transaction_contract.py`.
- Portable C++: **35/35 executables passed** independently with GCC and Clang (both run from the final working source). These are kernel-independent tests; they do not compile `SceneDocument.cpp`.
- Full static inventory: **397 source/build/test files, 83,930 lines**, with zero reported missing references, merge markers or Python syntax defects.
- Native CTest registration: **130/130 original names present**; **NOT executed** on this Sprint 7 candidate.
- Manual engineering acceptance: **0/60 completed**, not overridden by any source test.
- In the historical 2026-11-16 Windows Sprint 2 log, `cadcore_universal_feature_tree` failed on grouped unequal-size edge blends; no new native evidence resolves that known issue.

These assertions are limited to the categories implemented by the static audit; they do **not** prove all 83,930 lines compile or that all geometry operations are correct.

## Files modified / created

Modified: `src/core/SceneDocument.cpp`, `src/core/SceneDocument.h`, `tests/cadcore_tests.cpp`, `tools/audit_full_source.py`.
Added: `tests/machining_transaction_v170_tests.h`, `tests/test_sprint7_transaction_contract.py`, `tests/BASELINE_130_NATIVE_CTEST_NAMES.txt`, `RUN_SPRINT7_WINDOWS_STAGED_BUILD.ps1`, this continuation guide and `SPRINT7_QA_EVIDENCE/` logs. All files from Sprint 6 are preserved; updated SHA256 manifests intentionally replace the previous hashes.

## Windows verification from a FRESH PowerShell session (not yet executed)

1. Put `CADWorkbench_v170_BackendSprint7_ReferenceIntegrity_COMBINED_PRELIMINARY_20261116.zip` in Downloads.
2. Open a new PowerShell window and run:

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$zip = Join-Path $downloads 'CADWorkbench_v170_BackendSprint7_ReferenceIntegrity_COMBINED_PRELIMINARY_20261116.zip'
$dest = Join-Path $downloads 'CADWorkbench_Sprint7_FreshTest'
if (!(Test-Path -LiteralPath $zip -PathType Leaf)) { throw "ZIP missing: $zip" }
if (Test-Path -LiteralPath $dest) { throw 'Choose an unused extraction directory' }
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$root = Join-Path $dest 'CADWorkbench_v170_BackendSprint7_ReferenceIntegrity_PRELIMINARY_20261116'
Set-Location $root
py -3 .\tools\preflight_major_build.py --root .
if ($LASTEXITCODE -ne 0) { throw 'Preflight failure' }
# Earliest C++ integration verification: exits 4 deliberately in triage-only mode
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT7_WINDOWS_STAGED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4 -TriageOnly
Write-Host "Early compile/test exit: $LASTEXITCODE (4 is expected for completed triage)"
# When ready for the COMPLETE build, rerun without -TriageOnly:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT7_WINDOWS_STAGED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4
Write-Host "Full audit exit: $LASTEXITCODE"
```

The **early targeted native build is not optional evidence for a new Windows release**. For release candidacy, run all 130 native CTests, Python tests, and 60 manual P/A/D acceptance cases; inspect the full audit and preserve the last known passing Windows binary until completed.

## Next engineering priorities

- Close source-derived B-rep transaction gaps in the remaining legacy direct geometry commit paths only after a targeted native harness demonstrates incorrect behavior.
- Add stronger tolerance/shape-containment checks for milling operations with real OCCT test evidence; avoid claiming subset provenance from volume alone.
- Improve face/edge identity reattachment through ambiguous Boolean splits/merges with explicit reject/reselect paths rather than nearest-looking geometry.
- Add native drawing revision impact cases that edit saved annotations, preview feature batches, undo, redo and save/reopen the drawing.
- Break down large `SceneDocument.cpp` only with an MSVC build immediately following each extraction; casual refactoring of 13k+ lines is high regression risk.

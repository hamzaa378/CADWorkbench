# CADWorkbench Sprint 13 — Deadline Engineering Integration

**Status:** COMPLETE LOCAL SOURCE CANDIDATE. Sprint 11 is still the LAST ACTUALLY VERIFIED WINDOWS baseline: 130/130 native CTests and 952/952 Python/source tests. Sprint 12 and Sprint 13 have NOT been compiled under MSVC in this environment. No previous native CTest was removed or disabled. Engineering acceptance remains 0/60 NOT_RUN.

## Deadline priority / effort allocation

70% integrated manufacturing engineering, 20% fault containment and output verification, 10% missing independent native export verification. Focused development here is *not* proof of completed industrial CAD functionality.

### Engineering integration implemented

- **Independent source-linked flat-pattern development guard:** `src/core/LinkedFlatDevelopmentAudit.h` recalculates neutral bend-station positions, allowances, total length, width, reversal count and bend-center spacing from the saved folded definition. `SceneDocument::buildLinkedFlatDevelopmentSet()` now applies it before returning SVG/DXF/CSV; previously this code checked only visual and manufacturing outputs against each other. All outgoing buffers stay unchanged on any discrepancy. Actual saved folding and material effects remain subject to shop validation.
- **Single-part package verifier:** `tools/verify_linked_flat_package.py` accepts the *actual* `exportLinkedFlatManufacturingPackage()` directory format. It checks exact filenames, file-size/symlink bounds, SHA-256 inventory, saved source/linked revisions, independent neutral development, per-bend CSV with deductions and directions, DXF outline and bend-line geometry including reliefs, and SVG outline and bend paths. Rejects altered geometry even if its enclosing checksum manifest has been recomputed. User-entered bend-center spacing limit generates review findings, not a falsely certified plan.
- **Multi-part manufacturing batch verifier:** `tools/verify_linked_flat_batch.py` checks the native `exportLinkedFlatManufacturingBatch()` directory schema, 2–24 unique nested parts, per-file checksum and identity joins, stock sheets/placements, blank/relief area and cut-length arithmetic. Rectangular stock jobs additionally check gap and envelopes; contour-aware stock plans are treated as preliminary and require human toolpath/overlap verification rather than pretending the rectangular envelope test is sufficient.
- **Engineering handoff linkage:** `tools/engineering_readiness_workflow.py` accepts repeatable `--flat-package` folders and `--flat-batch`, joins their actual folded source IDs and known geometry revisions against the separately verified CAD handoff, emits distinct actions for uncertain source revisions, missing flat bodies, bend-spacing limits and mandatory forming review. All output remains UNISSUED; no measurements or drawing notes are auto-approved.
- **One external evidence bundle:** `tools/engineering_evidence_bundle.py` includes `sheet_metal_review.json` and cross-links it to `readiness.json`, preserving the existing atomic package publication. The verifier checks logical consistency of the action CSV, action count, source reported state and sheet-metal summary in addition to SHA-256 bytes. `tools/verify_engineering_evidence.py` automatically inherits these checks. Output remains outside all native source handoffs.

### Reliability and tests

- New 9,000-check native independent linked-flat consistency regression, built with GCC and Clang and integrated into the EXISTING Windows `cadcore_sheet_metal_flat_pattern` test path through `tests/cadcore_tests.cpp`.
- New Python source/geometry tests build real-format (but synthetic) native package fixtures for normal/relieved DXF/SVG/CSV, tampering even after rehashing, invalid tokens, bend ordering, both stock-backed and nonstock batches, semantic action evidence tampering and read-only behavior.
- 130 historic native CTest registrations unchanged. `RUN_SPRINT13_WINDOWS_TARGETED_BUILD.ps1` checks the inventory and compiles `cadcore`, `cadcore_tests`, `document_audit_tests`, workshop and topology targets, then runs four focused CTests. Use `-FullAudit` for full `CADWorkbench.exe`, all 130 CTests and Python suite.
- Static audit and both compiler portable suites were run locally. Native MSVC C++/OCCT and GUI still require user Windows run. Do not promote before that. Manual engineering acceptance is still unverified.

## Actual usage: native CAD → manufacturing shop review

1. In CADWorkbench, create/edit a *saved* folded sheet-metal body and linked flat blank. Export individual `exportLinkedFlatManufacturingPackage()` or a two-or-more-part `exportLinkedFlatManufacturingBatch()` job. Keep the unissued native package bytes unchanged.
2. Export the corresponding saved native UNISSUED manufacturing handoff with the folded sources included.
3. In PowerShell from a checked-out source folder, run for one or multiple individually exported flats:

```powershell
py -3 .\tools\verify_linked_flat_package.py 'C:\CAD\FoldedFlat1' --minimum-center-spacing-mm 10
py -3 .\tools\engineering_readiness_workflow.py 'C:\CAD\CurrentHandoff' --flat-package 'C:\CAD\FoldedFlat1' --json 'C:\CAD\Review\readiness.json'
py -3 .\tools\engineering_evidence_bundle.py 'C:\CAD\CurrentHandoff' 'C:\CAD\NewEvidenceBundle' --flat-package 'C:\CAD\FoldedFlat1'
```

For an exported batch:

```powershell
py -3 .\tools\verify_linked_flat_batch.py 'C:\CAD\SheetJob' --minimum-center-spacing-mm 10
py -3 .\tools\engineering_evidence_bundle.py 'C:\CAD\CurrentHandoff' 'C:\CAD\NewEvidenceBundle' --flat-batch 'C:\CAD\SheetJob'
py -3 .\tools\verify_engineering_evidence.py 'C:\CAD\NewEvidenceBundle'
```

Both evidence tools reject invalid or inconsistent source geometry and publish no partial report on errors. Outputs are not CMM results, flat patterns cannot prove springback/material formability, and verified checksums do not authenticate their creators.

## Windows verification

Use Visual Studio 2026 with MSVC, CMake, Qt 6/OpenCascade 8 via C:\vcpkg (same environment as Sprint 11). From **fresh extracted project root**:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT13_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot C:\vcpkg -Parallel 4
# Targeted success exit 4 is NOT a full build success.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT13_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot C:\vcpkg -Parallel 4 -FullAudit
```

The complete audit remains the only native automated gate; 60 manual engineering scenarios are a separate uncompleted gate. Keep the user's Sprint 11 compiled executable, dependencies and audit evidence untouched. Do not reuse an earlier build directory for a different ZIP.

## Files and continuation ownership

Native: `src/core/LinkedFlatDevelopmentAudit.h`, `src/core/SceneDocument.cpp`, `tests/cadcore_tests.cpp`, `tests/linked_flat_development_audit_portable_tests.cpp`. Python: `tools/verify_linked_flat_package.py`, `tools/verify_linked_flat_batch.py`, `tools/engineering_readiness_workflow.py`, `tools/engineering_evidence_bundle.py`, `tests/test_sprint13_flat_production_workflow.py`. Configuration: `CMakeLists.txt`, `tools/run_portable_regressions.py`, `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1`, and new Windows targeted launcher.

**Remaining major gaps:** Actual refold geometry compared to B-rep, geometric stock contour nesting proof, kerf/grain/tool access, real CMM measurement/uncertainty, guaranteed persistent topology naming under arbitrary Booleans, exact rotating collision, fully associative drawing updates, UI acceptance and deployment. No new Windows native test pass is claimed until user submits the output.

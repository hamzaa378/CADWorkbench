# CADWorkbench v1.70 — Sprint 12 Major Revision Integration

**Date:** 2026-11-17. **Status:** COMPLETE PRELIMINARY SOURCE. The *last verified Windows automated baseline* remains **Sprint 11**: the user provided a successful MSVC `CADWorkbench.exe` build, 130/130 native CTests, and 952/952 Python/source tests, with `AUTOMATED_PASS`. The 60 manual engineering acceptance scenarios remain `NOT_RUN`. Do not assert Sprint 12 passed MSVC, Qt, OpenCascade, full CTest, or GUI acceptance until actually run on Windows. Keep Sprint 11 binaries and audit evidence intact. This package is full cumulative source, not a patch.

## Engineering priority distribution for Sprint 12

This cycle follows the requested **70% completing/integrating existing workflows; 20% reliability and performance; 10% narrowly missing capabilities**. These describe developer effort allocation, **not feature completion percentages**.

### 70%: connect existing CAD, assembly, metrology and drawing workflows

- **One two-revision CAD change assessment:** `tools/engineering_revision_impact.py` reads two independently verified native **UNISSUED** manufacturing handoffs. It composes existing `compare_manufacturing_handoffs` and `assess_change_control`, checks matching body IDs and inspection revision tokens, and classifies every machining hole as `ADDED`, `REMOVED`, `CHANGED`, or `REFERENCE_MATCH`. Comparisons include saved `geometry_token`, hole coordinates, diameter, axial limits and through-envelope classification; revision-only changes invalidate the previous measurement binding. It reports body/occurrence changes, BOM and STEP change triggers, required review evidence, and a per-affected-item action register. Equal reference tokens cannot certify topology equivalence or metrology.
- **Conservative prior evidence handling:** Optional previous measurements must pass the original strict `review_measured_holes.review` against the PREVIOUS verified handoff, not against the new one. They are never copied to the current release as approved results. `automatically_carried_forward` is hard-coded to zero. Engineers must establish datum and inspection provenance again as appropriate for the new revision.
- **Drawing-to-drawing change reconciliation:** `tools/drawing_revision_diff.py` verifies **both** existing unissued drawing-export manifests and every advertised SVG/PDF hash. It joins by saved sheet-owner ID, detects changed title/drawing number, source revision, SVG/PDF bytes, added/removed owners and duplicates. It feeds the change report into the two-revision CAD impact analysis; check against the CURRENT handoff remains available through `reconcile_drawing_handoff.py`. A drawing can be a composite source, so a revision-token mismatch is a review item rather than automatically false geometry.
- **Single existing readiness workflow:** `tools/engineering_readiness_workflow.py` now accepts `--previous-handoff`, `--previous-drawings`, `--previous-measurements` in addition to `--drawings` and `--measured-csv`. It joins current CAD/BOM/procurement findings, current measurements, current drawings, changed manufacturing holes, altered occurrences, and changed drawing-preview references into an external unissued action register. It also flags sheet coverage that requires confirmation (absence of per-body sheets does NOT always indicate an actual missing drawing). No release-approval pathway was added.
- **Native C++ revision foundations:** `src/core/HoleRevisionImpact.h` extends the actual design-to-metrology structures in `HoleInspectionWorkflow.h`; its output is transaction-safe and independent of input order. `src/core/DrawingRevisionImpact.h` extends `DrawingReferenceAudit.h` with source-change tracking, annotation add/remove/modify/reference-match analysis and current annotation-freshness evidence. It never reissues or automatically acknowledges an annotation. Both headers are registered in `cadcore` CMake source inventory and exercised in the existing native `cadcore_v164_tolerance_stacks` and `cadcore_document_audit` CTests, respectively.

### 20%: reliability, failure isolation and performance

- **Verified, bounded drawing exports:** `tools/verify_unissued_drawings.py` now refuses linked folders/indexes/files and oversized batches (8 MiB index, 10,000 sheets, 256 MiB per drawing). Hashes are streamed in 1 MiB blocks instead of allocating whole PDFs/SVGs in RAM; files must match exactly the index inventory. Older file-not-found exceptions are preserved for established tests.
- **Strong-output guarantees:** New native revision comparisons stage their reports and return without modifying earlier outputs on invalid identities, duplicates or nonfinite inputs. Both classifiers are deterministic under shuffled input order and bounded to already supported inventories.
- **Earlier MSVC failure detection:** `RUN_SPRINT12_WINDOWS_TARGETED_BUILD.ps1` performs manifest, Python, static/CMake and 130-registration preflight before CMake. It compiles `cadcore`, workshop, document audit, articulated scene and feature-tree targets, then runs four relevant native CTests. Historical native test names are UNCHANGED: 130.
- **Expanded regression evidence:** New Python integration tests build real signed-inventory fixtures, separately exported hash-checked drawings, injected stale/malicious evidence, and exercise atomic publication. Added high-volume C++ permutation/rollback stress suites. Native source-integrated cases check changed revision invalidation and unissued drawing freshness.

### 10%: missing evidence bridge

- `tools/engineering_evidence_bundle.py` publishes one **new external directory atomically**. It contains integrated readiness JSON, spreadsheet-safe action CSV, optional revision-impact JSON, optional drawing-reconciliation and drawing-delta JSON and optional inspection review. It appends `evidence_manifest.json` with per-file SHA-256; all source handoffs/drawings remain unchanged, and any malformed input prevents a partial directory from being published. `tools/verify_engineering_evidence.py` independently verifies the bundle. Hashes are integrity checks, **not cryptographic signatures or authoritative provenance**.

## End-to-end instructions

1. Export two native `UNISSUED_REVIEW_REQUIRED` handoffs from different saved CAD revisions using CADWorkbench; do not edit their contents.
2. Optionally export earlier and current UNISSUED drawings from the model, and add independent measured hole data corresponding to the CURRENT revision. If supplying historical measurements, they must match the previous revision exactly.
3. From the source root, execute (edit paths, and remove unused optional arguments):

```powershell
py -3 .\tools\engineering_revision_impact.py "C:\CAD\OldHandoff" "C:\CAD\CurrentHandoff" --previous-drawings "C:\CAD\OldDrawings" --current-drawings "C:\CAD\CurrentDrawings" --json "C:\CAD\ExternalReview\revision_impact.json"
py -3 .\tools\engineering_readiness_workflow.py "C:\CAD\CurrentHandoff" --previous-handoff "C:\CAD\OldHandoff" --drawings "C:\CAD\CurrentDrawings" --previous-drawings "C:\CAD\OldDrawings" --measured-csv "C:\CAD\Measurements\current.csv" --json "C:\CAD\ExternalReview\readiness.json" --csv "C:\CAD\ExternalReview\actions.csv"
py -3 .\tools\engineering_evidence_bundle.py "C:\CAD\CurrentHandoff" "C:\CAD\EvidenceBundle_NEW" --previous "C:\CAD\OldHandoff" --drawings "C:\CAD\CurrentDrawings" --previous-drawings "C:\CAD\OldDrawings" --measured "C:\CAD\Measurements\current.csv"
py -3 .\tools\verify_engineering_evidence.py "C:\CAD\EvidenceBundle_NEW"
```

Output paths MUST be outside signed handoff and drawing folders. The bundle destination MUST NOT exist. Current measurements are optional, but missing data produces actionable open issues. Do not interpret a numeric inspection worksheet PASS as approval for fabrication, datum alignment, machine setup or accredited metrology.

## Tests run by assistant, this source only

- Python `python -m unittest discover -s tests -p '*.py' -q`: **985/985 PASS**, including independent Python OCP/CadQuery reference suites and 33 Sprint 12 integrated fixture tests (plus new CLI cases).
- GCC **41/41** portable C++ executables PASS; Clang **41/41** executables PASS. Two newest targeted high-volume suites: 81,110 CAD-hole revision assertions and 80,409 drawing-revision assertions.
- Standalone `g++` expanded workshop native-test SOURCE: **330/330** tolerance/inspection tests PASS (not proof of real MSVC GUI compilation).
- `tools/audit_full_source.py` reports valid source, Python AST, CMake references and 130/130 historic native-test registrations. Preflight will be rerun after regeneration of manifests and on freshly extracted ZIP.
- The package itself must be extracted, hash-verified and source-tested again before claiming verified delivery.

## Windows pre-build and full audit

Sprint 12 has **NOT** been compiled by MSVC. On a fresh Windows source extraction with `C:\vcpkg` and CMake installed:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT12_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4
```

Return code `4` means **targeted native compilation and 4 tests PASS**, and does not mean complete verification. After that, from the same source folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT12_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4 -FullAudit
```

Run ALL 130 native tests (no exclusions), **985 Python/source tests**, and inspect `build/v170_comprehensive_audit/backend_diagnosis.json`. Exit 3 can mean automated gates passed while manual acceptance remains pending. Upload failures and evidence logs for review, and do not promote before MSVC checks.

## Known limitations and continuation

- Python change-control operates on exported handoffs/drawings rather than UI edits; it does not prove B-rep equivalence, topology naming for arbitrary Boolean changes, multi-body drawing correctness or exact continuous rotating-body contact.
- Native revision classifiers are integration APIs + test scenarios, not auto-approved GUI controls. There is no electronic sign-off, CMM credential verification, measurement alignment or uncertainty computation.
- Real 60-scenario parametric/assembly/drawings acceptance remains **0/60** formally verified. Source-only and automated 130-test passing are different release gates.
- Large `SceneDocument.cpp` (~13.9k lines) and OCCT 8 deprecation warnings remain maintainability work; do not casually split the translation unit before native acceptance.
- Next work: UI-mediated design-change review (after UI freeze), source-token recovery on complex multi-body drawings, rotation-aware B-rep narrow-phase with proved intervals, actual multi-flange sheet-metal roundtrips, and manual end-to-end engineering validation. Preserve rollback and consolidate future improvements only after current Windows-native gate.

## File responsibilities

- Native: `src/core/HoleRevisionImpact.h`, `src/core/DrawingRevisionImpact.h`; existing native regression targets `tests/workshop_engineering_v164_tests.cpp` and `tests/document_audit_tests.cpp`.
- External workflows: `tools/engineering_revision_impact.py`, `tools/drawing_revision_diff.py`, `tools/engineering_readiness_workflow.py`, `tools/engineering_evidence_bundle.py`, `tools/verify_engineering_evidence.py`, `tools/verify_unissued_drawings.py`.
- Testing: `tests/test_sprint12_revision_workflows.py`, `tests/hole_revision_impact_portable_tests.cpp`, `tests/drawing_revision_impact_portable_tests.cpp`.
- Build: `RUN_SPRINT12_WINDOWS_TARGETED_BUILD.ps1`, `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1`, `CMakeLists.txt`, `tools/run_portable_regressions.py`.
- `LATEST_VERIFIED_BASELINE.md` describes Sprint 11 and deliberately remains the known-good *Windows* rollback record; Sprint 12 does not replace it until user-supplied real MSVC evidence is examined.

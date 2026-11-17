# CADWorkbench v1.70 — integrated three-workstream engineering update

**Status:** PRELIMINARY source checkpoint, 8 November 2026. No new MSVC/Qt/OpenCascade Windows native build has been performed on this archive.

## 1. Parametric modeling: simultaneous dimensions + optional features

- Added `SceneDocument::setCoordinatedParametricRevision` and matching detached-document preview.
- Allows 2–12 selected parametric bodies to change **named dimensions and feature suppression in the same final-state rebuild**. All named parameter links and saved expressions are reevaluated; edited dimensions replace their previous formulas by design.
- One B-rep rebuild per changed source, then a dependency-ordered linked-body propagation; the complete scene commits in **one undo checkpoint** only after every part succeeds.
- Removed the erroneous geometric eligibility check for a *suppressed* hole whose saved center is now outside a resized base. The original cut parameters are retained; reactivation still validates location, diameter and optional pattern.
- Batch editor UI now includes an optional-feature activation table below the named dimensions. Preview reports final solid volumes, face/edge counts and source/dependent bodies.
- Native test: `testCoordinatedDimensionSuppressionV170()` in `cadcore_parametric_foundation`, covering a final design that is valid even though the intermediate dimension-only state is invalid, plus rollback, undo/redo and save/reopen.

**Remaining:** This does not establish unrestricted persistent topology through arbitrary splits/merges or a general feature-DAG solver. All downstream face/edge references still obey the existing witness/provenance restrictions.

## 2. Assembly: bounded leader/follower drive coordination

- Added portable `JointDriveCoupling.h` with an acyclic dependency planner for one or more drive relationships, explicit same-kind (angular/angular or linear/linear) constraints, signed ratios, offsets, travel-limit checks, duplicate follower detection and fail-without-output-mutation behavior.
- The existing multijoint articulated editor can now enable one leader/follower relationship for the pending edit. A proposed target for the follower is computed from **relative displacement**, preserving the zero reference; the resulting coordinates pass through the application's actual OCCT articulated solver, full staged-geometry preview, optional motion samples and the existing checked one-step commit.
- This is **edit-time coordinated drive motion**, not a persisted gear relation. Dynamic forces, acceleration and arbitrary loop closure are not modeled by the new planner.
- Portable test: `tests/joint_drive_coupling_portable_tests.cpp` (positive/negative ratios, chain propagation, cycle rejection, duplicate followers, travel limits, mixed physical units, legacy unbounded joints and unchanged output on failure).

## 3. Sheet metal: developed cut-contour stock planning

- Introduced `StockContourNesting.h`: an orthogonal neutral-blank polygon represented as nonoverlapping solid rectangles after subtracting rectangular end-relief cutouts at each bend station.
- Stock planning for multi-part manufacturing jobs now uses the *same bend stations, K-factors and relief dimensions* as linked flat B-reps and export. Candidate material placement tests enforce planar material-to-material spacing, even if the **outer rectangles overlap inside relief voids**.
- Greedy deterministic, optional 90-degree rotation, bounded candidate search and all-or-nothing staged file publication remain in effect. The planner emits an SVG showing actual material patches rather than a solid envelope for notched blanks, plus CSV placement coordinates and JSON provenance including envelope vs net material utilization.
- Unrelieved blanks continue to use the previously verified free-rectangle envelope strategy; mixed batches use contour placement when any selected blank has end reliefs.
- Native test extends `testAssociativeManufacturingPackageV170()` in the existing `cadcore_sheet_metal_flat_pattern` CTest. Portable stress tests include nesting one blank inside another blank's relief cutout, 120 randomized 10-part layouts, duplicate/bad ID rejection, size/rotation limits and unchanged output on failed builds.

**Remaining:** This handles only the project’s existing **rectangular bend-end reliefs**, not general spline profiles, curved cutouts, kerf, grain constraints, tooling or validated CNC nesting. Nonrectangular blank profiles beyond these notches remain unsupported.

## Verification as of packaging

- Python/source suite: **506/506 passed**.
- GCC portable C++: **20/20 targets passed** without sanitizers.
- Clang portable C++: **20/20 targets passed** with address and undefined-behavior sanitizers.
- Additional targeted new portable C++ checks under both toolchains with UBSan: **passed**.
- SHA-256 source manifest and ZIP CRC are generated and verified at package creation.
- **Windows Release build for this new archive has not been run.** New native regression code is included but remains unverified on MSVC + Qt + OpenCascade.
- The last **user-reported** native baseline completed 130 tests with **128 passing**, with two known deferred failures: `cadcore_v170_assembly_mate_document` (clearance report) and `cadcore_folded_sheet_metal` (segfault). They are **not** fixed, bypassed or disabled here.
- The formal 60-scenario acceptance score has not been collected. No final-release approval is claimed.

## Focused native verification

Run from a clean extracted Windows source folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_NEW_WORKFLOWS.ps1 -VcpkgRoot C:\vcpkg
```

This compiles the application and runs a subset including `cadcore_parametric_foundation` and `cadcore_sheet_metal_flat_pattern`. It does NOT certify the full release. Later run `RUN_V170_RELEASE_GATE.ps1` and repair the two deferred native failures before attempting final approval.

# CADWorkbench v1.70 — Face-local mounting patterns and feature-history search

**PRELIMINARY DEVELOPMENT SOURCE ONLY. This is not a final release or a manufacturing-certified tool.**

## New saved modeling operations

1. **Face hole grid through all:** An editable rectangular drilling pattern on a witnessed planar face of a general valid solid. Enter `face_index`, `u`, `v` (mm, face-local pattern center), `radius` (mm, not diameter), `columns`, `rows`, `pitch_u`, `pitch_v` (center-to-center distances, mm), and `rotation_degrees` (−180° to 180°). The pattern is centered at the selected location, with 1–10 rows and 1–10 columns, 2–100 non-overlapping holes in total.
2. **Face bolt circle through all:** An editable flange/fastener mounting pattern on a witnessed planar face, using `count` (2–24), `pcd` (pitch-circle **DIAMETER**, mm), `start_angle_degrees` (−360° to 360°), `radius`, and local center `u`,`v`. The centers are uniformly spaced through 360°. The nearest-neighbor chord must exceed the bore diameter.
3. **Feature-tree filtering:** The General feature tree now has a search bar for existing feature operations, IDs, and numeric parameters. It filters rows without mutating node order or the underlying dependency graph. The Add feature dialog also indexes terms such as **flange**, **PCD**, **mounting array**, and **hole grid**.

The operations use the same persisted `ShapeFeatureNode`, planar witness resolution, `DirectionalHoleSpan` and OpenCascade Boolean machinery as the earlier through-all and slot operations. Cutters are rebuilt from the *current* B-rep at each feature regeneration, not from a saved material thickness. Multiple cylinders are passed as a cutter compound to one Boolean, retaining its kernel history for subsequent topology matching. Invalid input and failed Boolean operations are rejected before the document is committed.

To reduce silent invalid operations, every station center and 16 evenly spaced points on its entry-circle perimeter must lie strictly inside the witnessed planar face. This is a conservative sampling test, **not a mathematically exact proof that a circle is entirely contained by arbitrary trimmed faces**. Near-edge drilling, nonplanar entry surfaces, curved-surface normal patterns, complete topological naming through arbitrary face splits, multi-pattern variable-diameter tooling, tap/thread annotation, and toolpath planning are not provided by this increment.

## Regression changes

- New **C++20 standard-library-only** `ManufacturingHolePatterns.h` and a portable regression executable. Checks include centered and rotated grids, bolt-circle geometry, overlap rejection, integer counts, non-finite input, and failure atomicity.
- New independent **OpenCascade Python** reference tests: 2×2 and 3×2 grid material removal, multiple rotations, thickness changes, four/six/eight-hole PCD patterns, and drilling of a circular blank.
- Existing **native** `associative_features_v159_tests.cpp` enlarged (still the same CTest registration): grid edits, rotation, witnessed face translation, persisted bolt circle, invalid overlaps and off-face rollback. These tests have **not** yet run on Windows for this code.
- Search source contract ensures new operations are wired into the feature tree, UI, and Boolean lineage.

## Local verification (Linux; NOT a Windows build)

- 577/577 Python/source/OpenCascade-reference tests passed.
- 16/16 g++-compiled portable C++ test executables passed.
- 130/130 native CTest **registrations** preserved; Windows MSVC/Qt/OpenCascade compilation and execution **not performed** for this increment.
- The source-integrity manifest is regenerated during packaging and the ZIP is independently verified.

## Windows verification

Extract to a new empty directory separate from the Windows-tested Fix 2 baseline. Run PowerShell from the extracted project root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

**Expected only if the new Windows build succeeds:** 130/130 native CTests and 577 Python/source tests. Exit code 3 means automated verification passed but manual acceptance remains outstanding. Compile/test failures are real blockers and must not be bypassed.

## Workstream status and release gate

The last verified Windows baseline remains **Fix 2**, previously confirmed at 130/130 native tests and 546/546 Python tests. The historical **estimated** workstream coverage remains parametric 85%, assembly 90%, drawings/sheet metal 81% pending stronger evidence; the 60 GUI acceptance cases remain NOT_RUN. The hole patterns improve the parametric/mechanical workflow but do not establish 100% completion in any workstream.

The final release stays blocked until native Windows tests, all critical end-to-end GUI checks, and the accepted 60-case gate (two workstreams at least 17/20, third at least 13/20, with all critical cases passed) are actually satisfied.

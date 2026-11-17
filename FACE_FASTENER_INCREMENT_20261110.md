# CADWorkbench v1.70 — Associative face machining increment (preliminary)

**Status:** Source-only engineering development snapshot. Not a production release. The last Windows-confirmed build is the earlier Fix 2 baseline (130/130 native, 546/546 Python); this new increment **has not** had its native Windows build or GUI acceptance performed.

## New integrated feature-tree operations

- **Face through-all hole:** Hole center is entered as `u`, `v` millimetre offsets from a selected planar face center. The current B-rep and selected face determine inward direction and full cutting span on each regeneration, rather than a fixed depth.
- **Face counterbore through all:** Adds a coaxial cylindrical head recess using `radius` (pilot radius), `head_radius` and `recess_depth`. Designed for mounting screws and bolts when the dimensions are chosen for the actual fastener. The pilot bore runs through all current stock.
- **Face countersink through all:** Adds a conical head recess using `radius`, `head_radius` and `included_angle` in degrees, with allowed included angles 60–120. Its axial recess depth is calculated as `(head_radius-radius)/tan(included_angle/2)`.

These are **B-rep Boolean operations** in the existing saved Shape Feature Tree, not a new disconnected Python part generator. They reuse trimmed planar-face witness resolution, stable feature IDs, transactional rebuild and the scene's existing save/reopen and dependency propagation. The pilot and recess tools are combined for a single final Boolean cut that retains input-to-output kernel history. Invalid parameters, non-intersecting cuts and head recesses that remove no additional stock are rejected.

The commands appear in the searchable **General feature tree for any body** editor under **Add feature**. Select the stock's planar face before adding a command so the saved feature gets a witness anchor. `u/v` use the *local face* coordinate frame; the position is not a global XY coordinate. All radii are in millimetres, **not diameters**. A source planar face that disappears or becomes ambiguous is deliberately rejected rather than silently retargeted.

## Source paths changed

- `src/core/ShapeFeatureTree.cpp`: operation schema, input validation, B-rep construction, face-normal through cutter and atomic machining output.
- `src/ui/MainWindow.cpp`: menu/search registration, field labels, defaults and instructions, auto-captured face witness when adding.
- `tests/associative_features_v159_tests.cpp`: new native assertions in an **existing** 130-test target covering counterbore stock volume, moving face, edited head size, rejection/rollback, persistence, countersink volume, invalid dimensions and plain face through-hole.
- `tests/v170_face_fastener_machining_reference.py`: independent OpenCascade geometry references and source wiring contracts.

## Checks completed in this environment

- **567/567** Python/source/reference regression tests pass, including four new OCP geometry tests.
- **14/14** portable C++ executables pass.
- **130** native CTest registrations retained (unchanged).
- Byte-accurate SHA-256 manifests regenerated and verified against all packaged files.
- Patch was applied to the previous major engineering snapshot in a fresh directory; resulting source bytes match the ZIP.

**Not run here:** full Windows MSVC/Qt build, 130 Windows CTests of this increment, actual GUI interaction, manual mechanical fit/tolerance review, or any of the 60 release acceptance scenarios. A Python/OCP reference run does not verify the exact C++ implementation.

## Windows verification

Extract this snapshot to a **new empty directory**, not over Fix 2 or an earlier development build. From the extracted root containing `CMakeLists.txt`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Expected *only if the new C++ code builds and works*: 130/130 native CTests, 567/567 Python tests and exit code **3** for manual acceptance pending. Any error or nonzero native failure requires source correction; never disable or exclude a test. A clean extraction avoids the previously diagnosed stale `.exe` problem.

## Open engineering gaps

- Pilot through-hole cutting works on a selected **planar** face of a general valid solid. It is not yet face-attached drilling on arbitrary curved support surfaces. The already implemented arbitrary-axis, world-space through-all hole remains available.
- The head recess is referenced to the selected planar face and must geometrically remove material. It is not a full hardware/fastener clearance tolerance certification, does not automatically validate wall thickness, and does not replace actual fit/clearance checks.
- Edits to original geometry that delete the source face or make its witness ambiguous may require user re-selection. Universal topological naming across any possible split/merge is still incomplete.
- Remaining significant workstreams: complex persistent topology, assembly mating/kinematics and real fit verification, associative drawing/GD&T lifecycle, multi-flange sheet metal and GUI acceptance. Historical provisional implementation estimates **85% parametric, 90% assembly, 81% drawings/sheet metal** are not changed by this release. GUI acceptance remains **0/20, 0/20, 0/20 verified**.

**Final v1.70 release gate remains blocked:** Windows regression must pass, all critical acceptance scenarios must pass, and at least two workstreams must be 17/20 while the third is at least 13/20 with real evidence.

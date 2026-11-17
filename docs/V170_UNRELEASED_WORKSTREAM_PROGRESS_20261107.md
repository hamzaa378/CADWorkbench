# CADWorkbench v1.70 — unreleased 85/85/65 workstream increment

Status: unreleased development. **A preliminary source snapshot was explicitly requested after this pass; it is not a release and cannot be promoted or described as acceptance-gate-complete.** The user requires at least two workstreams reaching 17/20 independently verified acceptance cases and the remaining workstream reaching 13/20, with no critical failures. The face-machining CTest must also pass and a fresh Windows MSVC Release build must pass the complete 130-CTest suite. No such complete evidence exists for this working tree.

## Starting point

- Corrected source from `CADWorkbench_v170_FaceMachining_WitnessFix_Full_Source_20261107.zip`.
- Prior user Windows log: original clean-build source compiled but `cadcore_v170_interleaved_history` failed with `face machining produces a valid solid` (129/130). The face-witness test-fixture correction is present but **Windows retest has not been received**.
- Prior in-progress assembly enhancements: atomic nested-group move/rename, parent group listing, exact child-inclusive clearance audit, BOM UI actions, native persistence/rollback tests. These remain **Windows-unverified**.

## This development increment

### P — parametric-modeling interactive sketch editing

- Sketch editor now lists saved constraints and their referenced entity IDs, numeric dimensions, and fixed-point anchors. Constraint types not currently supported for numerical editing can still be removed.
- Edit existing point distance, length, radius, signed-distance, and angle dimensions through a real Qt dialog. Angular edits display degrees and convert to solver radians; sketch distance fields remain millimetres.
- Remove a selected constraint. Both edits use the existing full solver and closed-profile extrusion check; invalid edits are rejected without changing the sketch. The existing per-sketch undo restores the previous dimension, points, and circles together.
- Fixed a real defect in `SketchCanvasWidget::tryRelation`: the numerical solver returned updated circle radii, but the UI copied back only point positions. A radius dimension edit now copies `solvedCircleRadii` as well.
- Extended existing native `cadcore_sketch_solver` CTest to instantiate the real `SketchCanvasWidget` in offscreen Qt mode, covering dimension edits, invalid index/dimension, overconstraint rollback, circle radii, constraint deletion, and undo. This keeps the total native CTest count at 130.

### A — assembly hierarchy and clearance work retained

- `SceneDocument::moveAssemblySubtree` is atomic for existing assembly-group metadata and preserves solids, occurrence identities, and placement. Nested paths are validated and collisions are rejected. Undo/redo and file persistence are covered in new native regression assertions.
- `inspectAssemblyGroupFit` runs the existing exact OCCT pairwise fit audit over all solid bodies inside the chosen group, including descendants, with a 2–32 body limit and atomic output. UI shows the audit in the BOM/structure dialog.
- Outstanding: full interactive hierarchical mate authoring, freeform closed-loop motion and dragged dependent subassemblies, robust mate rebind across topology and large assemblies.

### D — manual drawing GD&T annotation edit

- The drawing annotation menu adds an **Edit** action for each existing manually authored feature-control frame instead of only Add/Remove. In-place edits preserve the frame's stable ID and prefill current saved values, source body, datums, characteristic, tolerance and paper position.
- Editing a frame after its referenced source geometry token changed requires an explicit re-authoring confirmation. This is not a GD&T validation or automated drawing release signoff.
- Added native drawing regression assertions that editing a frame keeps ID/source reference stable and survives undo/redo. Existing save/reopen, stale source and invalid duplicate-datum checks remain.
- Outstanding: true feature-based GD&T controls and datum references, associative dimension standards, production sheet-metal bend/corner system, and verified folded-flat-refold topology round trip.

## Verification in this environment

- `python -m unittest discover -s tests -p '*.py' -q`: **350 passed**; many are source-contract tests and independent Python/OCP reference checks, **not** direct native app verification.
- `bash scripts/verify_portable_consolidation.sh`: passed, including portable GCC C++ sanitizer checks and Python/OpenCascade independent reference checks.
- CMake test registrations: **130 maintained**. The existing `cadcore_sketch_solver` target now includes Qt Widgets and the sketch canvas implementation.
- **Not performed:** Windows configure, MSVC compile/link, 130 native CTests, Qt interactive manual checks, all 60 workstream acceptance scenarios. Hence **no numeric workstream completion percentages are assigned and no ZIP can be released**.

## Windows validation after further implementation

From the source root (with vcpkg installed):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

The test inventory remains exactly 130, but that count is NOT a pass claim. Keep the last fully verified binary outside this working tree. Do not promote this increment without an actual passing Windows build and the 85/85/65 release gate.

## Additional D-workstream increment — folded profile overlap guard (2026-11-07)

The folded sheet outline builder now rejects degenerate/self-intersecting faceted
XY cross-sections *before* OCCT attempts to construct an extruded solid. The
kernel-independent `FoldedContourTopology.h` tests edge crossings, nonadjacent
endpoint contacts, adjacent reversal, zero-area and zero-length edges. The
existing native folded-sheet CTest now requires a 165-degree two-bend
self-intersection to fail without changing previously valid outline, part count
or edited body token. Valid single- and opposite-direction folds retain the
existing native tests. Portable GCC/Clang sanitizer cases are runnable from
`scripts/verify_portable_consolidation.sh`.

Scope: this is a defensive geometric validity check, **not** edge-flange
feature history, corner-relief generation, fold/refold inverse reconstruction,
production tolerance certification, or verified UI acceptance. Full Windows
Qt/OCCT native build remains outstanding. No release ZIP is authorized.

## Additional P-workstream increment — fixed-anchor editing (2026-11-07)

The existing constraint manager now exposes a separate **Edit fixed anchor...**
action. A modal two-coordinate editor preloads the selected fixed point and
commits both coordinates atomically through the existing sketch solver and
profile-validity check. Non-finite/out-of-range inputs, wrong constraint types,
and invalid references fail without committing. Existing sketch undo restores
both the old anchor and the computed points. Native offscreen Qt regression
cases are appended to `cadcore_sketch_solver` without registering another CTest.

The new UI and native tests have not been built with MSVC/Qt here. This remains
source-level implementation only until Windows GUI/native CTest evidence exists.

## Additional P-workstream increment — interactive paired sketch constraints (2026-11-07)

The sketch canvas now exposes four existing solver relations through actual click tools:
**equal edge lengths**, **signed angle between two directed straight segments**,
**equal circle radii**, and **concentric circles**. For an angle, the first
selected segment defines the reference and the second segment defines the
moving side; the user-facing value is degrees and the solver stores radians.
Circle picking selects the circumference rather than treating a circle center
as an ordinary sketch vertex.

Each operation stages a full SketchModel, validates it through SketchSolver and
(if applicable) the closed-profile extrusion check, then commits once with
sketch undo. Auto-created fixed endpoints and second-circle driving radius or
center constraints are released only on the second selected entity, leaving the
first driving geometry and unrelated dimension constraints intact. Duplicate
identities, missing entities, curved-edge misuse and malformed angle values are
rejected. Contradictory remaining constraints prevent the commit; the editor
never silently deletes unrelated driving dimensions to force a solve.

The existing `cadcore_sketch_solver` native CTest now includes Qt widget
regressions covering dimension propagation, both circle relations, a signed
angle, invalid inputs, conflicts and undo restoration. Eight new Python
source-contract cases ensure these paths remain connected to the UI and
native test. There are still 130 registered CTests. The native tests were **not
compiled/executed under Windows** in this environment.

This increment moves toward P01/P03 but does not complete unrestricted solver
or history functionality, persistent topology, generalized loft/sweeps, or the
required 85% threshold. Release remains prohibited.

# CADWorkbench v1.70 — planar face lineage, in-place linked sketches and assembly mates

This is an incremental **116-native-CTest Windows Release candidate**, based on the previous 111-test associative sketch boss source. It does not claim to finish the 50-feature roadmap. The Windows MSVC/Qt/OpenCascade build has **not** run in the development environment.

## 1. Persistent planar face identity through supported topology changes

- New planar face anchors record a point **strictly inside the trimmed face**, in addition to its canonical normal, face centroid, area and source-body reference center. The witness is checked with OpenCascade's `BRepClass_FaceClassifier`, not just a plane-distance test.
- After regeneration, an anchor can resolve to the unique planar successor that contains its witness. This supports some **splits and coplanar merges**, including the verified split of a box's top face into two trimmed successor faces. Pure translations are also compensated using the stored body center.
- If two successors contain the witness or none does, the operation fails with an explicit reselect message instead of selecting a face arbitrarily. Incomplete witness data is rejected.
- Previously saved 7-field and 10-field anchors still use legacy matching and can be loaded.
- **Limits:** This is *not* a universal persistent topological naming implementation. It does not track all arbitrary rotations, curved faces, destroyed witness regions, face removal, non-planar replacements, or every Boolean split/merge. A witness near a split boundary may become ambiguous and require user reattachment. The old anchor cannot retroactively acquire a witnessed lineage without recapture.

## 2. In-place external constrained-sketch boss/pocket

- In **Edit → Add/edit in-place external sketch boss or pocket...** (wording may vary slightly by menu), choose a source body with a current editable constrained polygon sketch and an eligible ordinary target solid.
- Fuse adds material; Cut removes material. Depth and normal offset are independently editable. The operation changes **the target body's existing stable ID** and records its pre-feature B-rep. It does not add a separate visible output body.
- The existing graph planner tracks the source as a unary dependency. Editing the source sketch rebuilds the in-place modifier and any dependent linked Boolean results in dependency order. The update is committed atomically and participates in undo/redo.
- A saved v24 scene contains `in_place_sketch_feature` with source ID, source geometry token, operation, depth, offset and frozen original B-rep. Loading checks current source tokens, B-rep validity, recomputed volume and bounding coordinates before it replaces the current document.
- Source deletion is blocked while a live modifier refers to it. Detached/ineffective bosses and ineffective cuts are rejected; failed updates do not replace committed geometry.
- **Limits:** One in-place modifier per ordinary target body; it currently cannot coexist with a separate shape feature tree, another constrained sketch on the target, a component occurrence, or a linked Boolean/linked extrusion result. This is not yet a general multi-operation editable feature stack or sketch-attached modifier for all solids. Opening and re-saving in an older build can drop this optional v24 metadata: keep scene backups.

## 3. Rigid-body assembly mate solver

- The new C++ `AssemblyMateSolver` offers **point coincidence**, **coaxial oriented axis**, and **coincident oriented plane** constraints, with up to 32 individual constraints. Input coordinates are in the current assembly/world frame, units millimetres, normals unit-length.
- It solves a single free child occurrence's rigid 6-degree-of-freedom transform against a selected parent, with bounded damped nonlinear least squares. It explicitly rejects rank-deficient, conflicting, invalid, or nonconverged constraint combinations and commits nothing on failure. Coordinate recentering supports large world offsets within the model bounds.
- The **Assembly → Solve assembly frame mates...** UI provides **three corresponding non-collinear point pairs** to fully constrain one rigid placement. Optional persistent Rigid parent-child joint records the resulting attachment. Geometry, XYZ and yaw/pitch/roll placement are changed in one undoable step, and the saved placement is checked for replay consistency.
- **Limits:** Axis/plane constraints are available to the C++ solver but are **not yet directly authored through that three-point UI**. Mate constraints themselves are not persisted as individually editable formulas; the solved transform and optional rigid joint are. This is not a general closed-loop, many-body, continuously driven mate solver, collision avoidance planner, or industrial tolerance-fit analysis.

## Verification evidence in this source increment

- Portable mate regression (`tests/assembly_mate_solver_v170_tests.cpp`): **19 runtime checks** under both GCC and Clang, including C++ Release `NDEBUG`; ASan/UBSan also passed. The test deliberately does not rely on disabled Release `assert`.
- Source-level preflight: **106 assertions**; source contracts: **32 assertions**. Source checks inspect integration but cannot prove a native Qt/OCCT compile.
- Independent OCP/CadQuery geometry reference: **four Python unit tests**, including split-face witness uniqueness, in-place boss/pocket volumes and a reference rigid transform.
- Native test CMake registry: **116 unique CTests**, five more than the original 111-test package. Native tests cover face lineage, in-place regeneration and scene persistence, plus assembly solve and scene persistence.
- The source archive and its incremental patch are audited against a clean extraction of the exact 111-test source. Refer to the delivery message for audit result.

## Windows acceptance (required)

From a fresh extraction of the complete source ZIP, in PowerShell:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 116
```

Keep the last Windows-verified source and `.cadscene` backups until this build passes. Run an interactive smoke test for both new Edit/Assembly dialogs, a face-attached feature after a face split, save/reopen of an in-place modifier, and mate creation on two real assembly occurrences.

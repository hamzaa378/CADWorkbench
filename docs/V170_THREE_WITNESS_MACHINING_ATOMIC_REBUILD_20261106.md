# v1.70: three linked engineering-safety increments (2026-11-06)

**Status: source development candidate, not a verified Windows release.** The full Qt + MSVC + OpenCascade application has not been compiled, interactively checked, or put through its 130 registered native CTests for these edits. Keep the last successfully tested Windows executable.

## 1. Fail-closed edge and face reference recovery after Boolean cuts

A Boolean kernel's `Modified()` result is evidence of ancestry, **not** proof that the specific saved design location remains. The rebase path now requires the edge anchor's witnessed point to be found on a kernel-confirmed descendant, even if the kernel lists only one surviving edge. If the cut removed the saved midpoint, rebuilding the downstream fillet/chamfer is rejected.

For planar face operations, matching the face witness alone is insufficient when the actual pocket/boss machining center (`u`,`v`) was removed by a hole or split. The implementation reconstructs that point in the old face plane, checks it lies in the new face plane, and uses `BRepClass_FaceClassifier` to require it to remain strictly inside the trimmed successor. On a valid survivor, the unchanged design position is re-expressed relative to the new face center, rather than moving the machining operation when a face centroid changes.

Limitations: does not establish universal topological naming; disallows boundary-only positions, ambiguity, destroyed geometry, and changes beyond supported witnessed lineage. `1e-5 mm` plane and containment checks are internal conservative tolerances, not general CAD/inspection accuracy guarantees.

## 2. Current-kernel provenance for face machining

`ShapeFeatureTree.cpp` now retains `std::shared_ptr<BRepAlgoAPI_Cut>` / `std::shared_ptr<BRepAlgoAPI_Fuse>` history, with the exact committed result shape, for supported face-normal round/rectangular pockets/bosses and sketch-polygon face pockets/bosses. The downstream ancestry resolver can use `Modified()`/`Generated()` on these operation objects during **the current single-stage transaction**. A mismatched cached builder or unsupported multi-node kernel-history result is not accepted as lineage evidence. Previously supported Boolean/fillet/loft/sweep and rigid/pattern cases remain.

## 3. Atomic fan-out regeneration test coverage

New native C++ regression exercises one external constrained sketch driving two separate linked target bodies. The first branch uses an additive Boolean; the second uses a subtractive Boolean. A translated source remains valid for the additive branch but ceases to cut material in the second. The test requires the entire attempted source edit to fail without changing any of the three geometry tokens, volumes, or valid linked histories. A subsequent valid width change must rebuild both targets; undo and redo must restore/reapply their identities together.

Independent OpenCascade/OCP geometry reference tests prove the selected cut/fuse fixture has the intended valid/invalid intersections, but they **do not** execute or validate the app's C++ transactional implementation. The native test is registered within the existing `interleaved_history_v170_tests` CTest and requires Windows verification.

## Checks performed in this workspace

- `python -m unittest discover -s tests -p '*.py'`: **253 passed**, including new source-level contracts and independent OCP reference cases.
- Portable existing ancestry planner: **37** under each GCC and Clang with sanitizers.
- Portable existing sparse feature graph planner: **347** under each GCC and Clang with sanitizers.
- Portable existing linked-body DAG planner: **80** under each GCC and Clang with sanitizers.
- Existing CMake Windows-only guard confirmed on Linux; **not** a successful application compilation.
- All **130** registered Windows CTests remain present, but **none were executed here**.

Note: portable graph planners are pre-existing modules, not a compilation of the newly edited Qt/OpenCascade native paths. This increment fixes a forward declaration required for `insideTrimmedPlanarFace` in C++, but only the full Windows compiler can establish complete native build correctness.

## Mandatory native acceptance steps

From the extracted project root in PowerShell:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Then interactively edit a constrained sketch driving two linked solids, verify changed geometry and undo/redo, test one witness removed by a Boolean cut, test pocket/fillet references to surviving geometry, save/reopen, and verify all geometry remains valid. Do not supersede the last verified executable unless the native build, CTests and GUI workflows pass.

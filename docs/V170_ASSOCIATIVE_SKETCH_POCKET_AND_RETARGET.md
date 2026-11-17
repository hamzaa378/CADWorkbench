# CADWorkbench v1.70: Associative sketch pocket and source retargeting

## Implemented in this increment

The sketch-derived body editor can now **reassign its source** to a different existing, current constrained polygon sketch, while editing depth and normal offset. Stable body IDs are used. The proposed graph is evaluated in dependency order; invalid, missing or stale sources, invalid dimensions, and failed descendant Booleans are rejected before any checkpoint. Undo/redo returns the complete previous source graph. A no-op creates no checkpoint.

**Create → Associative pocket from another body's sketch...** builds two linked bodies in one transaction: a **hidden sketch-derived cutter** and a **visible linked Boolean Cut result** based on the user-selected target. The original sketch and target body remain unchanged, and the hidden cutter remains available for direct source retargeting or depth/offset editing. Edits to the original constrained sketch propagate through cutter to the Boolean result. The command rejects a disjoint cutter that removes no measurable material, a fully consumed target, stale target dependencies, or invalid source sketches. Subsequent source edits that move the cutter entirely off the target are also rejected transactionally instead of silently converting the pocket into a no-op. The two bodies appear and disappear together on one Undo/Redo.

Both workflows use the existing v24 `.cadscene` optional `linked_sketch_extrusion` and `linked_boolean` records; **no format change**. Save/reopen retains the cutter's visibility and live dependency IDs; a missing reference or mismatching saved geometry is rejected on load by the existing integrity checks.

## Scope and limitations

- **Separate derived result body, not an in-place pocket modifier.** The source body and target remain in the scene. The cutter is hidden by default, but the original target may need to be hidden manually to clearly see the result.
- Only current, saved **constrained polygon sketches** are live sources, not arbitrary imported geometry or an arbitrary feature-tree face.
- Pocket direction is the saved sketch plane normal; offset/depth control cutter placement. The command does not yet compute automatic depth-to-face or through-all end conditions.
- The referenced sketch's plane is reused; sketches on offset or oblique planes can be used, provided the generated cutter removes nonzero volume from the chosen target.
- Boolean subtraction must leave a valid positive-volume solid. Tangential or non-intersecting tool positions are rejected.
- This builds on the existing dependency graph; it is not full topological naming or a general assembly mate solver.

## Test gate

New native test modes: `cadcore_v170_linked_sketch_retarget`, `cadcore_v170_associative_sketch_pocket`, `cadcore_v170_associative_sketch_pocket_persistence`. The earlier 106 tests remain registered, for **109 total**. The tests exercise geometry volumes, transitive rebuild, validation/rollback, one-step undo/redo, deletion protection, save/reopen and hidden cutter state.

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 109
```

**Verification boundary:** The Python source-check suites, portable GCC/Clang tests and independent OpenCascade reference geometries can be run here. The Windows Qt/OCCT Release executable and 109 native CTests require native Windows acceptance; do not claim they passed before the user provides evidence.

An optional independent geometric reference, `tests/v170_sketch_pocket_kernel_reference.py`, runs 12 OpenCascade/CadQuery geometry and volume assertions. It requires the Python `cadquery` module and is **not** part of native CTest registration.

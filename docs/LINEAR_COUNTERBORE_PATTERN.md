# Linear counterbore pattern

Choose **Modify → Linear counterbore pattern** to cut a straight row or 2D vector array of counterbored holes into one selected body.

Set the first hole center, top entry height, drill diameter, counterbore diameter and depth, instance count (2–100), and X/Y spacing. Enable **Through all** for full-depth drill holes, or clear it to enter a blind-hole depth. Counterbore depth cannot exceed the blind drill depth. The whole pattern is applied as one undoable body change; if any sequential cutter fails, the original body is left unchanged.

## Verification

- Windows x64 Release application and regression target built successfully.
- Focused regression passed 23 assertions: parameter rejection, count and spacing bounds, through and blind patterns, valid topology, material removal, undo/redo, and rollback when a later cutter misses the body.
- Full CTest passed 20/20; source audit passed 136 checks with regression references for all 96 public `SceneDocument` operations.
- Live UI created three counterbores on a 40 × 30 × 20 mm box. Volume changed from 24,000 to 23,111.418 mm³; topology changed from 6 faces / 24 edges to 15 / 60. Undo restored the box and redo restored the three seats.

The operation cuts down along Z and spaces hole centers along a 2D vector. It does not add modeled threads or a parametric feature timeline.

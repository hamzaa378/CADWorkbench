# Linear countersink pattern

Choose **Modify → Linear countersink pattern** to cut a straight row or 2D vector array of countersunk holes into one selected body.

Set the first hole center, top entry height, drill diameter, countersink diameter and included angle, instance count (2–100), and X/Y spacing. Enable **Through all** for full-depth drill holes, or clear it to enter a blind-hole depth. The full pattern is one undoable body change. Each cutter is checked before commit; a failed later cutter leaves the source unchanged.

## Verification

- Windows x64 Release application and regression target built successfully.
- Focused regression passed 23 assertions: invalid parameters, head/drill sizing, count and spacing, through and blind patterns, valid topology, volume removal, undo/redo, and later-cutter rollback.
- Full CTest passed 20/20; source audit passed 136 checks with regression references for all 96 public `SceneDocument` operations.
- Live UI created three countersinks on a 40 × 30 × 20 mm box. Volume changed from 24,000 to 23,315.987 mm³; topology changed from 6 faces / 24 edges to 12 / 54. Undo restored the box and redo restored the three countersinks.
- Live UI blind-depth variant used three countersinks with an 8 mm drill depth. Volume changed from 24,000 to 23,682.423 mm³; topology changed from 6 faces / 24 edges to 15 / 54. Undo and redo restored the original and cut states.

The operation cuts down along Z and spaces hole centers along a 2D vector. The included-angle control sets the conical seat geometry. It does not model thread helixes or add a parametric feature timeline.

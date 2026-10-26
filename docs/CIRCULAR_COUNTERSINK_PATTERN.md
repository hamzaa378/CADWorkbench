# Circular countersink pattern

CADWorkbench v1.9 can cut a circular bolt-circle array of countersunk holes into one selected body. Choose **Modify → Circular countersink pattern**.

Set the bolt-circle center and radius, top entry height, drill diameter, countersink head diameter and included angle, hole count, and pattern arc. A 360° arc spaces holes around the circle without duplicating the first position; partial arcs include both endpoints. The holes may pass through the entire body or use the specified blind depth. The pattern is applied as one undoable change.

## Verification

- Windows Release build succeeded.
- Focused regression: 28 assertions covering invalid dimensions and counts, angle bounds, missing bodies, a later missed cutter leaving the body unchanged, valid full-circle through holes, valid blind partial-arc holes, volume reduction, and undo/redo for both through and blind patterns.
- Live UI applied four default 90° countersinks to a 40 × 30 × 20 mm box. The viewport showed four tapered seats. Measured volume changed from 24,000 to 23,087.983 mm³, with 14 faces and 64 edges. Undo restored 24,000 mm³ and 6/24 faces/edges; redo restored 23,087.983 mm³ and 14/64.
- Live parameter check applied three blind countersinks over a 180° arc, using an 8 mm blind depth. Three tapered seats were visible. Volume changed from 24,000 to 23,682.423 mm³, with 15 faces and 54 edges. Undo restored the original; redo restored all three cuts.

This feature places the holes on an XY bolt circle and cuts down from the selected body's specified entry Z. It does not create threaded geometry or a parametric feature timeline.

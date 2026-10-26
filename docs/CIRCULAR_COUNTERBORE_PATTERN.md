# Circular counterbore pattern

Choose **Modify → Circular counterbore pattern** to cut a bolt-circle array of stepped holes in a selected body.

Controls include the bolt-circle center/radius, entry height, drill diameter, counterbore diameter/depth, hole count, pattern arc, through-all mode, and blind depth. A 360° pattern spaces positions without repeating the start; partial arcs include both ends. The drill and counterbore cuts are applied together as a single undoable change.

## Verification

- Windows Release build and focused 21-assertion regression passed.
- Tests cover invalid head/drill proportions and blind depth, invalid count/radius, missing and missed bodies, full-circle through holes, blind partial-arc holes, solid validity, removal bounds, and undo/redo.
- Live UI applied four 7.2 mm diameter, 3 mm deep counterbores around a 7.5 mm bolt circle to a 40 × 30 × 20 mm box. Measured volume changed from 24,000 to 22,819.264 mm³; topology became 18 faces and 72 edges. Undo and redo restored the expected shapes.
- A second live run used three 180° partial-arc positions, an 8 mm blind depth, and 3 mm counterbore depth. Three stepped seats appeared; volume changed from 24,000 to 23,480.883 mm³. Undo restored the box and redo restored all three blind counterbores.

Threads are not modeled. The feature cuts direct geometry and does not add a parametric feature timeline.

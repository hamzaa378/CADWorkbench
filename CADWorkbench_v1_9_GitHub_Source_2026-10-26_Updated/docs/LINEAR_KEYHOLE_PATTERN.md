# Linear keyhole-slot pattern

Choose **Modify → Linear keyhole-slot pattern** to cut multiple keyhole profiles into one body.

The dialog sets the first center, entry Z, circular head diameter, throat width and length, cut depth, rotation around Z, number of instances (2–100), and 2D spacing vector. Each keyhole has a circular head connected to a narrower straight throat. The complete pattern forms one undoable body modification.

## Verification

- Windows Release build and focused 22-assertion regression passed.
- Coverage includes invalid head/throat dimensions, rotation, count and spacing, a rotated pair, shape validity, material removal, undo/redo, and rollback when a later slot misses the body.
- Live UI applied three keyholes (8 mm head, 4 mm throat, 12 mm throat length, 6 mm depth, 12 mm X spacing) to a 40 × 30 × 20 mm box. Volume changed from 24,000 to 22,506.725 mm³; topology became 30 faces and 138 edges. Undo and redo restored the expected states.

Slot centers follow a straight 2D spacing vector; instances have shared dimensions and orientation. This is a direct solid cut rather than a parametric feature timeline.

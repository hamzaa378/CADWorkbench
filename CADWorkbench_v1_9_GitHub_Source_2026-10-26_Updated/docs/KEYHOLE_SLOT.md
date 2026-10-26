# Keyhole slot

The Modify > Keyhole slot command cuts a round head connected to a narrower straight throat into one selected solid. The throat extends from the head center in the local positive-Y direction; rotation about world Z orients the whole profile.

## Controls

- Head center X and Y, plus entry Z, in millimeters.
- Head diameter, throat width, and throat length. The throat width must be less than the head diameter.
- Blind cut depth and rotation about world Z.

The application joins a cylindrical head cutter and rectangular throat cutter, validates the resulting body and confirms that material was removed, then presents a preview before apply. Applying the preview records one undoable body edit. A stale preview is rejected if its source body changes before commit.

## Verification

- Windows Release core and UI builds passed.
- `cadcore_tests --keyhole-slot` passed 28 assertions covering invalid dimensions, valid preview topology, material removal, rotation, missed-body rejection, stale-preview protection, undo, and redo.
- The dovetail, T-slot, rounded-slot, and keyhole-slot focused CTests passed 4/4.
- Live Windows UI applied a 10 mm head, 4 mm throat, 12 mm throat length, and 6 mm depth to a 40 x 30 x 20 mm box. Measured volume changed from 24,000 to 23,357.480 mm^3 and topology from 6 faces / 24 edges to 14 / 62. Undo restored the original box; redo restored the keyhole cut.

This operation makes a direct solid cut. It does not create a parametric sketch or feature timeline item.

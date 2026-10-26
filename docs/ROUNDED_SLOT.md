# Rounded slot

The Modify > Rounded slot command cuts an obround pocket into one selected solid. The plan profile is made from a rectangular middle section and two cylindrical ends; when overall length equals width, the cut becomes a circular pocket.

## Controls

- Center X and Y, plus entry Z, in millimeters.
- Slot width and overall length. Length must be at least the width.
- Blind depth and rotation about world Z.

The app builds a cutter that extends slightly above the entry face, checks the cut result for valid topology and material removal, then presents a preview before apply. Applying the preview is one undoable body edit. A stale preview is rejected if the target body changes before commit.

## Verification

- Windows Release core and UI builds passed.
- `cadcore_tests --rounded-slot` passed 31 assertions covering invalid input, preview validity, material removal, the length-equals-width case, rotation, missed-body rejection, stale-preview protection, undo, and redo.
- Related CTests passed 3/3: `cadcore_dovetail_groove`, `cadcore_t_slot_groove`, and `cadcore_rounded_slot`.
- Live Windows UI applied an 8 mm wide, 18 mm overall-length, 6 mm deep slot to a 40 x 30 x 20 mm box. Measured volume changed from 24,000 to 23,218.407 mm^3, and the viewport showed the obround pocket. Undo restored 24,000 mm^3; redo restored 23,218.407 mm^3.

This operation makes a direct solid cut. It does not create a parametric sketch or feature timeline entry.

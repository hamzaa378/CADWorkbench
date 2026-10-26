# T-slot groove

The Modify > T-slot groove command cuts one straight or rotated T-slot into a selected solid body. The cross-section has a narrower opening at the entry face and a wider rectangular undercut below it.

## Controls

- Center X and Y, plus entry Z, in millimeters.
- Neck width and undercut width.
- Total depth and undercut height.
- Cut length and rotation about world Z.

The application builds the cutter, validates the resulting solid, and confirms material removal before offering an apply step. Applying the preview records one undoable body edit. A stale preview is rejected if its source body changes during preparation.

## Verification

- Windows Release core and UI builds passed.
- `cadcore_tests --t-slot-groove` passed 28 assertions covering input validation, preview validity, material removal, rotated cuts, missed-body rejection, stale-preview handling, undo, and redo.
- `ctest --test-dir build -C Release -R "cadcore_t_slot_groove|cadcore_dovetail_groove" --output-on-failure` passed 2/2.
- Live Windows UI used a 40 x 30 x 20 mm box and the default T-slot controls. The preview showed the recessed channel; after apply, measured volume changed from 24,000 to 23,135.040 mm^3, and topology changed from 6 faces / 24 edges to 15 faces / 72 edges. UI undo restored 24,000 mm^3 and redo restored 23,135.040 mm^3.

This is a direct solid cut. It does not add an editable sketch or parametric feature timeline item.

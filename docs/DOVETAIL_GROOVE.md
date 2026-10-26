# Dovetail groove

The Modify > Features > Dovetail groove command cuts a straight dovetail channel into one selected body. The cross-section opens at the entry face and widens below the mouth to form the undercut. The groove can be rotated about world Z.

## Controls

- Center X and Y, plus entry Z, in millimeters.
- Mouth width and wider undercut width.
- Groove depth and length.
- Rotation about Z in degrees.

The preview is built from an extruded trapezoidal cutter. CADWorkbench validates the resulting boundary representation and confirms that material was removed before offering the apply step. Applying the preview is one undoable body edit. A stale preview is rejected if the body changed while it was being built.

## Verification status

The feature is implemented in the document model and connected to the Modify menu and ribbon. The Windows Release core and UI compiled, and `build/Release/CADWorkbenchLiveTest.exe` linked. The focused geometry regression passed 28 assertions for invalid inputs, valid preview topology, material removal, rotation, stale-preview handling, undo, and redo. The live UI workflow also passed: the default groove preview applied to a 40 x 30 x 20 mm box, changed its measured volume from 24,000 to 23,008.360 mm^3, and showed the channel in the viewport. Undo restored the box and redo restored the groove. A repeat UI attempt after adding the focused regression was blocked by a Computer Use target-window mismatch; the earlier UI result is from the same feature implementation.

This is a direct solid cut. It does not create a parametric sketch or feature timeline entry.

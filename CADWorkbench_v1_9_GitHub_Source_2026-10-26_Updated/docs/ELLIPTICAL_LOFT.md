# Elliptical loft

Choose **Create → Elliptical loft** to create a solid between two closed elliptical sections. Set the lower and upper semi-axis A/B values, height, and XY offset of the upper section. A and B remain aligned to world X and Y even when A is the smaller semi-axis. The operation creates one body and one undo step.

## Verification

- Windows Release core and UI build passed.
- Focused regression passed 18 assertions covering invalid semi-axes, height and offsets, solid topology, expected bounds and volume, and undo/redo.
- Full Windows CTest passed 21/21; source audit passed 136 checks and referenced all 97 public core operations from regressions.
- Live UI verified the menu and ribbon command, created the default offset loft, and showed a valid solid with volume 17,886.131 mm³, bounds X −20…20, Y −12…12, Z 0…35 mm, 3 faces and 6 edges. Undo removed it and redo restored it.

This is a two-section direct loft. It does not create an editable parametric feature history or support intermediate sections.

# Revolve around a custom 3D axis

Use **Create → Revolve about custom axis…** to revolve a rectangular radial/axial profile around any axis direction and origin. The direction is normalized. Radial offset and width define the section away from the axis; axial origin and height position it along the axis; sweep angle accepts partial revolutions from 1° through 360°. The radial orientation is chosen deterministically because rotating the section around the axis does not change the result.

The command builds an inspectable preview and adds the resulting solid as one undoable body. This remains a direct-solid operation; it does not add a parametric feature timeline.

## Verification

- Windows x64 Release build passed.
- Focused regression passed 18 assertions for a complete oblique-axis cylinder, analytical volume and centroid, partial 90° revolution, topology, invalid direction rejection, and undo/redo.
- Full CTest passed 41/41; source audit passed 145 checks with all 115 public core operations referenced by regression.
- Live Windows UI created a cylinder around direction (1, 1, 1). Properties reported 6,283.185 mm³, 3 faces, and 6 edges. Live undo and redo removed and restored the body with the same volume and bounds.

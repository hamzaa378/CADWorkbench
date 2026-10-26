# Nonuniform scale

Choose **Modify → Nonuniform scale (world origin)** with one body selected. Enter separate X, Y, and Z multipliers. Positive factors from 0.0001 through 10,000 are accepted. The operation transforms the selected solid about the world origin, preserves its body identity and appearance, and creates one undo step. Equal factors provide uniform scaling; factors of 1 on all axes are a no-op.

## Verification

- Windows Release build passed; focused regression passed 23 assertions covering invalid factors, missing-body rejection, geometry bounds and volume, identity no-op, and undo/redo.
- Full Windows CTest passed 22/22; source audit passed 136 checks and referenced all 98 public core operations from regressions.
- Live UI verified the Modify menu command. Scaling a 40 × 30 × 20 mm box by X=2, Y=0.5, Z=3 changed its volume to 72,000 mm³ and bounds to 80 × 15 × 60 mm. Undo and redo passed.

Nonuniform scale transforms circles and other analytic geometry into general surfaces where needed; it is a direct edit, not a parametric feature-history node.

# Through-all extrusion Cut

Sketch extrusion now offers **Through all (Cut)** for one-sided, untapered cuts. The operation measures the target body's bounds along the chosen sketch-plane normal, extends the cutter slightly past the far side, and subtracts the intersecting geometry. Forward and reverse directions work on XY, XZ, and YZ planes. The fixed extrusion-height field is disabled while this option is selected. Through-all is disabled for New Body and Join, and when symmetric or tapered extrusion is selected.

## Verification

- Focused Windows regression covers through-all Cut in both directions on all three planes, exact volume reduction, valid topology, commit/undo, rejection for New Body, and rejection when the extrusion points away from the target.
- Live UI: on a 40 × 30 × 20 mm box, a 5 × 5 mm XY profile at U=10 mm, V=10 mm, W=0 cut through the full 20 mm body. Volume changed from 24,000 to 23,500 mm³ and topology from 6 faces/24 edges to 10/48. Undo restored 24,000 mm³ and 6 faces/24 edges.
- The dialog disabled the fixed height field when Through all was selected, and disabled Through all for New Body, symmetric, and tapered extrusion.

Choose **Cut**, select a target body, set the sketch plane and profile position, and enable **Through all (Cut)**. The reverse-direction control determines which way the cutter travels. See `ORIENTED_SKETCH_EXTRUSION.md` for plane orientation and offset behavior.

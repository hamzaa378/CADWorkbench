# Linear rounded-slot pattern

CADWorkbench v1.9 can cut an array of rounded slots into a selected solid. Choose **Modify → Linear rounded-slot pattern**.

The dialog sets the first slot center in X/Y, entry Z, slot width, overall length, cut depth, rotation around Z, instance count, and X/Y spacing. There are 2–100 instances. Spacing follows the 2D vector `(Spacing X, Spacing Y)` for each successive instance. Each obround has semicircular ends; overall length includes those ends. The complete pattern is previewed and applied as a single undo step.

## Verification

- Windows Release build succeeded.
- Focused regression: 29 assertions covering invalid input, preview validity, source immutability before commit, material removal, rotated spacing, missed-body rejection, stale-preview rejection, and commit/undo/redo.
- Related CTests: 5/5 passed (dovetail groove, T-slot groove, rounded slot, keyhole slot, and rounded-slot pattern).
- Live UI used a 40 × 30 × 20 mm box and default settings (three slots, 8 mm width, 18 mm overall length, 6 mm depth, 12 mm X spacing). The viewport showed all three cuts. Measured volume changed from 24,000 to 21,655.221 mm³, with 33 faces and 120 edges. Undo restored 24,000 mm³ and 6/24 faces/edges; redo restored 21,655.221 mm³ and 33/120.

The operation cuts one selected body with equally sized, equally spaced slots along a straight 2D vector. It does not include per-instance variation or a parametric feature timeline.

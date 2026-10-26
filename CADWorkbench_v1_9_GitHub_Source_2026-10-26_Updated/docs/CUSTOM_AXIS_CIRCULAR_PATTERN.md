# Custom-axis circular pattern

Choose **Modify → Circular pattern about custom axis** with one body selected. Enter an axis origin, axis direction, number of total bodies, and total angle. The selected body is retained as the first instance; each generated copy is rotated around the supplied axis. Full circles distribute copies without repeating the starting position, while partial arcs include both ends. The operation validates all copies before committing one undo step and preserves source appearance.

Direction is normalized internally. Instance counts from 2 through 1,000 and total angles from 1° through 360° are accepted. The axis origin must be finite and within the document’s supported coordinate range; the direction must be finite and nonzero.

## Verification

Windows Release build passed. The focused regression passed 48 assertions, and the full Windows CTest suite passed 23/23. The source audit passed 136 checks and found regression references for all 99 public `SceneDocument` operations.

Live UI verification passed through **Modify → Circular pattern about custom axis**. A selected 40 × 30 × 20 mm box produced four visible instances around world Z; undo restored one body and redo restored all four. The focused regression covers malformed inputs, unchanged scene on rejected inputs, a four-instance pattern around a custom Z axis, solid validity, per-instance bounds and volume, and undo/redo. See `QA_MATRIX.md` for the project-wide verification record.

## Scope

This creates separate solid body copies. It does not add a parametric feature-history node or support patterned sketches and features.

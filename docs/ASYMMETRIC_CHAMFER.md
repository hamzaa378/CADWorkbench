# Asymmetric edge chamfer

The Modify menu and ribbon provide an asymmetric chamfer for one selected solid edge. Enter the two setback distances in millimeters, inspect the preview, then apply or cancel. Applying creates one undoable model change.

The reference side is chosen deterministically from the edge's adjacent faces in body traversal order. The first distance is applied to that reference face and the second to the opposite face; the UI does not currently let the user pick or swap the reference face explicitly. Distances must be finite and positive, and the geometry kernel may reject values that cannot produce a valid solid.

## Verification (2026-10-26)

- Windows x64 Release build: passed.
- Focused regression: 27 assertions passed, including invalid distances and references, geometry validity, asymmetric dimensions, unchanged symmetric chamfer behavior, and stale-command handling.
- Full CTest: 24/24 passed.
- Live UI: preview and apply passed. On a 40 × 30 × 20 mm box, volume changed from 24,000 to 23,960 mm³; Properties showed 7 faces and 30 edges. Ctrl+Z restored the box and Ctrl+Y restored the chamfer.

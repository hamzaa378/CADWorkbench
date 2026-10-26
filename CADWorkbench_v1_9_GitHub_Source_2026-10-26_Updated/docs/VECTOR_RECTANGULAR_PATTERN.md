# Two-vector rectangular pattern - 2026-10-26

Creates a two-dimensional rectangular array of copies from the selected solid using two independent direction vectors. The direction vectors are normalized; spacing is measured in millimetres along each normalized vector. Counts include the source position on each axis, so the total bodies are `firstCount × secondCount`.

The operation rejects zero or non-finite vectors, parallel directions, invalid spacing/counts, missing source bodies, and patterns exceeding the 1,000-instance limit. New bodies preserve source shape, volume, topology, visibility, and color, receive unique names, and are added as one undoable operation. The existing axis-aligned pattern tool remains available.

## Verification

- Focused regression: passed 44 assertions covering validation/no mutation, diagonal and orthogonal offsets, shape/topology preservation, unique names, undo/redo, and single-axis count behavior.
- Windows Release build: passed.
- Full CTest: passed 27/27, including `cadcore_vector_rectangular_pattern`.
- Source audit: passed 136 checks; all 102 public `SceneDocument` operations have regression references.
- Live Windows UI: selected a 40 × 30 × 20 mm box and applied vectors `(1,1,0)` and `(0,0,1)`, counts `2` and `3`, spacings `10` and `20` mm. The browser showed six bodies. Copy `2-1` reported bounds X 7.07…47.07, Y 7.07…37.07, Z 0…20 mm, volume 24,000 mm³, and 6 faces/24 edges. Undo restored the original single box.

## Use

Select a body, choose **Modify → Rectangular pattern along vectors**, enter two non-parallel directions and the count/spacing for each, then apply. Direction magnitudes do not scale spacing; only their orientation is used.

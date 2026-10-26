# Multi-face shell

The Shell command can remove one or more planar outside faces in a single operation. Choose among the six axis-aligned bounding sides and provide one inward wall thickness. The existing one-face `shellBody` API remains supported; `shellBodyWithOpenings` accepts 1–6 unique side selections. The selected faces must exist as planar faces on the body bounds.

The operation is applied to the existing body as one undoable document edit. Empty lists, duplicate sides, missing planar sides, invalid thickness, kernel failures, and invalid results are rejected without changing the document. Freeform face picking and outward shell thickness are not part of this command.

## Verification

- Windows Release app and core regression targets built successfully.
- Regression checks reject empty and duplicate opening lists without mutation, create valid shells from opposing and adjacent faces, confirm material removal, and undo to the original volume. The existing one-face shell volume regression remains passing.
- Full Windows CTest passed 26/26.
- Source audit passed 136 checks and found regression references for all 101 public `SceneDocument` operations.
- Live UI, opposing faces: on a 40 × 30 × 20 mm box, selected both Z faces at 2 mm thickness. Properties reported 5,280 mm³ and 10 faces/48 edges. Undo restored 24,000 mm³ and 6 faces/24 edges.
- Live UI, adjacent faces: selected +X and +Z at 2 mm thickness. Properties reported 6,216 mm³ and 10 faces/48 edges. Undo restored 24,000 mm³ and 6 faces/24 edges.

To use it, select a body, open **Modify → Shell selected body**, check one or more of the six axis-aligned outside faces, set wall thickness, and apply.

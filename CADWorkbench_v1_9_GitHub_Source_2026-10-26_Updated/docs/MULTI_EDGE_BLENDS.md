# Multi-edge fillet and chamfer

Symmetric selected-edge fillet and chamfer can now process several edges from one body in a single operation. In **Select → Edges** mode, click the first edge and Ctrl+click additional edges. Choose **Fillet selected edges** or **Chamfer selected edges** in the Modify menu or ribbon, enter one shared radius or distance, inspect the preview, and apply or cancel. Applying is one undoable model change.

The feature accepts only unique edges from the same body and geometry revision. Empty, duplicate, mixed-body, missing, or stale references are rejected before the kernel operation. Geometry combinations that the CAD kernel cannot blend produce a preview error without changing the document. Asymmetric chamfer remains a single-edge operation.

## Verification (2026-10-26)

- Windows x64 Release core and UI build: passed.
- Focused regression: 31 assertions passed for two-edge fillet and chamfer, invalid/duplicate/mixed-body/stale/missing selection references, solid validity, material removal, commit, undo, and redo.
- Full CTest: 25/25 passed.
- Live UI multi-edge selection and preview: pending.

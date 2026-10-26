# STEP Import

CADWorkbench can append solid geometry from `.step` and `.stp` files into the current scene. Use **File > Import STEP...** or the **Import STEP** button on the File ribbon. The existing scene remains in place, and each imported solid is added as a separate selectable body. Imported bodies receive names based on the source filename.

The import checks that the file is readable, contains transferable geometry, fits the scene object limit, and produces valid solids with positive volume. The document changes only after every imported solid passes validation. A successful multi-body import is one undoable operation. Invalid files leave the scene untouched.

The importer reads solid geometry. It does not currently preserve STEP assembly hierarchy, source product names, colors, or metadata. Non-solid geometry is rejected. Save the resulting work as a `.cadscene` file to retain the imported bodies in the native project.

## Verification

Windows x64 Release build passed. The CTest suite passed 4/4 tests. The geometry and persistence test exports a two-body STEP fixture, imports it into a document that already contains a body, checks the two imported solid volumes and body names, verifies one-step undo, and confirms malformed input causes no document change.

The File ribbon entry was verified in the live Windows app. Clicking it opened the standard file picker filtered to `*.step` and `*.stp`; canceling left the scene unchanged. Importing a selected file through the UI was not manually exercised; the automated regression tests the core import path without the dialog.

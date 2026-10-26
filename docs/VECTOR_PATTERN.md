# Linear pattern along a vector

The vector linear pattern creates equally spaced copies of a selected body along a user-specified 3D direction. The direction is normalized, so spacing is measured in millimeters regardless of the vector's magnitude. Positive or negative spacing is supported. Body visibility and color are copied to each instance, and the full pattern is one undo/redo step.

Open **Modify > Linear pattern along vector...** or use the Pattern group in the Modify ribbon. Enter direction X/Y/Z, total instances, and spacing.

## Verification (Windows x64 Release, 2026-10-25)

- Focused core regression: **PASS, 17 assertions**, including zero vector, invalid count, zero spacing, missing body, valid diagonal positions, topology, undo, and redo.
- Full CTest: **PASS, 10/10 tests**.
- Public API coverage audit: **PASS, 81/81 operations are referenced by the native regression suite**. This is a source-level coverage check; it does not establish that every UI dialog has been exercised manually.
- Live UI workflow: **PASS**. The selected-body dialog opened with vector, instance-count, and spacing controls. Accepting the defaults added two pattern copies to the model browser and viewport.

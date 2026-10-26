# Oriented sketch-plane extrusion

The sketch-and-extrude command now supports XY, XZ, and YZ sketch planes, with a signed offset W along the selected plane normal and a reverse-direction option for one-sided extrusions. Profile width and depth follow local sketch coordinates U and V, while extrusion height follows the plane normal. Existing XY behavior at W=0 with forward direction remains the default. Symmetric and tapered extrusion, New Body/Join/Cut operations, preview, and undoable commit use the selected plane and offset.

## Verification

- Windows x64 Release build: passed for `CADWorkbench` and `cadcore_tests`.
- Focused regression: `cadcore_tests --oriented-extrusion` passed 249 assertions covering expected bounds and volume for all three planes at W=0 and W=7 mm in forward and reverse directions, symmetric extrusion at W=7 mm, reverse circle/taper geometry, and through-all cuts on all planes in both directions, topology validity, non-mutating preview, commit, undo/redo, and invalid plane/offset rejection.
- Full Windows CTest: 26/26 passed, including `cadcore_oriented_extrusion`.
- Live UI: selected XZ, entered U=5 mm, V=−4 mm, W=0, previewed and applied the 40 × 30 × 20 mm rectangle. Properties reported 24,000 mm³, bounds X=5…45 mm, Y=−20…0 mm, Z=−4…26 mm, 6 faces, and 24 edges.
- Live offset UI: repeated on XZ with W=7 mm. Properties reported 24,000 mm³ and bounds X=5…45 mm, Y=−27…−7 mm, Z=−4…26 mm, 6 faces, and 24 edges.
- Live reverse UI: selected Reverse extrusion direction with XZ at W=7 mm. Properties reported 24,000 mm³ and bounds X=5…45 mm, Y=−7…13 mm, Z=−4…26 mm, 6 faces, and 24 edges.
- Live reverse Cut: cut a 10 × 10 mm profile 5 mm in the reverse direction through the test body. Properties reported 23,600 mm³ and 11 faces/48 edges; Undo restored 24,000 mm³ and the original 6 faces/24 edges.
- Live YZ UI: created a 10 × 8 mm profile at U=10 mm, V=−5 mm, W=3 mm with 12 mm reverse extrusion. Properties reported 960 mm³, bounds X=−9…3 mm, Y=10…20 mm, Z=−5…3 mm, 6 faces, and 24 edges.
- Live XY UI: created an 8 × 6 mm profile at U=−3 mm, V=7 mm, W=−2 mm with 4 mm reverse extrusion. Properties reported 192 mm³, bounds X=−3…5 mm, Y=7…13 mm, Z=−6…−2 mm, 6 faces, and 24 edges.
- Live control behavior: checking Symmetric about sketch plane disables Reverse extrusion direction in the dialog.
- The live desktop workflow exercised XY and YZ with nonzero plane offsets and reverse direction, plus XZ at W=0 and W=7 in both directions. All three orientations are covered by core regression and live UI checks.

To use it, choose Create → Sketch and extrude profile, select a sketch plane, set the profile dimensions and local origin U/V/W, and enable Reverse extrusion direction when needed. Symmetric extrusion disables reverse direction because the resulting geometry is unchanged. Confirm the preview to apply the operation. See `QA_MATRIX.md` for the current project verification snapshot.

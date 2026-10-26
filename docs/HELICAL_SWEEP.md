# Helical sweep

CADWorkbench can create a solid coil or spring by sweeping a circular wire profile around a helical centerline.

## Controls

Open **Create > Helical sweep...** or select **Helical sweep** in the Create ribbon. The dialog accepts:

- Profile radius, in millimeters
- Coil radius, in millimeters
- Pitch, in millimeters per turn
- Number of turns, from 0.25 to 20

Dimensions must be positive, the coil radius must exceed the wire radius, and pitch must be greater than the wire diameter to prevent adjacent turns from overlapping. The result is one solid named **Helical Sweep**, added as an undoable body.

## Verification (Windows x64 Release, 2026-10-25)

- Focused core regression: **PASS, 9 assertions** covering invalid pitch, intersecting coil radius, turn limit, valid solid topology/body name, undo, and redo.
- Full native CTest: **PASS, 5/5 tests**.
- Live UI: **PASS**. The Create ribbon button and Create menu entry were visible; the dialog opened with the expected four controls; submitting the defaults created a visible spring solid and a `Helical Sweep` browser row. Ctrl+Z removed the body and Ctrl+Y restored it.

## Scope

This creates a fixed circular wire around a smooth helical path. It does not provide a parametric feature timeline, variable pitch/radius, tapered coils, flat spring ends, or manufacturing validation. The solid uses Open CASCADE's pipe-shell sweep construction; see the [MakePipeShell reference](https://occt3d.com/dev/doc/refman/html/class_b_rep_offset_a_p_i___make_pipe_shell.html) and [GeomAPI_Interpolate reference](https://occt3d.com/dev/doc/refman/html/class_geom_a_p_i___interpolate.html).

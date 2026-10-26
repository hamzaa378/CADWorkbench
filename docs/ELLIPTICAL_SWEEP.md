# Elliptical profile sweep

Create a solid by sweeping an elliptical cross-section along the application's smooth generated 3D path.

## Controls

Open **Create > Sweep elliptical profile...** or select it from the Create ribbon. Set semi-axis A and B, path length, lateral offset, and vertical rise. Dimensions must be positive; offsets are bounded by the existing sweep limits. The result is one undoable body named **Elliptical Sweep**.

The cross-section is placed perpendicular to the generated path at its start. The larger semi-axis defines the ellipse major radius.

## Verification (Windows x64 Release, 2026-10-25)

- Release build: **PASS**.
- Focused sweep regression: **PASS, 17 assertions** across circular, rectangular, and elliptical sweeps. Elliptical checks include zero-axis rejection, valid topology, body insertion/name, and undo.
- Full CTest: **PASS, 10/10 tests**.
- Live menu/dialog workflow: **PASS**. The sweep dialog opened with semi-axis, path length, lateral offset, and vertical rise controls; accepting defaults created a visible **Elliptical Sweep** body.

## Scope

This is a direct solid sweep along the built-in smooth path. It does not accept a user-drawn sketch path or support a parametric feature timeline.

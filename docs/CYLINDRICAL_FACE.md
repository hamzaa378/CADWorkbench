# Cylindrical face measurement

CADWorkbench v1.28 adds **Inspect > Measure selected cylindrical face** and the matching Inspect ribbon control. With **Faces (inspection)** active, select one cylindrical side face to read its radius, diameter, axis direction, and axis origin. Planar caps and other surface types are rejected. This query does not modify the model or undo history.

The face reference includes the body's geometry token, so a selection made stale by a geometry edit is rejected. The reported axis uses the underlying cylindrical surface's axis in model coordinates.

## Verification

- Windows x64 Release build passed; focused `cadcore_cylindrical_face` regression passed **19 assertions**, including side-surface measurement, planar-cap rejection, invalid/out-of-range/missing/stale face references, transform refresh, and read-only behavior.
- Full CTest and source audit: pending the full-suite run.
- UI command is available from the Inspect menu and ribbon; live result confirmation is pending the reserved UI pass.

See [`FACE_ANGLE.md`](FACE_ANGLE.md) for the companion planar-face angle and separation tool.

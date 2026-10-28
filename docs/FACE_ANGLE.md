# Planar face relationship measurement

CADWorkbench v1.28 adds **Inspect > Measure angle between planar faces** and the matching Inspect ribbon control. Select exactly two faces with **Faces (inspection)** active. The result reports the oriented angle between their unit normals and both normal vectors. When the planes are parallel or opposite-facing, it also reports the parallel-plane separation in millimeters. Curved faces are rejected because they do not have one constant plane normal.

This is a read-only measurement. Face references include the body's geometry token, so the tool rejects selections that became stale after a geometry edit. It does not change the document or undo history.

## Verification

- Windows x64 Release build passed.
- Focused `cadcore_face_angle` regression passed **50 assertions**: adjacent and opposing box-face normals, parallel-plane gaps, unit normals, curved-face rejection, invalid and missing references, stale-reference rejection, and non-mutating behavior.
- The live UI command is available in the Inspect menu and ribbon. Two-face numerical confirmation is pending the reserved live UI verification pass.

The reported angle follows the selected face orientations and ranges from 0° to 180°. The gap is reported only when the plane normals are parallel within a 1e-7 dot-product tolerance.

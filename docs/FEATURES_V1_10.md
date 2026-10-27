# CADWorkbench v1.10.0 modeling update

This update adds two reusable direct-modeling workflows:

1. **Custom 3D sketch-plane extrusion** supports the existing profile library on an origin, normal, and U direction supplied in world coordinates. Join, Cut, taper, symmetric, reverse, and through-all Cut behavior are available where their existing rules allow.
2. **Custom-axis revolution** revolves a rectangular profile around an arbitrary 3D direction and axis origin, with a partial sweep angle.

## Verification status

| Area | Result |
|---|---|
| Windows x64 Release build | PASS |
| Custom-plane extrusion focused regression | PASS — 24 assertions |
| Custom-axis revolve focused regression | PASS — 18 assertions |
| Existing oriented-extrusion regression | PASS — 249 assertions |
| Full Windows CTest suite | PASS — 41/41 |
| Source audit | PASS — 145 checks; 115/115 public core operations referenced |
| Live custom-plane extrusion | PASS — preview, apply, geometry properties, undo, and redo |
| Live custom-axis revolve | PASS — preview, apply, expected mass properties, undo, and redo |

These commands remain direct-solid tools and do not provide Fusion 360 feature-history parity. See [`CUSTOM_PLANE_EXTRUSION.md`](CUSTOM_PLANE_EXTRUSION.md) and [`CUSTOM_AXIS_REVOLVE.md`](CUSTOM_AXIS_REVOLVE.md) for controls and limits.

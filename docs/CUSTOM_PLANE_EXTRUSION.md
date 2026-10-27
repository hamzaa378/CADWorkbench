# Custom 3D sketch-plane extrusion

Use **Create → Sketch and extrude profile… → Custom 3D plane** to place a profile in any plane in world space. Enter the plane origin, a normal vector, and a U direction. The normal and projected U direction are normalized; V is derived as normal × U so the sketch frame stays right-handed. The profile's local U/V origin and signed W offset continue to work in this frame.

All eight supported profiles use the custom plane. New Body, Join, and Cut modes are available. Reverse direction, symmetric extrusion, taper, and one-sided through-all Cut use the same controls as the orthogonal planes. Through-all computes a conservative travel interval from the target body's bounds projected onto the custom normal.

## Verification

- Windows x64 Release build passed.
- Focused regression passed 24 assertions for oblique placement, expected volume and centroid, preview non-mutation, commit/undo/redo, invalid normal and U direction, non-finite origin, and through-all Cut in both directions.
- Existing oriented-extrusion regression passed 249 assertions.
- Full CTest passed 41/41; source audit passed 145 checks with all 115 public core operations referenced by regression.
- Live Windows UI created an angled 40 × 30 × 20 mm profile solid (24,000 mm³; 6 faces and 24 edges). The custom-plane controls displayed, and live undo/redo removed and restored the body.

The custom plane is numeric: this release does not create persistent datum planes or allow selecting an arbitrary planar face as a sketch plane.

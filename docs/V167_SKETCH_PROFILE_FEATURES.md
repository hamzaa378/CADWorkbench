# CADWorkbench v1.67 — Editable polygon-profile solid features

## Source baseline and scope

The user confirmed that CADWorkbench **v1.66 built on Windows and passed 90/90 native CTests**. This source increment retains the existing v1.66 dependency-aware feature system and scene schema v24. It adds **four** genuine parametric polygon-profile operations in the per-body general feature tree. It is not a finished v1.70 release.

### New editable feature nodes

| Feature-tree node | Input | Operation |
|---|---|---|
| Sketch polygon extrude Z | One arbitrary simple straight-edge XY polygon, base Z, positive height | Adds a vertical prism by Boolean union |
| Sketch polygon pocket Z | Same | Cuts a prism; rejects no-material-removed cases |
| Sketch polygon revolve Z | One simple polygon in radius/height coordinates, all radii positive | Rotates the closed XZ section 360° around global Z and fuses it |
| Sketch polygons loft Z | Two simple straight-edge XY polygons with the same number of corresponding vertices, base Z and positive separation | Creates a ruled solid loft between sections and fuses it |

This removes the rectangle/annulus template restriction for these **straight-line** profile families. It does not yet support curved boundaries, internal islands, multiple profiles, arbitrary axes for the polygon operations, or feature links to independently owned constrained sketches. The sweep workflow remains the older template and is not generalized by v1.67.

### UI use

1. Select one **solid body** in the model tree and open **Edit → General feature tree for any body**.
2. Choose **Add feature**, then one of the four `Sketch polygon ...` items.
3. Enter 3–24 vertices with one comma-separated `x,y` coordinate pair per line; the last-to-first edge closes automatically. For revolve, `x` means radius and `y` means axial Z height in millimetres.
4. Optionally click **Draw ... in sketch canvas** for straight-line polygon outlines. Curves and circles are rejected instead of silently discarded.
5. For loft, enter/draw both profiles with the **same number of corresponding vertices and same winding direction**. Set base Z and positive section height.
6. Use **Validate staged geometry (dry run)**; only then commit. Existing dependent nodes will be rebuilt using the v1.66 sparse dependency planner.

### Safety and consistency

- The portable polygon validator rejects non-finite or out-of-bounds coordinates, self-intersections, touches, duplicate/short edges, collinear degeneracies, insufficient area, illegal vertex counts, axis-crossing radial polygons, incompatible loft vertex counts, and reverse winding.
- The node's validated `profile_count` and `p0_x/p0_y` etc. (plus `q...` for lofts) are persisted as numeric feature parameters in the existing scene format. No global schema migration is required.
- OCCT creates exact analytic planar/ruled B-reps from those vertices. The Boolean result is checked for B-rep validity and positive volume before being committed. A pocket must actually remove material.
- Save/reopen rebuilds the feature tree; edited profiles are subject to transactional rebuild and undo/redo. Old scene readers may reject unknown operations; **back up projects before saving v1.67-specific feature trees**.
- As with prior versions, geometric changes can invalidate face/edge anchors on later features, which must then be reselected. Stable topological naming is NOT solved by this increment.

### Verification evidence and limits

- Portable polygon validator: **88/88 assertions** under GCC and Clang; Clang ASan/UBSan execution passed.
- Independent Python OpenCascade B-rep reference: four operations produced valid tools and valid Boolean results with the intended positive or negative volume change.
- Legacy source-contract and release preflight: **18/18 suites, 86 Python unittest assertions** passed.
- CMake parsed as far as the **intentional Windows-only** platform guard on Linux. This cannot verify MSVC, Qt or Windows OCCT API integration.
- Two new native tests are registered: `cadcore_v167_polygon_profile_validation`, `cadcore_v167_sketch_polygon_features`. Expected total: **92** native CTests. **Native Windows build and 92 CTests remain pending.**

### Windows acceptance

From fresh extraction:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 92
```

Targeted:

```powershell
ctest --test-dir .\build -C Release --output-on-failure -R 'cadcore_v167_polygon_profile_validation|cadcore_v167_sketch_polygon_features|cadcore_v166_sparse_feature_regeneration'
```

Verify the UI by creating a solid, adding each new node, editing its coordinates, using undo/redo, reopening a saved project, and checking that unrelated feature branches remain unchanged. Preserve the v1.66 90/90 passing baseline until v1.67 passes.

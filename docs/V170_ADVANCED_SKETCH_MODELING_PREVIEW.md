# CADWorkbench v1.70 development candidate — advanced sketch-based solids

**This is a v1.70 development preview, not completion of the 50-target roadmap.** The last user-confirmed Windows baseline is v1.67, 92/92 CTests. The code in this package must pass an independent MSVC Release compile and all 94 registered native CTests before being called a verified Windows release.

## Four real feature-tree operations

1. **Sketch polygon sweep 3D**: extrudes an editable closed straight-edge polygon along 2–24 piecewise-linear 3D guide points. Cross section is normal to the initial segment.
2. **Sketch polygon sweep spline 3D**: interpolates a smooth B-spline through 3–24 guide points and sweeps the polygon along it. The section is normal to the initial spline tangent. Curved path geometry is generated using `GeomAPI_Interpolate` and `BRepOffsetAPI_MakePipeShell`.
3. **Sketch multi-section loft Z**: creates a solid from 2–8 closed polygon sections at strictly increasing Z elevations; every section must have the same 3–24 vertices and consistent winding.
4. **Sketch polygon revolve axis**: revolves an editable radial/height polygon about a user-defined world-space axis and origin, with an angle from 0 to 360 degrees.

All four features use the existing `ShapeFeatureNode` ID, parameter map, feature stack, dependency planner, dry-run validation, transaction rollback, undo/redo, and scene save/reopen flow. Their geometry is joined to the input solid using a Boolean fuse. The implementation does not claim to be a full independent multi-body parametric sketch system.

## How to use

Open an existing solid body and go to **Edit → General feature tree for any body**. Add one of the four advanced sketch operations. Enter the 2D polygon as one `x,y` pair per line. For a sweep, enter path stations as `x,y,z` per line. For a loft, enter sections beginning `z=8`, followed by its vertices, with `---` between sections. For a revolve, enter `(radius, axial distance)` vertices, axis vector, axis origin and sweep angle. Inputs are in millimetres; angle is in degrees. Use **Validate staged geometry (dry run)** before saving.

Example 3D spline path for a polygon profile: `10,10,8`, `10,10,14`, `10,12,19`, `12,15,24` on separate lines. A square profile can use `-2,-2`, `2,-2`, `2,2`, `-2,2`.

## Design constraints and limitations

- **Profiles are straight-sided closed polygons only**. No arcs/splines in the *cross-section*, internal voids, or surface-attached parameter references.
- Sweep guides accept arbitrary 3D positions but do **not** track separate editable sketches or external datum objects; they are numeric coordinates stored in the feature node.
- The spline is interpolated through the guide positions and may bulge between them. A mathematically valid path does not guarantee a watertight solid; B-rep validity and Boolean fusion are checked, and the feature is rejected when the CAD kernel cannot construct a valid result.
- Loft sections use world XY planes at different Z coordinates, not arbitrary orientations. Matching vertex count and winding are required.
- The revolve polygon is radial/axial, has strictly positive radius, and uses a user-specified axis, but does not yet link to an independent constrained sketch.
- Geometry changes upstream may still break a downstream face/edge anchor; current geometric matching cannot survive arbitrary face splitting/merging.
- No global assembly mating solver or safe CAM postprocessor is claimed.

## Verification and acceptance

Portable C++ validator: GCC and Clang strict C++20 compilation; **322 checks each**. Clang AddressSanitizer/UndefinedBehaviorSanitizer: **322 checks**. New Python source integration: **6 checks**. Independent Python/OpenCascade geometry references: **4 tests**, including spline, line sweep, multi-section loft, and arbitrary-axis revolve. Legacy source-contract suites: **18 suites, 92 checks**. CMake syntax checked on Linux until its intentional Windows-only guard; this is *not* a Windows configure, compile, or GUI smoke test.

Windows (from extracted source root):

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 94
```

The two new native CTests are `cadcore_v170_advanced_profile_validation` and `cadcore_v170_advanced_sketch_geometry`. The geometry test checks new feature creation, cold rebuild, rollback, undo/redo, save/reopen, and restored parameter data for the four operations. Preserve your verified v1.67 folder until all 94 tests pass and you have manually opened the application and tried each dialog.

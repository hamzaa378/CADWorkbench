# CADWorkbench v1.56 development candidate: interactive XY sketches and generic feature stacks

## Scope of this increment

This is a **development candidate**, not a Windows-validated release. The user's prior v1.55 development ZIP passed **73/73 native CTests** on Windows. The new v1.56 changes add one native test binary and therefore expect **74 registered CTest cases**, subject to fresh CMake configuration.

### Interactive 2D sketch canvas

Use **Create > Interactive XY sketch canvas** to click an arbitrary polygon outline, then close it by clicking the starting vertex or pressing *Close outline*. Switch between Polyline, Select/drag, and Circle cutout. Grid snap (1 mm), endpoint snap, wheel zoom, middle-mouse pan, fit-to-sketch, and an in-dialog sketch undo history are included. Choose extrusion depth in millimetres.

The canvas uses the existing `SketchModel`, `SketchSolver`, and `SceneDocument::addConstrainedPolygonExtrusion`; it is not a disconnected presentation tool. The saved sketch includes persistent point/segment/circle identities and fixed-point/radius constraints. For an existing constrained polygon extrusion, use **Edit > Edit selected sketch on canvas**. Invalid or self-intersecting boundaries and overlapping/out-of-bounds cutouts are still rejected by the core model before commit.

**Limitations:** Currently XY profiles only, with straight boundary edges and circular cutouts. Arc, spline, internal wire, surface/plane sketching, fully automatic geometric relations, real-time degrees-of-freedom indication, and freeform constraint dragging are not implemented. The existing constraint editor remains available. Dragging points on a heavily constrained sketch can produce an overconstrained/unsolved profile; no model edit is committed when rebuild fails.

### Shape-independent modifier tree

Use **Edit > General feature tree for any body** or **Modify > Manage** with exactly one body selected. The editor has Add feature, Edit dimensions, Suppress/restore, Move up/down, and Remove commands. The scene browser also shows a collapsed or expanded ordered feature list under each body.

A tree stores a **frozen copy of the source B-rep** and an ordered set of stable-ID modifier nodes. This works on supported existing solids regardless of their origin: box, sphere, cylinder, cone, prism, loft, imported solid, or arbitrary constrained-sketch extrusion. Surface-only bodies support compatible transform nodes; solid-only modifiers report clear errors. These currently implemented operations are:

| Operation | Parameters | Input |
|---|---|---|
| Translate | x, y, z, mm | Solid or surface |
| Rotate X / Y / Z | degrees, around world origin | Solid or surface |
| Uniform scale | factor, world origin | Solid or surface |
| Cylinder cut Z | x, y, bottom z, radius, depth in mm | Solid |
| Fillet all edges | radius, mm | Solid |
| Chamfer all edges | distance, mm | Solid |

Each change rebuilds the whole ordered stack in temporary geometry, validates each intermediate result, and commits **one undoable update** only on success. Failed operations leave the last good shape and nodes untouched. A non-intersecting cylindrical cut is rejected. Parameters, feature IDs, order, suppression, and the frozen source geometry are stored in the version-23 scene format. The current source and the saved result are checked for consistency on reload. Older project schema versions remain readable.

**Important architecture boundary:** This is a *shape-independent, single-body feature stack*, not yet a general multi-body associative dependency graph. It does not reconstruct the original primitive dimensions, preserve persistent face-level topological naming after changes, or automatically propagate upstream sketch edits through later modifiers. Editing a body with a separate direct-modeling command marks its stack **stale**. The UI requires an explicit, undoable rebase onto the current shape, which discards the old nodes. Assemblies whose poses depend on component/joint frames are intentionally excluded from generic modifier editing pending integration. Any active legacy rectangle/sketch history can become stale after applying these downstream modifications.

### Format compatibility

This increment writes `cadworkbench.scene.v23`. Previous scene versions, including v22, can be read by the updated application. Older CADWorkbench binaries cannot open the new v23 files. Retain backups of existing projects before editing them with this candidate. Direct STEP/DXF/SVG/PDF exports are still snapshots rather than associative file links.

## Verification procedure

On a Windows machine with Qt 6, OCCT 8 and Visual Studio MSVC, run:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
.\scripts\build_windows.ps1
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
```

Check that **74/74 CTests pass**, including the new `cadcore_universal_feature_tree` case. If CTest count differs, make sure the build folder was reconfigured using the new CMakeLists.

Manual acceptance is still necessary:

1. Draw and close a concave five-vertex outline, drag a vertex, add a circular cutout, extrude, save, reopen, and edit again.
2. Attempt a self-intersecting profile and an out-of-bounds circle; verify no body is committed.
3. Create a box, sphere, cylinder, imported STEP body, and a surface body. Open their generic feature stacks. Test transforms on all and solid-only rejection on the surface.
4. Add Translate and Scale, reorder them, suppress one, undo/redo, save/reopen, and compare volume/bounds.
5. Add a cylinder cut intersecting a solid, then deliberately move it outside the part. The editor must reject and preserve the previous result.
6. Apply a direct transform outside the feature stack; the audit should mark `FEATURE_TREE_STALE`. Explicitly rebase and confirm the operation is undoable.
7. Open an existing v22 document and the earlier 73 native test fixtures. Run the full existing CTest suite.

Do not claim that the GUI was live-tested or that native Release compilation passed until those steps have been performed.

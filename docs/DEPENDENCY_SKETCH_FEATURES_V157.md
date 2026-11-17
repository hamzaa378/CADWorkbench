# v1.57 — dependent feature graph, plane sketches, curves and face features

**Status:** Windows build candidate. Source-level checks have passed; the new C++ test and GUI behavior have **not** been verified on Windows. The user's *previous* v1.56 source passed **74/74 CTests** on 4 November 2026. This candidate registers **75 CTests**.

## What changed

### 1. Sketch on a datum plane

Create > **Sketch canvas** now includes a plane control with **World XY**, **World XZ**, **World YZ**, and **Custom** choices. Custom supports an origin (X/Y/Z), normal (X/Y/Z) and local twist angle. Sketch coordinates remain local millimetres, and extrusion proceeds along the plane's normal. Select a planar model face in face-selection mode to use **Create > Sketch on selected planar face**. The selected face seeds the sketch's origin and normal; the created extrusion is currently a **new separate body**, **not** an associative face-bound sketch and not automatically fused to its parent. For reproducibility, use datum-plane coordinates when possible.

### 2. Circular arcs and cubic Bézier spline edges

Complete a closed polygon outline in the sketch dialog. Select **Circular arc**, click an existing boundary edge and then click the desired arc midpoint. Or choose **Bézier spline**, click a boundary edge and click two control handles in sequence. Drag visible control handles afterward to refine curvature. Curved edges are stored as persistent sketch entities and built as OCCT circular-arc and cubic Bézier B-rep edges; they are *not* approximated with mesh edges. Self-intersection checking samples the outline, so extremely small/complex intersections may require manual inspection. Exact B-splines/NURBS, trim/extend, fillets, splines through fit points and advanced sketch constraints remain open.

### 3. Feature graph with explicit source dependencies

Select any supported B-rep body, then **Edit > General feature tree for any body**. Each node has a stable ID and optional source connection. **Edit sources** to choose the frozen body (`@base`), an earlier node output, or automatic previous output. **Fuse branches** and **Subtract branches** combine two earlier sources. Cycles, missing references, duplicate inputs and forward references are rejected. Nodes can be edited, suppressed, reordered and removed; the entire candidate graph is computed before being committed, with one undo checkpoint. Reordering may invalidate dependencies and is rejected without changing the body. Current operations include transforms, cuts, all-edge fillet/chamfer, face-normal pocket/boss, and branch Boolean combinations.

### 4. Face-normal pocket and boss

Select a planar face on the model (or enter its 1-based index in the operation). Supply lateral **u/v** offsets on the face, a radius, and depth. A **pocket** removes cylindrical material into the body; a **boss** adds a cylindrical projection. The rebuild checks that the indexed face still exists and is planar, then checks the solid's validity and material effect. **Caution:** face indices depend on the CAD kernel's topological traversal. If an upstream edit changes face ordering, a matching numerical index could refer to a *different* face. This release does **not** provide stable topological naming, geometric face signatures, automatic remapping, or face-key persistence.

### 5. Transactional upstream-to-downstream regeneration

An existing constrained polygon extrusion with a modifier stack is now a *rebuildable upstream sketch source*. Editing its sketch, depth or plane first rebuilds the exact base solid, then replays **all its downstream feature nodes** in graph order. If a later cut, boss, or Boolean fails after the sketch edit, the update fails without changing the model or its saved feature IDs. Successful updates create one undo step. Saved graph IDs, arcs, Bézier control handles, datum planes and source connections use scene schema **`cadworkbench.scene.v24`**. Older v23 records remain readable and default to XY and implicit previous-result dependencies.

## Architectural boundaries

- Dependency graphs are **per body**, with up to 40 feature nodes. The input geometry of a non-sketch body is a frozen B-rep snapshot. This is not yet a general multi-body feature network, topologically named persistent face graph, or a universal parametric history for every legacy command.
- A sketch created *from* a face receives a placement snapshot but is not yet a persistent associative reference to that parent face. Face geometry changes do not automatically move the independent sketch body.
- Existing rectangle-specific parametric workflows remain and are not yet automatically converted to the new graph.
- Feature graphs rebuild the whole branch sequence, rather than incrementally recomputing only dirty nodes. Unsuccessful nodes must be resolved by the user; geometry isn't silently dropped.
- `CADWorkbench` continues to be a Windows-only MSVC + Qt6 + OCCT8 application. Only the native Windows build/tests can establish compile and runtime correctness.

## Windows build and acceptance checklist

Run from the **extracted** source folder in PowerShell:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\build_windows.ps1
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
```

Expected registered tests: **75**. Key new case: `cadcore_dependency_sketch_v157`.

1. Open Create > Sketch canvas. Create a closed outline, set World XZ, add an arc and Bézier edge, and create an extrusion. Check that the orientation and curves are visible.
2. Save to `.cadscene`, close/reopen and edit the sketch. Confirm the control handles and custom plane return.
3. Select the body. Open Edit > General feature tree for any body; add a cutter and then edit the source sketch's width. Check that the cutter remains on the regenerated solid.
4. Undo and redo the sketch update; confirm the resulting volume and feature list return correctly.
5. Add two separate transform nodes fed by `@base`; use Fuse branches and Subtract branches. Confirm the order and shape change correctly.
6. Attempt to point a node at a later node. Check that Save rejects the change without altering the displayed geometry.
7. Select a planar face, use Create > Sketch on selected planar face, and confirm that the sketch is placed in the intended plane **as a separate body**.
8. Try Face normal pocket and Face normal boss on planar faces. Confirm failed cuts are rejected instead of removing features or corrupting the document.
9. Reopen one existing v1.56 `.cadscene` project to exercise v23 migration. Keep a backup because saving it writes the v24 schema, and older binaries cannot be assumed to understand v24.
10. Verify ribbon commands are less truncated on a 1080p screen, and test both common and advanced modeling commands in the unchanged overflow menus.

## Tests in this package

- New `tests/dependency_sketch_v157_tests.cpp`: native OCCT regression for plane extrusion, curved profile persistence, graph Boolean branching, invalid forward references, face feature, sketch/source regeneration, rollback, undo/redo, save/load and invalid project rejection.
- New `tests/dependency_sketch_v157_contract.py`: seven static source/registration checks. These can pass even when C++ does not compile, so they are *supplemental only*.
- All existing v1.56 code and earlier regression targets are retained; the versioned schema check in the v1.56 static contract is updated to recognize v24 and v23 migration.

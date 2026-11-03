# CADWorkbench v1.55 feature-gap comparison

## Counting method

There is no fixed, authoritative count of every feature in a full CAD system: products group and split tools differently, and the scope changes by edition. This comparison therefore uses a stated benchmark of 30 broad capability areas drawn from Autodesk Fusion's design, electronics, generative design, rendering, animation, simulation, manufacturing, and drawing workspaces. The official product pages describe those areas at different levels of detail, so this is a planning benchmark, not a feature-by-feature parity score.

**Current tally:** 6 capability areas implemented, 15 partial, and **9 not yet implemented** in CADWorkbench v1.55, out of these 30 benchmark areas. This is a broad planning benchmark, not a percentage-complete score. Basic BOM/parts-list workflows, readiness checks, model-linked SVG/PDF drawing sheets, XDE STEP assembly exchange, bounded assembly solving, constrained parametric history with optional-step suppression and named all-edge chamfer/fillet, multi-bend DXF with rectangular reliefs, OBJ mesh cleanup, persisted surface bodies, offline document revisions, open manifold OBJ/STL surface intake, surface trim and solid-guided surface splitting, open-shell and solid sewing, thickening, and IGES solid exchange now exist; the remaining limitations are listed below.

The current working tree adds eight extensions beyond that v1.55 baseline: ordered combined chamfer-plus-fillet history, multi-face-shell splitting, definition synchronization of BOM/material data, separate drawing source revisions for geometry and BOM/material data, undoable linear/circular component occurrence patterns, affine named-parameter links, configurable section/detail/parts-list/balloon checklist conventions, and an editable parametric blind-hole cut with named depth.

Separately, v1.9 adds 100 component templates. Across v1.3–v1.9, the release catalog contains 405 dimensioned component templates (30 + 30 + 35 + 70 + 70 + 70 + 100). Component templates are counted separately from modeling-workflow capabilities below.

## Capability areas

| # | Capability area | Status | CADWorkbench v1.55 coverage |
|---:|---|---|---|
| 1 | Sketch geometry, dimensions, and constraints | Partial | A reusable 2D point/segment solver drives trapezoid, triangle, and vertex-defined polygon extrusions. Sketch constraints persist and dimensional/anchor edits rebuild after reopening. The UI does not yet provide canvas drawing, non-fixed constraint creation, arcs, or splines. |
| 2 | Parametric feature history and timeline | Partial | Bounded rectangle → extrusion histories support a circular through-hole with optional X/Y linear pattern or one editable blind-hole cut with named depth. Named dimensions, chained scale-plus-offset links, rebuild, persistence, and undo/redo are available; through-hole, pattern, blind-hole, chamfer, and fillet steps can be suppressed/restored with downstream rebuild where supported. Blind holes cannot yet be patterned. The sequence cannot be arbitrarily reordered, inserted, or edited as a general dependency graph. |
| 3 | Direct solid modeling | Implemented | Primitive, profile, and component solids. |
| 4 | Solid editing and transforms | Implemented | Boolean, holes, patterns, mirror, shell, split, draft, fillet, chamfer, move, rotate, and scale. |
| 5 | Standard component templates | Implemented | 405 dimensioned templates across the v1.3–v1.9 releases. |
| 6 | Assemblies and reusable component occurrences | Partial | Persistent component IDs, occurrences, XYZ placements, undoable linear and world-Z circular patterns of 2–100 total occurrences, explicit source-to-occurrence geometry and shared BOM/material synchronization, rigid/revolute/prismatic parent-child joints, parent-first pose solving, and BOM links; no closed-loop solver, live shared feature history, or subassembly editor. |
| 7 | Assembly joints and motion studies | Partial | Rigid/revolute/prismatic tree constraints, bounded drives, combined parent-first pose updates, and persisted linear keyframe tracks are available; no closed-loop/over-constraint solving, real-time playback, velocity/acceleration analysis, or collision analysis. |
| 8 | Surface modeling | Partial | Core APIs create ruled faces/shells, multi-section lofts, edge extrusions/revolutions, face offsets, and U/V extensions. Connected persisted face/shell bodies can be sewn into one open shell with face-count validation; closed surface sets can be stitched into solids. Solid-guided trimming can keep either side and produce multiple surface bodies; a solid can split a persisted face or multi-face shell (up to 5,000 source faces) into individually selectable face fragments with area-conservation validation. Thickening is available. Sources are preserved and results are appended atomically. A single planar face is extruded to thickness, while other surfaces use kernel offset thickening. Arbitrary free-form splitting, disconnected-shell merging, preview, and editable surface feature history remain absent. |
| 9 | Mesh modeling and repair | Partial | STL/OBJ intake supports configurable weld tolerance, duplicate/degenerate removal, winding correction, optional capped filling of strictly convex planar boundary loops up to 64 edges, and direct persistence of open manifold imports as faceted BRep surface bodies. These surfaces can use common surface operations but do not retain editable triangle connectivity. Self-intersection repair, remeshing, decimation, and hybrid editing remain absent. |
| 10 | Sheet metal and flat patterns | Partial | One-bend SVG and 2–50 bend DXF flat patterns use per-bend rules; DXF supports optional rectangular end reliefs. A scene solid can be created from signed, sampled bend arcs, but exact analytic bend faces, flat/folded associativity, a full feature tree, corner seams, material rule libraries, springback, and tooling are not implemented. |
| 11 | 2D technical drawings and projected views | Partial | A4/A3 SVG and single-page PDF export can combine up to 50 selected bodies, supports first/third-angle orthographic views and an isometric or section view, and records source IDs plus separate geometry and BOM/material revision tokens. Sheets persist placed views, detail/section views, notes, dimensions, and a linked parts list; source revisions can be refreshed, but exports remain snapshots and do not live-update. |
| 12 | Drawing dimensions, annotations, and tolerances | Partial | Overall envelope dimensions, notes, geometry-linked cylindrical diameter callouts with selectable feature references, a BOM-linked parts list, and persistent BOM balloons are available; arbitrary selected-edge dimensions, datums, tolerances, and GD&T are absent. |
| 13 | Photorealistic rendering | Not implemented | No material, lighting, or render workspace. |
| 14 | Appearance and material library | Partial | Body color, saved per-body preset or custom name/density assignments, and mass-property density defaults are supported; extended physical properties and a broad appearance library are absent. |
| 15 | Measurement, selection, and mass properties | Implemented | Body/subshape selection, properties, and mass-property inspection. |
| 16 | 3D camera navigation and standard views | Implemented | Mouse navigation and standard camera views. |
| 17 | Native document save/load and undo/redo | Implemented | Native scene persistence and edit history. |
| 18 | STEP/STL exchange | Partial | STEP XDE preserves supported assembly names, paths, definitions, occurrences, and placements. IGES solid import/export adds flattened legacy exchange; closed STL/OBJ meshes import as faceted solids, and open manifold meshes can be saved as faceted surface bodies. STL export is available. PMI and many other neutral formats are not supported. |
| 19 | DWG/DXF drawing exchange | Partial | Multi-bend sheet-metal flat patterns export as DXF. Drawing sheets export SVG/PDF, but sheet-to-DXF/DWG export and native DWG drawing exchange are absent. |
| 20 | Point clouds and drawing/model underlays | Not implemented | No point-cloud or external-reference workflow. |
| 21 | CNC CAM, toolpaths, and post-processing | Not implemented | No subtractive manufacturing workspace. |
| 22 | Additive build preparation and print simulation | Not implemented | STL export only; no build plate, supports, or print simulation. |
| 23 | Engineering simulation and FEA | Not implemented | No solver or results workspace. |
| 24 | Electronics schematics and PCB design | Not implemented | No ECAD workflow. |
| 25 | Generative design and optimization | Not implemented | No design-space generation or optimization solver. |
| 26 | Cloud sharing and team collaboration | Not implemented | No cloud project or shared review workflow. |
| 27 | Version comparison and change review | Partial | Named offline checkpoints support SHA-256 validation, JSON field-path old/new comparisons, tags, retention pruning that preserves tagged revisions, optional checkpoint-on-save with unchanged-scene deduplication, and restore to a separate file. There is no general feature-operation replay, dependency-aware rebuild, editable timeline, branching, merge, or cloud collaboration. |
| 28 | Automation API, macros, and plugins | Not implemented | No user-facing automation or extension API. |
| 29 | BOM, drawing sheets, and data extraction | Partial | Structured BOM and per-body parts list include procurement data and CSV export; drawing SVGs can include a linked BOM table with up to 18 rows, but there is no broader manufacturing-data workflow. |
| 30 | CAD standards and standards checking | Partial | Configurable project-required title-block fields and eight annotation-presence conventions (including section/detail views, linked parts lists, and BOM balloons) are checked against saved sheets under ISO/ASME readiness profiles; formal standard clauses, licensed tolerance data, datums, placement rules, and GD&T are not validated. |

## Suggested remaining-work order

1. General assembly solving, full occurrence transforms, shared reusable definitions, and motion studies.
2. Model-linked drawing entities with feature dimensions, independently placed section planes/paths, and change propagation.
3. Configurable clause-level ISO and ASME checks, with licensed/project tolerance data supplied by the user.
4. Expand the parametric sketch solver and feature history beyond the current focused rectangle/extrusion/hole-pattern chain.
5. Refine the faceted folded-body approximation into analytic bends, link it to flat-pattern relief and material rules, and add editable feature history.
6. Expand surface work into persisted/editable surface bodies, apply mesh repair to STL import, and support further neutral-file types.
7. Extend checkpoints into a persistent, editable/replayable feature timeline with dependency-aware rebuild and change review.
8. Materials and rendering.
9. CAM, additive preparation, simulation, collaboration, and automation.

## Reference scope

- [Autodesk Fusion workspaces](https://help.autodesk.com/view/fusion360/ENU/?contextId=GS-WORKSPACES) describe Design, Electronics, Generative Design, Render, Animation, Simulation, and Manufacture workspaces.
- [Autodesk Fusion tutorials](https://help.autodesk.com/view/fusion360/ENU/?guid=GS-TUTORIALS) cover assemblies, sketch constraints, solid/surface/form/sheet-metal design, simulation, manufacture, and drawings.
- [Fusion drawing export](https://help.autodesk.com/view/fusion360/ENU/?contextId=DWG-EXPORT) documents PDF, DWG, DXF, and CSV export from the Drawing workspace.
- [Fusion supported file formats](https://help.autodesk.com/view/fusion360/ENU/?guid=TPD-SUPPORTED-FILE-FORMATS) shows broader CAD, drawing, manufacturing, and electronics interchange, with availability varying by license.

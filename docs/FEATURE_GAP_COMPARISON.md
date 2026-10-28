# CADWorkbench v1.48 feature-gap comparison

## Counting method

There is no fixed, authoritative count of every feature in a full CAD system: products group and split tools differently, and the scope changes by edition. This comparison therefore uses a stated benchmark of 30 broad capability areas drawn from Autodesk Fusion's design, electronics, generative design, rendering, animation, simulation, manufacturing, and drawing workspaces. The official product pages describe those areas at different levels of detail, so this is a planning benchmark, not a feature-by-feature parity score.

**Current tally:** 6 capability areas implemented, 13 partial, and **11 not yet implemented** in CADWorkbench v1.48, out of these 30 benchmark areas. This is a broad planning benchmark, not a percentage-complete score. Basic BOM/parts-list workflows, readiness checks, model-linked SVG/PDF drawing sheets, XDE STEP assembly exchange, bounded assembly solving, a constrained parametric hole-pattern chain, multi-bend DXF with rectangular reliefs, closed-manifold STL/OBJ intake, and IGES solid exchange now exist; the remaining limitations are listed below.

Separately, v1.9 adds 100 component templates. Across v1.3–v1.9, the release catalog contains 405 dimensioned component templates (30 + 30 + 35 + 70 + 70 + 70 + 100). Component templates are counted separately from modeling-workflow capabilities below.

## Capability areas

| # | Capability area | Status | CADWorkbench v1.48 coverage |
|---:|---|---|---|
| 1 | Sketch geometry, dimensions, and constraints | Partial | Basic profile workflows exist; no full constraint solver. |
| 2 | Parametric feature history and timeline | Partial | A bounded rectangle → extrusion → circular through-hole → optional X- or Y-direction linear hole pattern has named parameters, sketch constraint records, rebuild, persistence, and undo/redo; no general dependency graph or editable arbitrary feature timeline. |
| 3 | Direct solid modeling | Implemented | Primitive, profile, and component solids. |
| 4 | Solid editing and transforms | Implemented | Boolean, holes, patterns, mirror, shell, split, draft, fillet, chamfer, move, rotate, and scale. |
| 5 | Standard component templates | Implemented | 405 dimensioned templates across the v1.3–v1.9 releases. |
| 6 | Assemblies and reusable component occurrences | Partial | Persistent component IDs, occurrences, XYZ placements, explicit source-to-occurrence geometry synchronization, rigid/revolute/prismatic parent-child joints, parent-first pose solving, and BOM links; no closed-loop solver, live shared feature history, or subassembly editor. |
| 7 | Assembly joints and motion studies | Partial | Rigid/revolute/prismatic tree constraints, bounded drives, combined parent-first pose updates, and persisted linear keyframe tracks are available; no closed-loop/over-constraint solving, real-time playback, velocity/acceleration analysis, or collision analysis. |
| 8 | Surface modeling | Not implemented | No surface-body toolset. |
| 9 | Mesh modeling and repair | Partial | Closed manifold STL and OBJ meshes import as faceted solids with a bounded welding tolerance; no editable mesh body, repair, remeshing, or decimation workflow. |
| 10 | Sheet metal and flat patterns | Partial | One-bend SVG and 2–50 bend DXF flat patterns use per-bend rules; DXF supports optional rectangular end reliefs. Folded sheet-solid modeling, corner seams, material rule libraries, springback, and tooling are not implemented. |
| 11 | 2D technical drawings and projected views | Partial | A4/A3 SVG and single-page PDF export can combine up to 50 selected bodies, supports first/third-angle orthographic views and an isometric or section view, and records source IDs plus an aggregate geometry token. Sheets persist placed views, detail/section views, notes, dimensions, and a linked parts list; source revisions can be refreshed, but exports remain snapshots and do not live-update. |
| 12 | Drawing dimensions, annotations, and tolerances | Partial | Overall envelope dimensions, notes, geometry-linked cylindrical diameter callouts with selectable feature references, a BOM-linked parts list, and persistent BOM balloons are available; arbitrary selected-edge dimensions, datums, tolerances, and GD&T are absent. |
| 13 | Photorealistic rendering | Not implemented | No material, lighting, or render workspace. |
| 14 | Appearance and material library | Partial | Body color, saved per-body preset or custom name/density assignments, and mass-property density defaults are supported; extended physical properties and a broad appearance library are absent. |
| 15 | Measurement, selection, and mass properties | Implemented | Body/subshape selection, properties, and mass-property inspection. |
| 16 | 3D camera navigation and standard views | Implemented | Mouse navigation and standard camera views. |
| 17 | Native document save/load and undo/redo | Implemented | Native scene persistence and edit history. |
| 18 | STEP/STL exchange | Partial | STEP XDE preserves supported assembly names, paths, definitions, occurrences, and placements. IGES solid import/export adds flattened legacy exchange; closed STL/OBJ meshes import as faceted solids and STL export is available. Open surfaces, PMI, and many other neutral formats are not supported. |
| 19 | DWG/DXF drawing exchange | Partial | Multi-bend sheet-metal flat patterns export as DXF. Drawing sheets export SVG/PDF, but sheet-to-DXF/DWG export and native DWG drawing exchange are absent. |
| 20 | Point clouds and drawing/model underlays | Not implemented | No point-cloud or external-reference workflow. |
| 21 | CNC CAM, toolpaths, and post-processing | Not implemented | No subtractive manufacturing workspace. |
| 22 | Additive build preparation and print simulation | Not implemented | STL export only; no build plate, supports, or print simulation. |
| 23 | Engineering simulation and FEA | Not implemented | No solver or results workspace. |
| 24 | Electronics schematics and PCB design | Not implemented | No ECAD workflow. |
| 25 | Generative design and optimization | Not implemented | No design-space generation or optimization solver. |
| 26 | Cloud sharing and team collaboration | Not implemented | No cloud project or shared review workflow. |
| 27 | Version comparison and change review | Not implemented | Undo/redo exists, but no document revision comparison. |
| 28 | Automation API, macros, and plugins | Not implemented | No user-facing automation or extension API. |
| 29 | BOM, drawing sheets, and data extraction | Partial | Structured BOM and per-body parts list include procurement data and CSV export; drawing SVGs can include a linked BOM table with up to 18 rows, but there is no broader manufacturing-data workflow. |
| 30 | CAD standards and standards checking | Partial | Configurable project-required title-block fields and annotation conventions are checked against saved sheets and exported SVG structure under ISO/ASME readiness profiles; formal standard clauses, licensed tolerance data, datums, and GD&T are not validated. |

## Suggested remaining-work order

1. General assembly solving, full occurrence transforms, shared reusable definitions, and motion studies.
2. Model-linked drawing entities with feature dimensions, independently placed section planes/paths, and change propagation.
3. Configurable clause-level ISO and ASME checks, with licensed/project tolerance data supplied by the user.
4. Expand the parametric sketch solver and feature history beyond the current focused rectangle/extrusion/hole-pattern chain.
5. Add folded sheet-solid modeling and richer corner-relief/material-rule handling on top of multi-bend DXF.
6. Add surface-body modeling, editable mesh repair, and support for further neutral-file types.
7. Materials and rendering.
8. CAM, additive preparation, simulation, collaboration, version comparison, and automation.

## Reference scope

- [Autodesk Fusion workspaces](https://help.autodesk.com/view/fusion360/ENU/?contextId=GS-WORKSPACES) describe Design, Electronics, Generative Design, Render, Animation, Simulation, and Manufacture workspaces.
- [Autodesk Fusion tutorials](https://help.autodesk.com/view/fusion360/ENU/?guid=GS-TUTORIALS) cover assemblies, sketch constraints, solid/surface/form/sheet-metal design, simulation, manufacture, and drawings.
- [Fusion drawing export](https://help.autodesk.com/view/fusion360/ENU/?contextId=DWG-EXPORT) documents PDF, DWG, DXF, and CSV export from the Drawing workspace.
- [Fusion supported file formats](https://help.autodesk.com/view/fusion360/ENU/?guid=TPD-SUPPORTED-FILE-FORMATS) shows broader CAD, drawing, manufacturing, and electronics interchange, with availability varying by license.

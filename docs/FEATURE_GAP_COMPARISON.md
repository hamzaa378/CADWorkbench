# CADWorkbench v1.27 feature-gap comparison

## Counting method

There is no fixed, authoritative count of every feature in a full CAD system: products group and split tools differently, and the scope changes by edition. This comparison therefore uses a stated benchmark of 30 broad capability areas drawn from the design, electronics, generative design, rendering, animation, simulation, manufacturing, and drawing workspaces documented for Autodesk Fusion, plus AutoCAD's drafting, 3D, collaboration, and customization areas. The official product pages describe those areas at different levels of detail, so this is a planning benchmark, not a feature-by-feature parity score.

**Current tally:** 6 capability areas implemented, 8 partial, and **16 not yet implemented** in CADWorkbench v1.27, out of these 30 benchmark areas. This is a broad planning benchmark, not a percentage-complete score. Basic BOM/parts-list workflows, readiness checks, and single-body SVG drawings now exist; the remaining limitations are listed below.

Separately, v1.9 adds 100 component templates. Across v1.3–v1.9, the release catalog contains 405 dimensioned component templates (30 + 30 + 35 + 70 + 70 + 70 + 100). Component templates are counted separately from modeling-workflow capabilities below.

## Capability areas

| # | Capability area | Status | CADWorkbench v1.27 coverage |
|---:|---|---|---|
| 1 | Sketch geometry, dimensions, and constraints | Partial | Basic profile workflows exist; no full constraint solver. |
| 2 | Parametric feature history and timeline | Not implemented | Direct-solid changes do not build an editable feature dependency tree. |
| 3 | Direct solid modeling | Implemented | Primitive, profile, and component solids. |
| 4 | Solid editing and transforms | Implemented | Boolean, holes, patterns, mirror, shell, split, draft, fillet, chamfer, move, rotate, and scale. |
| 5 | Standard component templates | Implemented | 405 dimensioned templates across the v1.3–v1.9 releases. |
| 6 | Assemblies and reusable component occurrences | Partial | A derived assembly tree rolls up path-labeled bodies; no persistent component occurrences or independent subassemblies. |
| 7 | Assembly joints and motion studies | Not implemented | No joint solver or mechanism animation. |
| 8 | Surface modeling | Not implemented | No surface-body toolset. |
| 9 | Mesh modeling and repair | Not implemented | No mesh editing workflow. |
| 10 | Sheet metal and flat patterns | Not implemented | No sheet-metal feature or flat-pattern workflow. |
| 11 | 2D technical drawings and projected views | Partial | One-body A4/A3 SVG export supports first/third-angle orthographic views, an isometric or simple section view, and basic dimensions; no persistent editable drawing workspace. |
| 12 | Drawing dimensions, annotations, and tolerances | Partial | Overall envelope dimensions, notes, cylindrical centerlines and diameter callouts are exported; feature dimensions, datums, tolerances, and GD&T are absent. |
| 13 | Photorealistic rendering | Not implemented | No material, lighting, or render workspace. |
| 14 | Appearance and material library | Partial | Body color, saved per-body preset or custom name/density assignments, and mass-property density defaults are supported; extended physical properties and a broad appearance library are absent. |
| 15 | Measurement, selection, and mass properties | Implemented | Body/subshape selection, properties, and mass-property inspection. |
| 16 | 3D camera navigation and standard views | Implemented | Mouse navigation and standard camera views. |
| 17 | Native document save/load and undo/redo | Implemented | Native scene persistence and edit history. |
| 18 | STEP/STL exchange | Partial | STEP solid import/export and STL export are present; STEP import does not preserve assembly hierarchy, source product metadata, or non-solid entities, and other neutral formats are absent. |
| 19 | 2D drafting and DWG/DXF workflows | Not implemented | No native 2D drawing database or DWG/DXF workflow. |
| 20 | Point clouds and drawing/model underlays | Not implemented | No point-cloud or external-reference workflow. |
| 21 | CNC CAM, toolpaths, and post-processing | Not implemented | No subtractive manufacturing workspace. |
| 22 | Additive build preparation and print simulation | Not implemented | STL export only; no build plate, supports, or print simulation. |
| 23 | Engineering simulation and FEA | Not implemented | No solver or results workspace. |
| 24 | Electronics schematics and PCB design | Not implemented | No ECAD workflow. |
| 25 | Generative design and optimization | Not implemented | No design-space generation or optimization solver. |
| 26 | Cloud sharing and team collaboration | Not implemented | No cloud project or shared review workflow. |
| 27 | Version comparison and change review | Not implemented | Undo/redo exists, but no document revision comparison. |
| 28 | Automation API, macros, and plugins | Not implemented | No user-facing automation or extension API. |
| 29 | BOM, drawing sheets, and data extraction | Partial | Structured BOM and per-body parts list include procurement data and CSV export; basic drawing-sheet SVG export is available, but there is no linked drawing BOM or broader manufacturing data workflow. |
| 30 | CAD standards and standards checking | Partial | BOM/model completeness and basic exported-SVG readiness checks run under ISO and ASME labels; they do not validate standard clauses, tolerances, datums, or GD&T. |

## Suggested remaining-work order

1. Persistent assemblies, reusable component instances, and assembly BOM links.
2. Editable multi-part drawings with feature dimensions, datums, tolerances, and detail/section views.
3. Configurable clause-level ISO and ASME checks, with licensed/project tolerance data supplied by the user.
4. Parametric sketches and editable feature history.
5. Native STEP import plus fuller neutral-file workflows.
6. Surface, mesh, and sheet-metal modeling.
7. Materials and rendering.
8. CAM, additive preparation, simulation, collaboration, version comparison, and automation.

## Reference scope

- [Autodesk Fusion workspaces](https://help.autodesk.com/view/fusion360/ENU/?contextId=GS-WORKSPACES) describe Design, Electronics, Generative Design, Render, Animation, Simulation, and Manufacture workspaces.
- [Autodesk Fusion tutorials](https://help.autodesk.com/view/fusion360/ENU/?guid=GS-TUTORIALS) cover assemblies, sketch constraints, solid/surface/form/sheet-metal design, simulation, manufacture, and drawings.
- [Autodesk AutoCAD features](https://www.autodesk.com/products/autocad/features) groups 2D drafting and annotation, 3D modeling and visualization, collaboration, and customization features.

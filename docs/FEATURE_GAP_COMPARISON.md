# CADWorkbench v1.9 feature-gap comparison

## Counting method

There is no fixed, authoritative count of every feature in a full CAD system: products group and split tools differently, and the scope changes by edition. This comparison therefore uses a stated benchmark of 30 broad capability areas drawn from the design, electronics, generative design, rendering, animation, simulation, manufacturing, and drawing workspaces documented for Autodesk Fusion, plus AutoCAD's drafting, 3D, collaboration, and customization areas. The official product pages describe those areas at different levels of detail, so this is a planning benchmark, not a feature-by-feature parity score.

**Current tally:** 6 capability areas implemented, 4 partial, and **20 not yet implemented** in CADWorkbench v1.9, out of these 30 benchmark areas. The body-schedule CSV export provides a first data-extraction workflow, but it is not a structured BOM with component quantities or drawing sheets.

Separately, v1.9 adds 100 component templates. Across v1.3–v1.9, the release catalog contains 405 dimensioned component templates (30 + 30 + 35 + 70 + 70 + 70 + 100). Component templates are counted separately from modeling-workflow capabilities below.

## Capability areas

| # | Capability area | Status | CADWorkbench v1.9 coverage |
|---:|---|---|---|
| 1 | Sketch geometry, dimensions, and constraints | Partial | Basic profile workflows exist; no full constraint solver. |
| 2 | Parametric feature history and timeline | Not implemented | Direct-solid changes do not build an editable feature dependency tree. |
| 3 | Direct solid modeling | Implemented | Primitive, profile, and component solids. |
| 4 | Solid editing and transforms | Implemented | Boolean, holes, patterns, mirror, shell, split, draft, fillet, chamfer, move, rotate, and scale. |
| 5 | Standard component templates | Implemented | 405 dimensioned templates across the v1.3–v1.9 releases. |
| 6 | Assemblies and reusable component occurrences | Not implemented | Bodies are stored independently; no assembly structure. |
| 7 | Assembly joints and motion studies | Not implemented | No joint solver or mechanism animation. |
| 8 | Surface modeling | Not implemented | No surface-body toolset. |
| 9 | Mesh modeling and repair | Not implemented | No mesh editing workflow. |
| 10 | Sheet metal and flat patterns | Not implemented | No sheet-metal feature or flat-pattern workflow. |
| 11 | 2D technical drawings and projected views | Not implemented | No drawing-sheet workspace. |
| 12 | Drawing dimensions, annotations, and tolerances | Not implemented | No drafting annotation or GD&T toolset. |
| 13 | Photorealistic rendering | Not implemented | No material, lighting, or render workspace. |
| 14 | Appearance and material library | Partial | Body color, saved per-body preset or custom name/density assignments, and mass-property density defaults are supported; extended physical properties and a broad appearance library are absent. |
| 15 | Measurement, selection, and mass properties | Implemented | Body/subshape selection, properties, and mass-property inspection. |
| 16 | 3D camera navigation and standard views | Implemented | Mouse navigation and standard camera views. |
| 17 | Native document save/load and undo/redo | Implemented | Native scene persistence and edit history. |
| 18 | STEP/STL exchange | Partial | STEP and STL export are present; neutral-file import is not in this workflow. |
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
| 29 | BOM, drawing sheets, and data extraction | Partial | Body-schedule CSV exports body ID, name, visibility, volume, area, bounds, and center; no component quantity aggregation, BOM metadata, or drawing sheets. |
| 30 | CAD standards and standards checking | Not implemented | No configurable drafting/model standards checker. |

## Suggested remaining-work order

1. Parametric sketches and editable feature history.
2. Assemblies, component reuse, joints, and motion.
3. 2D drawing sheets, projected/section/detail views, dimensions, and annotations.
4. Native STEP import plus fuller neutral-file workflows.
5. Surface, mesh, and sheet-metal modeling.
6. Materials and rendering.
7. CAM, additive preparation, and simulation.
8. Collaboration, version comparison, automation, and data extraction.

## Reference scope

- [Autodesk Fusion workspaces](https://help.autodesk.com/view/fusion360/ENU/?contextId=GS-WORKSPACES) describe Design, Electronics, Generative Design, Render, Animation, Simulation, and Manufacture workspaces.
- [Autodesk Fusion tutorials](https://help.autodesk.com/view/fusion360/ENU/?guid=GS-TUTORIALS) cover assemblies, sketch constraints, solid/surface/form/sheet-metal design, simulation, manufacture, and drawings.
- [Autodesk AutoCAD features](https://www.autodesk.com/products/autocad/features) groups 2D drafting and annotation, 3D modeling and visualization, collaboration, and customization features.

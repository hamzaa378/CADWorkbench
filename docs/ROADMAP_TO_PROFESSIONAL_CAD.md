# CADWorkbench: integrated mechanical-CAD feature roadmap

**Baseline:** the uncommitted v1.55 source snapshot documented in `CONTINUE_HERE.md`.
**Status of this increment:** source changes are present; no Windows Release build or live GUI validation has been performed for the increment. The historical 72/72 results belong to the original source snapshot.

## Product target and comparison

Aim first for a dependable *offline mechanical CAD* workflow, not feature-count parity with Fusion, Onshape, or FreeCAD. A drawing, assembly, or flat-pattern tool becomes professionally useful only when its input can be edited without losing identity, model references rebuild predictably, and saved deliverables report stale data. Keep native scene compatibility, exact engineering units, undo/redo atomicity, and read-only preflight semantics as cross-cutting rules.

- **FreeCAD:** robust parametric sketch/part workflows, reusable assemblies, TechDraw, CAM and FEM workbenches. Reference: <https://freecad.org/>.
- **Fusion:** editable parametric/direct/surface/mesh design alongside assembly, drawing, manufacturing, simulation, and electronics workspaces. Reference: <https://help.autodesk.com/view/fusion360/ENU/?contextId=GS-WORKSPACES>.
- **Onshape:** cloud-native assemblies, associative drawings, BOM, and sheet metal. Reference: <https://www.onshape.com/en/platform>.

This is a feature-area comparison, not a comparative benchmark or certification claim. Some professional workflows, especially CAM, FEA, collaboration, and ECAD, remain out of scope until fundamental design operations are stable.

## Current development increment: cross-workflow foundation

1. **Project health preflight:** `src/core/DocumentAudit.*` performs read-only checks of BOM/solid health, stale sketch/feature histories, stale folded sheet-metal definitions, invalid joint parents/limits, drawing source status, and drawing-edge/balloon references. One project report can be filtered in the UI and exported as UTF-8 CSV or text. Surface-only bodies are not falsely required to have positive solid volume.
2. **Command discovery:** `Ctrl+K` searches currently enabled menu commands, their categories and shortcuts; invokes existing QAction handlers rather than duplicating functionality.
3. **Regression coverage:** `tests/document_audit_tests.cpp` exercises empty documents, metadata warnings, drawing-link freshness, parametric history freshness, undo, report export and CSV escaping. This is a new CTest target, and has **not** been verified in a native Windows build.

The audit is a *triage tool*, not geometry repair, a full interference study, live document recompute, a material-property certification, or a substitute for ISO/ASME inspection. Existing checks may be computationally expensive on large scenes; non-blocking background processing is a later performance task.

## Priority gates (one vertical workflow at a time)

| Gate | Work area | Concrete engineering deliverable | Regression / acceptance criterion |
|---|---|---|---|
| G1 | Sketch canvas | Interactive polyline, circle, arc, construction lines; selection and snap to grid/end/midpoint/center; undoable constraint editing and over/under-constrained visualization | Drag a dimensioned sketch, persist, reopen and re-solve; topology edits maintain saved references where possible |
| G2 | Parametric engine | Generic feature nodes, stable IDs, parent dependency graph, operation insertion/reorder, dirty-node tracking, topological ordering, controlled rebuild and rollback | Change a base dimension; update affected extrusions/cuts only; failed downstream feature leaves last valid scene untouched; undo/reopen preserve graph |
| G3 | Solid modeling | Editable hole/pocket/revolve/loft/sweep/pattern features based on sketch profiles; faces and edges tracked with explicit invalidation | Modify original sketch after downstream features; exact solids validated by OCCT, no silent stale result |
| G4 | Assemblies | Component tree, in-canvas joint selection, local mating frames, grounded components, rigid groups, bounded closed-loop solver, pose dragger | Joint loop converges or explains overconstraint; move a parent and verify children, saved poses and BOM consistency |
| G5 | Technical drawings | Associative view recompute, view-specific section/detail entities, edge/face ordinate and angular dimensions, GD&T as explicit *annotations* | Edit model; mark sheet stale; regenerate views and references without corrupting sheets; exported DXF/PDF remains an immutable snapshot |
| G6 | Sheet metal | Analytic bend profiles, persistent flanges and corners, common bend rule/material library, associatively generated flat patterns | Change bend radius or thickness; folded/flat patterns and bend-table values update together and pass geometric checks |
| G7 | Surface/mesh | Persisted editing features for surface stitch/trim/split; mesh analysis, simplification, self-intersection reporting and controlled conversion | Import pathological meshes safely; repair operations either pass watertightness checks or report exact failure |
| G8 | UI and learning curve | Context-sensitive tasks, editable property grid, model/feature navigator, preview/rollback, consistent units, command search and guided error messages | End-to-end novice workflow without hunting menus; keyboard/mouse tests and accessibility checks |
| G9 | Document reliability | Autosave recovery, external reference provenance, controlled schema migrations and design-graph version diffs | Crash recovery and migration tests; old projects open without data loss; source files remain reproducible |
| G10 | Materials and rendering | Typed material catalog (density and provenance), appearance settings, lighting presets, exportable static renders | Consistent mass/BOM values; render outputs independently verified against model state |
| G11 | CAM / additive | Separate tooling data model, operation/setup tree, safe toolpath preview; postprocessor behind explicit machine profile | Unit/safety checks, collision/dry-run validation before any generated G-code is considered usable |
| G12 | FEA / automation | Verified external solver coupling, meshing and boundary-condition editor; versioned scripting API and macro recording | Reference benchmark comparison, repeatable numerical tolerances, sandboxed automation and rollback |

## Cross-module invariants

- **One source of truth:** parameter edits update dependent features, referenced component geometry, drawings and manufacturing snapshots through explicit revision/freshness states; do not silently rewrite exports.
- **Transactional model edits:** rebuild to temporary shapes, validate, then commit one undo step; rejected operations preserve the document and history.
- **Identity and reference hygiene:** persistent IDs refer to objects, subshape IDs include geometry provenance, and stale references are exposed instead of guessed.
- **Scalability:** set measurable performance targets after collecting real sample assemblies; avoid assuming large-scene performance from tiny tests.
- **Verification:** each gate must include native C++ tests, scene save/reopen fixtures, Windows Release/CTest, GUI interaction, and export/open-in-third-party checks where applicable.
- **Honest scope:** project-level shop/readiness checks are not formal ISO/ASME certification; a CAD kernel is not a validated FEA or CAM solver.

## Required next validation

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\build_windows.ps1
ctest --test-dir .\build -C Release --output-on-failure --parallel 4
```

Verify `Ctrl+K` with keyboard and mouse, disabled-action filtering, modal dialog closing, and command execution. Verify the project audit on an empty scene, a valid single-part drawing, a stale linked sheet, a transformed parametric extrusion, a modified folded body, and an assembly with joints. Check generated CSV in Excel and a text editor. Do not treat historical v1.55 test numbers as verification of these new source changes.

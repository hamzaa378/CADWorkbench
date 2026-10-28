## v1.48.0 parametric Y-axis patterns and schema migration - 2026-10-28

- Extended the focused parametric through-hole pattern to support X or Y. Pattern axis, constraints, history, containment checks, and parameter rebuilds now remain consistent when saved and reopened.
- Bumped the scene schema to v19. Schema-v18 X-only patterns load as X patterns; earlier scene schemas remain supported.
- Fixed the application title to use the CMake project version rather than a stale hard-coded v1.39.0 string.
- Windows x64 Release build passed. The parametric foundation regression passed 113 assertions; full CTest passed 53/53 in 34.99 seconds on the latest rerun. Source audit passed 332 checks, covering all 145/145 public core operations. Live UI Y-pattern verification passed with two visible holes and the expected 21,989.381 mm³ volume; the rebuilt app launched as v1.48.0.

## v1.47.0 parametric hole patterns, sheet-metal reliefs, and IGES exchange - 2026-10-28

- Added an editable parametric linear through-hole pattern along +X. Count (2–100) and spacing persist in scene schema v18; geometry rebuilds validate containment and non-overlap as one undoable operation. Schemas v1–v17 remain readable.
- Added optional rectangular end-relief notches to multi-bend DXF flat patterns, with geometric bounds checks and editable width/depth in the export dialog.
- Added IGES solid import/export and File menu actions. Imports validate each solid and commit atomically; IGES exchange is flattened and does not preserve product hierarchy or PMI. Linked the current exchange-scope documentation.
- Added closed OBJ polygon-mesh import as faceted solids, including positive/negative vertex indices, triangulation, sewing-tolerance validation, and rejection of open or malformed meshes.
- Windows x64 Release build passed. Focused regressions passed: parametric foundation 96 assertions, sheet metal 41, IGES exchange 15, OBJ mesh import 17. Full CTest passed 53/53 in 45.64 seconds; source audit passed 332 checks with 145/145 public core operations referenced. Live UI tests were not run by request.

## v1.46.0 assembly pose solving and motion tracks - 2026-10-28

- Added prismatic joints with persisted normalized axes, travel limits, driven offsets, and full rigid-descendant translation. Revolute drives now carry complete rigid/revolute/prismatic descendants and transform nested joint frames.
- Added a parent-first assembly pose solver that validates and applies mixed revolute/prismatic targets atomically as one undoable edit.
- Added persistent joint motion keyframes with linear interpolation, range checking, undo/redo, and UI actions to edit tracks and evaluate a study time.
- Added explicit synchronization of edited reusable component geometry to all occurrences while preserving XYZ placements.
- Bumped the native scene schema to v17; schemas v1-v16 remain supported. Also fixed the missing Open CASCADE STL reader include and corrected the STL test's diagnostic field names so the existing closed-mesh import builds and tests.
- Added **File → Refresh saved drawing source link...** with undo/redo. ISO/ASME model readiness checks now inspect project-configured fields and annotation conventions on saved sheets, and report stale/missing source warnings without claiming formal compliance.
- Windows x64 Release build passed; full CTest passed 51/51 in 45.19 seconds. Focused regressions passed: assembly joints/motion 133, persistent assembly/drawing links 104, BOM/standards 93, orthographic drawings 120, STL import 17, parametric foundation 77, multi-bend sheet metal 34, and STEP assembly import 28. Source audit passed 332 checks with 141/141 public core operations covered. Live UI was not run by request.

## v1.44.0 atomic parametric edits and drawing source status - 2026-10-28

- Added `setNamedParameters` to validate and rebuild multiple values in the rectangle/extrusion/through-hole parametric chain in one transaction. Invalid combined values leave the solid and parameter map unchanged; undo/redo operates on the complete edit. The Edit menu now presents all named dimensions in one dialog.
- Added **File → Check saved drawing source...** to report whether a sheet's linked bodies are current, stale, or missing. The check is read-only; SVG/PDF files remain export snapshots.
- Parent translation and rotation now carry rigid/revolute descendants recursively, transform saved revolute axes/origins, and remain one undo/redo operation.
- Windows x64 Release build passed. Focused regressions passed: parametric foundation 59 assertions, assembly joints 71, persistent assembly/drawing source status 101, and orthographic drawings 120. Full CTest passed 50/50 in 48.58 seconds. Source audit passed 309 checks with 132/132 public core operations covered. Live UI testing was not run by request.

## v1.43.0 persistent drawing section and detail views - 2026-10-28

- Added **File → Edit saved detail views...** to create, position, and remove up to four circular details linked to the sheet's TOP, FRONT, or RIGHT projected view. The SVG marks the source capture circle, clips enlarged projected edges into the detail circle, adds a leader when separated, and carries stable annotation/source-view metadata.
- Added **File → Edit saved section views...** for up to four separately positioned X/Y/Z axis-aligned cuts, with independent depth/placement, source-view cut lines, section letters, and hatched projected geometry.
- Cylindrical diameter callouts now include stable geometry-derived dimension IDs and source-feature references in the SVG. Re-exporting unchanged geometry preserves the identifiers, and moving/changing the source cylinder changes its feature reference and measured callout.
- Scene schema v15 persists detail and section records. Schemas v1-v14 continue to load; v14 retains detail views and gets empty section defaults. Migration regressions preserve v13 balloons and project checklist policy.
- Windows x64 Release build passed; the final full CTest run passed 50/50 in 45.94 seconds. Focused regressions passed: persistent assembly/drawing settings 92 and orthographic drawing/PDF/detail/section/feature-reference tests 120 assertions. Source audit passed 292 checks with all 131 public core operations referenced. Live UI testing was not run by request.

## v1.42.0 orthographic drawing PDF export - 2026-10-28

- Added **File → Export orthographic drawing PDF...**. It exports the same persisted sheet setup and source-scoped content as SVG to a single A4/A3 landscape PDF page using Qt SVG and PrintSupport.
- PDF writing is atomic. The regression checks both paper sizes, PDF completeness, malformed SVG rejection, and preservation of an existing destination.
- Windows x64 Release build passed; full CTest passed 50/50 in 46.44 seconds. Orthographic drawing regression passed 109 assertions. BOM/standards 83, STEP XDE 28, sheet metal 14, parametric foundation 51, joints 32, persistent assembly/drawing settings 81. Source audit passed 272 checks with 131/131 public core operations referenced. Live UI testing was not run by request.

## v1.41.0 persistent project drawing checklist defaults - 2026-10-28

- Project ISO/ASME drawing readiness requirements can now be saved in the scene, restored on reopen, and changed with undo/redo. The review dialog starts with the saved fields/conventions and offers an explicit save-as-project-defaults option.
- Scene schema v13 stores the drawing checklist; schema v1-v12 load with empty defaults. Invalid or duplicate policy keys and malformed persisted settings are rejected transactionally.
- Windows x64 Release build passed; full CTest passed 50/50 in 43.65 seconds. BOM/parts/standards regression passed 83 assertions; persistent assemblies 81; STEP XDE import 28; sheet-metal flat pattern 14; parametric foundation 51; joints 32; orthographic drawings 98. Live UI testing was not run by request.

## v1.40.0 persistent BOM balloons for multi-body drawing sheets - 2026-10-28

- Added stable-ID BOM balloon records to drawing sheets and scene schema v12, with schema-v1-v11 backward loading.
- Added **File → Edit saved BOM balloons...** to add, position, and remove up to 50 source-linked balloons. Each exported balloon carries its source body ID and item number from the sheet's source-scoped BOM.
- Windows x64 Release build passed; full CTest passed 50/50 in 43.22 seconds. Focused regressions passed: parametric foundation 51 assertions, assembly joints 32, persistent assembly/drawing settings 81, and orthographic drawings 98. Source audit passed 263 checks with 130/130 public core operations referenced. Live UI testing was not run by request. See `QA_MATRIX.md` and `local_verification.json`.

## v1.39.0 parametric hole, assembly joints, and multi-body drawings - 2026-10-28

- Extended the bounded parametric rectangle/extrusion chain with a constrained circle sketch and editable through-hole cut. Diameter and center coordinates are named parameters, and the four-step history rebuilds with undo/redo and stale-history protection.
- Added persistent rigid and revolute parent-child joint records. Revolute joints store an axis and limits and can be driven; invalid references, cycles, self-joints, duplicates, and invalid limits are rejected.
- Drawing export can use up to 50 selected bodies as a compound source. Per-sheet settings and SVG metadata carry source body IDs and an aggregate geometry revision. Existing exports remain snapshots.
- Added multi-body source comparison to the drawing checker. It compares selected or persisted source-body IDs and the aggregate geometry revision, and reports source-set mismatch or stale geometry.
- Windows x64 Release build passed; full CTest passed 50/50 in 44.07 seconds; focused regressions passed: parametric 51 assertions, persistent assemblies 75, joints 32, and orthographic drawings 97. Source audit passed 257 checks with 130/130 public core operations referenced. Live UI testing was not run by request. See `QA_MATRIX.md` and `local_verification.json`.

## v1.36.0 linked drawing parts-list mass column - 2026-10-28

- Added a per-unit mass (kg) column to the BOM-linked drawing table. The value comes from the BOM's unit mass, preserving material-density calculations already used by BOM and mass-property workflows.
- SVG rows expose `data-bom-unit-mass-kg` alongside the item number and component-definition identity. No density assigned produces the BOM's existing zero-mass value; the drawing does not infer material density.
- Windows x64 Release build passed; full CTest passed 49/49; source audit passed 250 checks (126/126 public core operations referenced). Live v1.36 export showed the mass column and a zero unit mass for occurrences without assigned material density. The regression fixture verifies a nonzero 0.024 kg value. See `docs/QA_MATRIX.md` and `docs/local_verification.json`.

## v1.35.0 BOM-linked drawing parts list - 2026-10-28

- Added stable-ID linked parts-list placement to drawing sheets and scene schema v11. Schema v1-v10 scenes remain loadable; schema-v10 migration preserves dimensions and adds a hidden default table record.
- Added **File → Place linked drawing parts list...** to the menu and ribbon. The editor saves visibility and paper-space placement for a selected body's sheet.
- SVG export rebuilds up to 18 table rows from the current grouped BOM, including item, part number, description, quantity, material, revision, and component-definition identity. Existing SVGs remain export snapshots.
- Windows x64 Release build passed; full CTest passed 49/49; source audit passed 250 checks (126/126 public core operations referenced). Live Windows UI verified setup and a real SVG export. See `docs/QA_MATRIX.md` for the bounded result and output evidence.

## v1.34.0 persistent editable overall drawing dimensions - 2026-10-27

- Added stable-ID X/Y/Z overall dimension records to drawing sheets and scene schema v10. v1-v9 scenes remain loadable and receive default dimension records during migration.
- Added **File → Edit saved drawing dimensions...** in the menu and ribbon to set each dimension-line offset from 5 to 60 mm. Dimension values remain derived from the linked body's overall projected envelope.
- SVG output identifies each dimension annotation and records its saved paper-space offset. Dimensions remain associated with overall body bounds, not selectable feature edges or parametric features.
- Windows x64 Release build passed. Focused regressions passed 68 persistent-assembly/drawing-setup assertions and 78 orthographic-drawing assertions; full CTest passed 49/49. Live UI was not run.

## v1.33.0 persistent positioned drawing notes - 2026-10-27

- Added stable-ID note records with printable text and paper-space X/Y placement to persistent drawing-sheet settings. Scene schema v9 stores each note and its position; v1.34 continues this in schema v10.
- Added **File → Edit saved drawing notes...** in the menu and ribbon. The editor handles up to five notes and re-links the sheet to the body's current geometry when saved.
- SVG output now includes each note's identity and placement, retains stable sheet/view identities, and escapes user text. Notes remain vector text; dimensions are still generated annotations.
- Windows x64 Release build passed. Focused regressions passed 63 persistent-assembly/drawing-setup assertions and 77 orthographic-drawing assertions; full CTest passed 49/49. Live UI was not run.

## v1.32.0 persistent placed-view drawing sheets - 2026-10-27

- Added persistent drawing-sheet and projected-view identities to scene schema v9, including the source geometry token and editable paper-space centers for top, front, right, and auxiliary views.
- Added **File → Arrange saved drawing views...** so users can reposition the four views and explicitly refresh a stale sheet link to the selected body's current geometry.
- SVG export now uses saved placements and writes sheet/view IDs into the output. It remains a vector snapshot and does not live-update after later model edits.
- Windows x64 Release build and full CTest passed 49/49 before the v1.33 note addition. The isolated v1.32 source audit and live UI were not run.

## v1.31.0 parametric foundation, STEP assemblies, sheet-metal flat pattern - 2026-10-27

- Added an origin-constrained XY rectangle + base extrusion with named width/height/depth parameters, a two-step editable history, schema-v8 persistence, parameter editing, undo/redo, and stale-history protection after direct geometry edits.
- Switched STEP import/export to OCCT XDE. Assembly paths, names, reusable definitions, occurrence identities, and placements round-trip when present in the STEP data.
- Added one-bend sheet-metal development and SVG flat-pattern export with thickness, inside radius, bend angle, K-factor, bend line, and clear calculation limits.
- Added configurable required title-block and annotation checks per drawing review, plus stale/wrong-body source snapshot detection.
- Focused regressions passed: STEP XDE 28 assertions, parametric foundation 33, sheet metal 14, and drawing checklist/source comparison 72. The Windows x64 Release build passed, full CTest passed 49/49, and source audit passed 250 checks with all 126 public core operations referenced. Live UI was not run for this update; see `QA_MATRIX.md`.

## v1.28.0 planar face relationship measurement - 2026-10-27

- Added a face-inspection command that measures the angle between two planar face normals and reports plane separation when the faces are parallel.
- Added 50 focused regression assertions for box face angles, parallel gaps, invalid/stale references, curved-face rejection, and read-only behavior.
- Added cylindrical-face radius, diameter, and axis inspection; its focused regression passed 19 assertions for cylinder side faces, planar cap rejection, stale selections, and read-only behavior.
- See `FACE_ANGLE.md` for usage, verification, and limits.

## v1.27.0 drawing issue-date validation - 2026-10-27

- Drawing export now rejects nonempty issue dates outside `YYYY-MM-DD`; the exported-SVG checker flags malformed dates as `DRAWING_ISSUE_DATE_FORMAT`.
- The Windows x64 Release build passed; focused drawing regression passed 64 assertions, full CTest passed 43/43, and source audit passed 222 checks.
- Final-app live export with revision A and date `2026-10-27` passed ISO and ASME screens with 0 errors and 0 warnings. An external SVG with `27/10/2026` produced 1 date-format warning under each profile; details state that clause compliance is not checked.

## v1.26.0 derived assembly hierarchy report - 2026-10-27

- Added an Assembly structure report tab that derives parent/child nodes from slash-separated assembly paths and rolls up descendant quantity, total volume, and total mass.
- Live Windows verification confirmed `Powertrain/Drive` shows quantity 1 / 24,000 mm³, then quantity 2 / 48,000 mm³ after duplicating the test part. The unsaved test duplicate was discarded.
- Windows x64 Release build passed; full CTest passed 43/43; source audit passed 219 checks. ISO/ASME drawing checks and editable drawing revision were also verified in the preceding v1.25 UI.
- The hierarchy is a report view derived from text paths; persistent assembly objects and independent subassembly definitions remain unimplemented.

## v1.25.0 editable drawing revision - 2026-10-27

- Added a drawing-specific revision field to the SVG title-block dialog; it defaults to the selected part's revision and validates length and control characters.
- The revision is XML-escaped in the SVG. Drawing readiness screens can now pass without a missing-revision warning when the field is supplied.
- Windows x64 Release build passed; focused drawing regression passed 61 assertions, BOM/parts/standards regression passed 69 assertions, full CTest passed 43/43, and source audit passed 217 checks.
- Live Windows UI exported a revision-A sheet; ISO and ASME content screens both returned 0 errors and 0 warnings. They remain content screens, not standards certification.

## v1.24.0 editable drawing title-block identity fields - 2026-10-27

- Added drawing number, drawn-by, checked-by, and issue-date fields to the orthographic SVG export flow and title block. Empty drawing numbers use the part number; user text is validated and XML-escaped.
- The SVG readiness screen reports a missing drawing number. The UI is tested live: export retained notes and entered title-block fields, and XML parsed successfully. ISO and ASME drawing screens each returned 0 errors and 1 expected revision warning, with explicit non-certification scope details.
- Windows Release build passed; focused drawing regression passed 60 assertions, BOM/parts/standards regression passed 69 assertions, full CTest passed 43/43, and the source audit result is recorded in `docs/local_verification.json`.
- The export remains a static SVG with envelope dimensions. ISO 7200 certification, ISO tolerances, ASME Y14, datums, and GD&T remain unimplemented.

## v1.22.0 assembly-aware BOM paths - 2026-10-27

- Added slash-separated per-body assembly paths, editable with part number, description, and revision. BOM and parts-list reports and CSV exports now include the path; the BOM keeps identical part definitions in different paths separate.
- Native scenes save as schema v4; older v1-v3 files continue to load with empty assembly paths. Duplicate, pattern, and mirror operations carry path metadata.
- Orthographic SVG export accepts up to five user-entered general-note lines, validates their length and printable text, and XML-escapes notes in the output.
- Windows Release build and full CTest passed (43/43); BOM/standards focused regression passed 53 assertions; drawing regression passed 54 assertions; source audit passed 205 checks with all 118 public core operations covered.
- Live UI verified assembly-path editing/reporting and schema-v4 save/reopen. General notes were entered during SVG export; the saved file contains both notes, and ISO and ASME readiness screens each returned 0 errors and 2 expected metadata warnings.
- Assembly paths are grouping metadata. An editable assembly tree, recursive subassembly rollups, purchased parts, and ERP/MRP integration remain outside this update.

## v1.21.0 projection-convention symbol - 2026-10-27

- Orthographic sheets now include a schematic truncated-cone/circle projection symbol. The circular end view is placed to the right for first-angle and to the left for third-angle projection.
- The symbol is separately identified in SVG and covered by regressions for both conventions; it is a drawing aid, not a claim of ISO/ASME symbol conformance.
- Windows x64 Release rebuild passed; focused drawing regression passed 41 assertions; full CTest passed 43/43; source audit passed 196 checks with all 118 public core operations covered. Live UI exports passed for A4 first-angle and third-angle sheets; SVG inspection confirmed the matching symbol placement and Ø20.000 mm cylinder callout.

## v1.20.0 cylindrical diameter drawing callouts - 2026-10-27

- Orthographic SVG export now adds a deduplicated leader callout for the actual diameter of end-on cylindrical features. This complements the existing overall envelope dimensions, cylinder centerlines, and center marks.
- Focused drawing regression passed 40 assertions; full Windows x64 Release CTest passed 43/43; the source audit and live Windows export confirmed a 10 mm radius cylinder is annotated `Ø20.000 mm`.
- BOM, parts-list, combined checks, and individual ISO/ASME drawing-readiness checks were also exercised live. These checks report readiness and data issues and do not claim ISO or ASME compliance.
- See `ORTHOGRAPHIC_DRAWINGS.md`, `STANDARDS_CHECKS.md`, and `QA_MATRIX.md` for scope and remaining work.

## v1.10.0 custom-axis modeling - 2026-10-27

- Added sketch extrusion on arbitrary 3D planes, including world origin, normal, and projected U direction. Existing profiles, Join/Cut, taper, symmetric/reverse modes, and through-all cuts use the custom frame.
- Added revolution of rectangular profiles about arbitrary 3D axes with a configurable axis origin, direction, and partial sweep angle.
- Windows x64 Release build passed. Focused regressions passed 24 custom-plane assertions and 18 custom-axis revolve assertions; the existing oriented-extrusion regression passed 249 assertions. Full CTest passed 41/41 and the source audit passed 145 checks with all 115 public operations referenced.
- Live UI verified both commands, their previews, resulting solid properties, and undo/redo. See `FEATURES_V1_10.md` for detailed results and `CUSTOM_PLANE_EXTRUSION.md` / `CUSTOM_AXIS_REVOLVE.md` for behavior.

## Distance between selected vertices - 2026-10-26

- Added a read-only straight-line distance query and an Inspect menu/ribbon action for two picked vertices.
- Focused regression passed 12 assertions; Windows x64 Release app and tests built; full CTest passed 37/37; source audit passed 136 checks with all 112 public scene operations referenced.
- Live UI confirmed command availability and the insufficient-selection guard. Numeric two-point UI verification remains pending because the desktop input interface cannot Ctrl-click to keep two vertices selected. See `VERTEX_DISTANCE.md`.

## Selected vertex coordinates - 2026-10-26

- Added viewport vertex picking and X/Y/Z coordinate reporting in Inspect, with geometry-token validation that rejects stale vertex references after body edits.
- Focused regression passed 59 assertions; Windows x64 Release app and test targets built; full CTest passed 36/36; source audit passed 136 checks with all 111 public scene operations referenced.
- Live UI picked a box corner and reported X 0.000, Y 0.000, Z 20.000 mm. See `VERTEX_COORDINATES.md`.

## Selected edge length - 2026-10-26

- Added viewport-picked edge length measurement to the Inspect menu and ribbon, with geometry-token validation that rejects stale edge references after body edits.
- Focused regression passed 38 assertions; Windows x64 Release app and test targets built; full CTest passed 36/36; source audit passed 136 checks with all 111 public scene operations referenced.
- Live UI picked edge 5 on a 40 × 30 × 20 mm box and reported 20.000 mm, matching the box's Z extent. See `EDGE_LENGTH.md`.

## Selected face area - 2026-10-26

- Added a viewport-picked face area measurement to Inspect, with geometry-token validation that rejects stale face references after a body edit.
- Focused regression passed 12 assertions; Windows x64 Release build passed; full CTest passed 34/34; source audit passed 136 checks with all 109 public scene operations referenced.
- Live UI picked the top face of a 40 × 30 × 20 mm box and reported 1,200.000 mm². See `FACE_AREA.md`.

## Interference volume inspection - 2026-10-26

- Added a read-only core query and Inspect menu/ribbon command that computes the shared solid volume of two selected bodies.
- Focused Windows regression passed 27 assertions, including overlap centroid and contained-solid volume; isolated Windows Release build and Qt runtime staging passed; full CTest passed 29/29. Source audit passed 136 checks, with all 104 public `SceneDocument` operations referenced. Live UI verification is pending. See `INTERFERENCE_VOLUME.md`.

## Minimum distance inspection - 2026-10-26

- Added a read-only core measurement and Inspect menu/ribbon command for the minimum clearance between two selected bodies, including one closest-point pair.
- Focused Windows regression passed 23 assertions, including face-touching bodies; full CTest passed 29/29. Source audit passed 136 checks, with all 104 public `SceneDocument` operations referenced by regressions. Live UI verification is pending. See `MINIMUM_DISTANCE.md`.

## Two-vector rectangular pattern - 2026-10-26

- Added two-direction rectangular body arrays with arbitrary nonparallel vectors, normalized directions, independent counts/spacings, a 1,000-body limit, and atomic undo.
- Focused regression passed 44 assertions; Windows build passed; full CTest passed 27/27. Source audit passed 136 checks with all 102 public scene operations referenced. Live UI created six bodies from a box, verified translated bounds/volume/topology, and Undo restored the single source body. See `VECTOR_RECTANGULAR_PATTERN.md`.

## Multi-face shell - 2026-10-26

- Extended Shell to remove one or more unique planar outside faces in one operation; the existing one-face command remains available.
- Windows Release app and regression targets built. Regression covers empty/duplicate rejection, no mutation on rejection, opposing and adjacent face shell validity/material removal, and undo. Full CTest passed 26/26; source audit passed 136 checks with all 101 public scene operations referenced. Live UI verified opposing Z faces at 2 mm (24,000 → 5,280 mm³) and adjacent +X/+Z faces at 2 mm (24,000 → 6,216 mm³), with undo restoring the original in both cases.
- Live UI shell on a 40 × 30 × 20 mm box with both Z faces open and 2 mm walls produced 5,280 mm³ (10 faces/48 edges); Undo restored 24,000 mm³ (6/24). See `MULTI_FACE_SHELL.md`.

## Through-all extrusion Cut - 2026-10-26

- Added a Through all (Cut) end condition for one-sided sketch cuts. It adapts cutter length to the selected target and supports forward/reverse direction on XY, XZ, and YZ planes.
- Focused regression covers six plane/direction combinations, volume and topology, commit/undo, and invalid operation/direction handling. Live UI verified a 500 mm³ cut and Undo restoration on a 40 × 30 × 20 mm box.
- Windows Release app and tests built; full CTest passed 26/26.

## Oriented sketch-plane extrusion - 2026-10-26

- Added XY, XZ, and YZ plane selection, signed W offset along the chosen normal, and reverse-direction control to sketch extrusion. Profile coordinates use local U/V; symmetric mode disables reverse direction.
- Windows Release build passed; focused regression passed 202 assertions, including symmetric extrusion at nonzero plane offsets and reverse circular/tapered geometry; full CTest passed 26/26; source audit passed 136 checks with all 100 public scene operations referenced by regression.
- Live UI verified XY, XZ, and YZ orientations, nonzero offsets and reverse directions, XZ at W=0 and W=7 in both directions, and symmetric-mode disabling reverse. See `ORIENTED_SKETCH_EXTRUSION.md`.
- Live reverse Cut also passed: volume changed from 24,000 to 23,600 mm³, and Undo restored the initial volume and 6-face/24-edge topology.
- Live XY and YZ reverse extrusions at nonzero plane offsets passed with expected volume and X/Y/Z bounds; all three sketch-plane orientations now have live UI coverage.

## Elliptical sweep and tapered spring - 2026-10-25

- Added solid sweeps with an elliptical profile, including validation and undo coverage.
- Added tapered coil springs with start/end coil radii plus pitch and turn controls.
- Windows Release build passed; focused regressions passed 17 sweep assertions and 10 tapered-helix assertions; full CTest passed 7/7. Live UI click-through is pending. See `ELLIPTICAL_SWEEP.md` and `TAPERED_HELIX.md`.

## Variable-pitch spring - 2026-10-25

- Added a smooth variable-pitch circular-profile coil with independent start and end pitch controls, in the Create menu and ribbon.
- Focused Windows regression passed 10 assertions and full CTest passed 6/6. Release build succeeded. Live UI click-through remains pending because the app could not be relaunched through the available desktop app list. See `VARIABLE_PITCH_HELIX.md`.

## Helical sweep - 2026-10-25

- Added configurable circular-profile solid coils with wire radius, coil radius, pitch, and turn count controls in the Create menu and ribbon.
- Focused Windows regression passed 9 assertions; full CTest passed 5/5. Live UI verified the dialog, visible solid creation, and Ctrl+Z/Ctrl+Y undo/redo. See `HELICAL_SWEEP.md`.

## Profile sweep tools - 2026-10-25

- Added circular-profile and rectangular-profile solid sweeps along a smooth interpolated 3D centerline, with path length and endpoint offset controls.
- Added both commands to the Create menu and Create ribbon Profiles group.
- Focused Windows regression passed 11 assertions for invalid-input rejection, valid solid topology, insertion, naming, and undo. Full native CTest passed 4/4. The UI commands are visible in both menu and ribbon; dialogs were not exercised live. See `PROFILE_SWEEP.md`.

# Changelog

## Multi-edge fillet and chamfer - 2026-10-26

- Symmetric selected-edge fillet and chamfer now accept multiple edges on one body, with Ctrl+click selection, a shared parameter dialog, preview, apply, and undo support. Duplicate, empty, stale, or mixed-body selections are rejected.
- Windows Release build passed; focused regression passed 31 assertions and full CTest passed 25/25. Live multi-edge viewport selection is pending. See `MULTI_EDGE_BLENDS.md`.

## Asymmetric edge chamfer - 2026-10-26

- Added a selected-edge chamfer with separate distances on the two adjacent faces, with preview and undoable apply in the Modify menu and ribbon.
- Windows Release build passed; focused regression passed 27 assertions and full CTest passed 25/25. Live UI verified preview, apply, undo, and redo; Properties reported volume changing from 24,000 to 23,960 mm³. See `ASYMMETRIC_CHAMFER.md`.

## Custom-axis circular pattern - 2026-10-26

- Added body copies rotated around a user-defined 3D axis, with a configurable axis origin, direction, instance count, and sweep angle.
- Added the command to the Modify menu and ribbon Pattern group. Windows Release build, 48 focused regression assertions, full CTest 23/23, source audit, and live UI creation plus undo/redo passed. See `CUSTOM_AXIS_CIRCULAR_PATTERN.md`.

## Nonuniform scale - 2026-10-26

- Added per-axis X/Y/Z scale factors about the world origin for a selected body, with identity no-op and one-step undo support.
- Added the operation to the Modify menu and ribbon Transform group.
- Windows Release build passed; focused regression passed 23 assertions and full CTest passed 22/22. Live UI verified the X=2, Y=0.5, Z=3 transform and undo/redo.

## Elliptical loft - 2026-10-26

- Added a two-section solid loft with independently controlled elliptical semi-axes, height, and top-section XY offset.
- Added the operation to the Create menu and Create ribbon Profiles group.
- Windows Release build passed; focused regression passed 19 assertions, and full CTest passed 21/21. Live UI created the default loft with volume 17,886.131 mm³ and verified menu/ribbon access plus undo/redo.

## Linear counterbore and countersink patterns - 2026-10-26

- Added linear arrays of through-all or blind counterbored and countersunk holes, using an editable XY spacing vector and a one-step undoable body operation.
- Added Modify menu and ribbon controls for both patterns.
- Each operation passed a 23-assertion focused regression for parameter validation, through/blind cuts, material removal, undo/redo, and rollback after a later cutter misses the body.
- Full Windows CTest passed 20/20 and the source audit passed 136 checks with 96/96 public operations referenced by regressions.
- Live UI verified both operations on a 40 x 30 x 20 mm box. The three counterbores changed volume to 23,111.418 mm^3; the three countersinks changed volume to 23,315.987 mm^3. Undo/redo passed for both. See their feature notes and `QA_MATRIX.md`.

## Verification and source package refresh - 2026-10-26

- Refreshed the Windows verification snapshot to CTest 20/20 and the source audit to 136 checks, covering all 96 public `SceneDocument` operations with regression references.
- Strengthened the countersink-pattern regression to 28 assertions, including blind-pattern undo/redo and rollback when a later cutter misses the body. Added live UI evidence for a three-hole blind 180° pattern.
- Strengthened the linear keyhole-pattern regression to 22 assertions, including rollback when a later slot misses the body.
- Representative live feature workflows and measured geometry are recorded in `QA_MATRIX.md` and the per-feature notes. The full command-dialog and clean-machine release checklists remain open.

## Linear keyhole-slot pattern - 2026-10-26

- Added a Modify operation for 2–100 repeated keyhole slots with head diameter, throat width/length, depth, center, rotation, and 2D spacing controls.
- Validates every sequential cut and commits the finished pattern as one undoable operation.
- Windows Release build passed; focused regression passed 22 assertions, including rotated cuts, parameter rejection, shape validity, expected volume removal, rollback after a later missed slot, and undo/redo.
- Live UI applied three keyhole slots to a 40 x 30 x 20 mm box. Volume changed from 24,000 to 22,506.725 mm^3 (30 faces, 138 edges); undo and redo passed. See `LINEAR_KEYHOLE_PATTERN.md`.

## Circular counterbore pattern - 2026-10-26

- Added through and blind bolt-circle patterns of stepped counterbore holes, with independent head diameter and depth.
- Windows Release regression passed 21 assertions for validation, full-circle and partial-arc cuts, shape validity, volume bounds, and undo/redo.
- Live UI applied four counterbores to a 40 x 30 x 20 mm box. Volume changed from 24,000 to 22,819.264 mm^3 (18 faces, 72 edges); undo and redo passed. See `CIRCULAR_COUNTERBORE_PATTERN.md`.

## Circular countersink pattern - 2026-10-26

- Added circular through or blind hole arrays with countersunk heads, adjustable drill/head diameters, included angle, bolt-circle position/radius, count, and arc angle.
- Reuses the validated countersink cutter for each position and commits all holes as one undoable document change.
- Windows Release build passed; focused regression passed 28 assertions for input rejection, full-circle through holes, blind partial arcs, valid geometry, volume, undo/redo, and later-cutter rollback.
- Live UI applied four through countersinks to a 40 x 30 x 20 mm box (24,000 to 23,087.983 mm^3; 14 faces, 64 edges) and three blind countersinks over 180° (24,000 to 23,682.423 mm^3; 15 faces, 54 edges); undo and redo passed. See `CIRCULAR_COUNTERSINK_PATTERN.md`.

## Linear rounded-slot pattern - 2026-10-26

- Added a Modify operation for 2–100 equally spaced obround pockets, with 2D spacing, center, rotation, width, overall length, and depth controls.
- Added sequential boolean preview generation, valid-solid and material-removal checks, stale-preview protection, and one-step undoable commit.
- Windows Release build passed; the focused regression passed 29 assertions and the five related slot/groove CTests passed.
- Live UI applied three slots to a 40 x 30 x 20 mm box. Volume changed from 24,000 to 21,655.221 mm^3 (33 faces, 120 edges); undo and redo restored and reapplied the pattern. See `ROUNDED_SLOT_PATTERN.md`.

## Dovetail groove - 2026-10-26

- Added a Modify command for cutting a rotated dovetail groove with a wider undercut, configurable depth, length, and position.
- Added validated cutter preview, stale-preview protection, material-removal checks, and a single-step undoable commit.
- Added the focused `cadcore_dovetail_groove` regression. Its 28 assertions pass on Windows Release; live workflow evidence is recorded in `DOVETAIL_GROOVE.md`.

## T-slot groove - 2026-10-26

- Added a Modify operation for cutting a straight or rotated T-slot with independent neck, undercut, depth, and length controls.
- Added validated preview generation, stale-preview protection, material-removal checks, and a one-step undoable commit.
- Windows Release core and UI builds passed. The focused regression passed 28 assertions, and both groove CTests passed (2/2).
- Live UI created a default T-slot in a 40 x 30 x 20 mm box. Measured volume changed from 24,000 to 23,135.040 mm^3, and the viewport showed the recessed slot. Undo restored the box and redo restored the cut. See `T_SLOT_GROOVE.md`.

## Rounded slot - 2026-10-26

- Added a Modify operation for cutting an obround pocket with semicircular ends and controls for center, entry height, width, overall length, depth, and rotation.
- Added a validated cutter preview, stale-preview protection, positive material-removal check, and one-step undoable commit.
- Windows Release core and UI builds passed. The focused regression passed 31 assertions, including input validation, rotated cuts, the length-equals-width circular case, missed-body rejection, stale-preview protection, and undo/redo. Groove/slot CTests passed 3/3.
- Live UI applied the default rounded slot to a 40 x 30 x 20 mm box. Measured volume changed from 24,000 to 23,218.407 mm^3; the obround pocket appeared in the viewport. UI undo and redo both passed. See `ROUNDED_SLOT.md`.

## Keyhole slot - 2026-10-26

- Added a Modify operation for a circular pocket head connected to a narrower straight throat, with head diameter, throat width and length, depth, position, and rotation controls.
- Added validated cutter construction, preview, stale-preview protection, material-removal checks, and single-step undoable commit.
- Windows Release core and UI builds passed. The focused regression passed 28 assertions, and all four related slot/groove CTests passed.
- Live UI applied the default cut to a 40 x 30 x 20 mm box. Volume changed from 24,000 to 23,357.480 mm^3; the keyhole profile appeared in the viewport. UI undo and redo both passed. See `KEYHOLE_SLOT.md`.

## Circular hole pattern and live UI verification - 2026-10-25

- Added a validated bolt-circle hole pattern with through-all or blind depth, full-circle and partial-arc spacing, and one-step undo.
- The focused regression passed 15 assertions; full CTest passed 10/10 and the source audit passed 136 checks with all 81 public SceneDocument operations referenced.
- Live Windows UI checks created the variable-pitch and tapered springs, elliptical sweep, all five profile/loft tools, a vector pattern, and a four-hole through-all pattern. See `CIRCULAR_HOLE_PATTERN.md` and `QA_MATRIX.md` for observed output.

## Vector linear pattern and API coverage - 2026-10-25

- Added linear patterning along an arbitrary 3D direction, exposed in the Modify menu and Pattern ribbon group.
- Added focused coverage for zero vectors, invalid counts/spacing/source, copy positions, topology, undo, and redo.
- The source audit now verifies that every public `SceneDocument` operation is referenced by the regression suite; all 80 operations are covered in this revision. Full CTest and live UI status are recorded in `VECTOR_PATTERN.md` and `QA_MATRIX.md`.

## Five profile and transition tools - 2026-10-25

- Added elliptical prism and elliptical tube solids, circular loft, rectangular-to-circular loft transition, and a hollow loft reducer.
- Windows Release build passed; focused tests passed 30 assertions across all five tools; full CTest passed 8/8. Live UI click-through remains pending. See `FORM_FEATURES.md`.

## Native STEP import - 2026-10-25

- Added File menu and File ribbon commands to import `.step` and `.stp` geometry into the current scene.
- Imports separate solid bodies, validate topology and positive volume before commit, reject invalid input without mutation, and record the entire import as one undo step.
- Windows x64 Release build passed and CTest passed 3/3. The automated STEP regression checks multi-body append, volume preservation, undo, and malformed-file rollback. Live UI check confirmed the File ribbon entry, STEP/STP file filter, and unchanged scene on cancel. Full import via the picker was not manually exercised. See `STEP_IMPORT.md`.

## 30 Modeling Tools update - 2026-10-25

- Added 30 dimension-driven direct-solid commands under Create > Modeling Tools and a matching Create ribbon group.
- Added default-geometry regression coverage: 210 assertions across the 30 tools, including validation, topology, positive volume, insertion, and undo. Focused Wing rib regression also passes.
- Live Windows app launch passed; all 30 commands were observed in the menu and ribbon. A Rectangular tube was created from the menu and appeared in the viewport and model browser. Individual UI testing of every tool remains unperformed; see `MODELING_TOOLS.md`.

## v1.9.0 - 2026-10-25

- Added 100 dimensioned Production Components spanning aerospace, transit, mining, packaging, fabrication, energy infrastructure, sports equipment, waterworks, printing, and industrial automation.
- Added a Production Components Create menu and ribbon group with component-specific dimensional dialogs.
- Added checked direct-solid generation and undoable insertion with nine geometry families.
- Windows Release build passed; CTest passed 1/1 and the direct core run passed 4,113 assertions, covering all 405 component defaults across v1.3-v1.9. Expanded live Windows checks to cover document actions, exports, view navigation, body properties, mass properties, visibility, duplicate/move/rotate/scale/mirror, linear/circular patterns, Boolean union, split, shell, holes, hole grid, fillet, and chamfer. Expanded regression coverage for selected-edge fillet/chamfer, erase, clear, and new-document reset; audited every public core API operation. Full W01-W24 manual QA and clean-VM deployment remain outstanding; see `manual_qa_results.md`.

## v1.8.0 - 2026-10-25

- Added 70 dimensioned Precision & Utility component commands spanning robotics, laboratory equipment, optics, energy, infrastructure, transport service, and electronics.
- Added a dedicated Create menu and ribbon group with component-specific dialogs.
- Added checked direct-solid generation and undoable insertion using nine reusable geometry families.
- Build and runtime verification remain pending.

## v1.7.0 - 2026-10-25

- Added 70 more component generators for material handling, workshop tooling, HVAC, agriculture, marine systems, electrical installations, and process equipment.
- Added an Extended Components menu and Create-ribbon group with parameter dialogs.
- Added per-component regression for default geometry, invalid dimensions, topology, positive volume, insertion, and undo.
- Windows Release build and CTest passed; direct core suite passed 2,555 assertions, including all 70 new component defaults. The component models are direct-geometry templates with simplified internals; see `FEATURES_V1_7.md`.

## v1.6.0 - 2026-10-25

- Added 70 dimensioned industrial component commands spanning drivetrains, bearings and seals, valves and pipework, thermal/process equipment, and industrial infrastructure.
- Added a dedicated Create > Industrial Components menu and Create-ribbon group.
- Added regression coverage for every default model, zero-dimension rejection, B-rep validity, positive volume, insertion and undo.
- Geometry uses nine reusable direct-model families; specialized internal and manufacturing details are approximations.

## v1.5.0 - 2026-10-25

- Added 35 specialized component generators for gears and planetary drives, bearings, actuators, engine parts, chain and shaft couplings, pipework and cooling.
- Added a dedicated Create → Specialized Components menu and Create-ribbon group with dimension dialogs.
- Added regression coverage for every new default shape, invalid zero dimension, B-rep validity, positive volume and undo.
- Windows Release build passed; direct core suite passed 1,295 assertions.

## v1.4.0 - 2026-10-25

- Added 30 advanced component commands for fasteners, couplings, supports, linear motion parts, springs, brakes, fittings, heat sinks and rotating assemblies.
- Added a separate Create → Advanced Components menu and Create-ribbon group with component-specific dimensional dialogs.
- Added geometry, parameter validation, atomic document insertion and undo regression coverage for all 30 new default models.
- Windows Release build and CTest passed; direct core suite passed 980 assertions.

## v1.3.0 - 2026-10-25

- Added 30 mechanical component generators for shafts, pulleys, bearing assemblies, pipe fittings, flanges, structural sections, gears, and an impeller.
- Added Create-menu and Create-ribbon commands with component-specific dimensional dialogs.
- Added per-component range validation and atomic undoable document insertion.
- Added regression coverage for all 30 default builds, topology, positive volume, zero-dimension rejection and undo.
- Windows Release build passed; CTest 1/1 passed; direct core suite passed 710 assertions.

## v1.2.0 - 2026-10-25

- Added 30 direct CAD modeling and inspection capabilities. See `FEATURES_V1_2.md`.
- Added new solids and sketch profiles, extrusion controls, arbitrary-axis transforms/revolve, lofting, hole grids, patterns, edge finishing and face draft.

## v1.1.0 - 2026-10-24

- Added profile extrusion New Body, Join and Cut with asynchronous previews and atomic undo.
- Fixed B-rep preview aliasing that could corrupt undo/redo snapshots.


- Added read-only geometry health and topology inspection to the Inspect menu and ribbon. Windows Release build passed, focused regression passed 20 assertions, full CTest passed 30/30, and live UI verified a 40 × 30 × 20 mm box (1 solid, 1 shell, 6 faces, 6 wires, 12 unique edges, 8 unique vertices; 24,000 mm³ volume; 5,200 mm² area; center (20, 15, 10) mm). See `GEOMETRY_DIAGNOSTICS.md`.
- Live UI verification passed for minimum distance (5.0000 mm clearance between separated boxes, with closest-point coordinates displayed) and interference volume (840.0000 mm³ overlap with centroid (7.5, 6, 7) mm; face contact returned zero). Full CTest passed 30/30. See `MINIMUM_DISTANCE.md` and `INTERFERENCE_VOLUME.md`.
- Added density-based mass calculation to the Inspect Mass Properties workflow. Users can enter material density in kg/m³; the read-only report returns estimated mass in kg with volume and center of mass. Focused regression passed 14 assertions, source audit covers all 106 public scene operations, Windows Release build passed, full CTest passed 31/31, and live UI reported 0.188400 kg for a 40 × 30 × 20 mm box at 7,850 kg/m³. See `MASS_FROM_DENSITY.md`.
# Planar section contour measurement - 2026-10-26

- Added a read-only Inspect command that measures unique section edges and total contour length for an axis-aligned plane through one selected body.
- Focused regression passed 15 assertions; Windows x64 Release build passed; full CTest passed 34/34; source audit passed 136 checks with all 109 public scene operations covered.
- Live UI verified a Z-normal section through a 40 × 30 × 20 mm box at 10 mm: 4 edges and 140.000 mm contour length. See `PLANAR_SECTION.md`.

# Surface type summary - 2026-10-26

- Added a read-only Inspect report that classifies unique B-rep faces as planar, cylindrical, conical, spherical, toroidal, or other and totals area by type.
- Focused regression passed 22 assertions across box, cylinder, sphere, cone, and torus surfaces. Windows x64 Release build passed; full CTest passed 34/34; source audit passed 136 checks with all 109 public scene operations covered.
- Live UI verified a box's 5,200 mm² planar area and a cylinder's cap and curved areas (628.319 and 2,199.115 mm²). See `SURFACE_TYPE_SUMMARY.md`.

## Body schedule CSV export - 2026-10-26

- Added **File → Export body schedule CSV...** with body ID/name, visibility, volume, surface area, X/Y/Z extents, and center of mass.
- Windows Release application and test targets rebuilt; full CTest passed 38/38, including 14 focused CSV assertions; source audit passed 141 checks with all 113 public core operations referenced by regression.
- Live UI confirmed the File menu command is visible; save-dialog workflow remains pending. This is a body schedule, not a quantity-aggregated BOM.

## Per-body materials - 2026-10-26

- Added saved material name and density fields, assignment/clear with undo-redo, a preset selector, body-property display, and assigned-density defaults for mass inspection.
- Duplicated, mirrored, and patterned bodies inherit the source material. Boolean unions retain a material only when both inputs have matching assignments.
- Native v2 scenes without these optional fields continue to load as unassigned.
- Added custom material entry for name and density; custom assignments on any body can be reused elsewhere in that document.
- Multiple selected bodies can receive one material in a single atomic, undoable edit.
- Windows x64 Release application and test targets rebuilt; focused material regression passed 50 assertions, full CTest passed 39/39, and source audit passed 145 checks with all 114 public scene operations referenced. Live UI verification remains pending. See `MATERIAL_ASSIGNMENTS.md`.


- Live Windows UI verification passed: preset and custom material names/densities appeared in body properties, and mass properties used the assigned densities (0.064800 kg at 2,700 kg/m³; 0.188400 kg at 7,850 kg/m³ for a 24,000 mm³ box).

## v1.11.0 BOM, parts lists, and data readiness - 2026-10-27

- Added persistent part numbers, descriptions, and revisions with native-scene schema v3 and backwards-compatible v1/v2 loading.
- Added grouped BOM and per-body parts-list reports, material-aware unit/total mass and volume, escaped CSV exports, and part metadata inheritance for duplicate/pattern/mirror operations.
- Added BOM quality checks and selectable ISO/ASME drawing-readiness profiles. These profiles do not claim to validate drawing rules; ISO tolerances and ASME title-block/dimension/GD&T checks remain pending 2D drawings.
- Windows Release build passed, focused regression passed 42 assertions, full CTest passed 42/42, and live UI confirmed metadata, material-aware mass, duplicate aggregation, per-body rows, both CSV exports, and all three checker profiles.
- See `BOM_PARTS_LIST.md`, `STANDARDS_CHECKS.md`, and `QA_MATRIX.md` for behavior, coverage, and remaining work.

## v1.12.0 orthographic SVG drawings - 2026-10-27

- Added A4 landscape SVG export for one selected body, with front, top, and right hidden-line views and a basic part metadata block.
- Windows Release build passed; drawing export regression passed 15 assertions; full CTest passed 43/43.
- Live UI exported and parsed a test drawing with all three views, part number, revision, and 24 vector polylines.
- This is an unannotated drawing export. Dimensions, tolerance annotations, datums, GD&T, and standards validation remain unimplemented.

## v1.13.0 first-angle and third-angle drawing layouts - 2026-10-27

- Added a projection picker and first-angle/third-angle orthographic layouts, with matching view arrangement and title-block placement.
- Windows x64 Release build passed; focused drawing regression passed 19 assertions; full CTest passed 43/43.
- Live UI exported both conventions for a 40 × 30 × 20 mm test solid. Both SVG artifacts parsed, and rendered previews confirmed the layouts.
- The exports include overall-envelope dimensions only. Editable dimensions, feature dimensions, projection symbols, tolerance annotation, and ISO/ASME validation remain unimplemented.

## v1.14.0 drawing sheet sizes - 2026-10-27

- Added A3 landscape SVG export alongside A4, with a selectable paper size in the drawing export flow and proportional layout scaling.
- Focused orthographic drawing regression passes 22 assertions. Live Windows UI exported an A3 first-angle test drawing; its SVG parsed with a 420 × 297 mm canvas, metadata, three orthographic views, envelope dimensions, and 24 model-edge paths. The rendered sheet was visually reviewed.
- Windows Release build and full CTest/source-audit results are recorded in `docs/QA_MATRIX.md` and `docs/local_verification.json`.

## v1.15.0 isometric drawing view - 2026-10-27

- Added a hidden-line isometric view to the A4/A3 first-angle and third-angle SVG sheets; the drawing scale now fits all four views.
- Core drawing regression passes 24 assertions. The live Windows app exported an A3 third-angle box drawing; SVG parsing confirmed a 420 × 297 mm sheet, all four view labels and 36 edge paths. Rendered layout review passed.
- Windows x64 Release build and full CTest passed 43/43. See `docs/ORTHOGRAPHIC_DRAWINGS.md` and `docs/QA_MATRIX.md`.

## v1.16.0 section views and combined readiness checks - 2026-10-27

- Added selectable Section A-A at the body's Y mid-depth to first-/third-angle A4/A3 SVG sheets. The generated section has a cut-plane callout, same-direction arrows, and clipped 45-degree hatch lines. It remains a simple fixed-plane section view.
- Added “All available checks” to run the BOM data, ISO drawing-readiness, and ASME drawing-readiness reports together with separate findings and combined totals.
- Windows x64 Release build passed. Focused orthographic drawing regression passed 26 assertions; full CTest passed 43/43; source audit passed 180 checks with 118/118 public core operations referenced. Live UI verified the combined checks and A4 section export; rendered review of the section sheet remains pending.
- The standards profiles remain readiness/data checks and do not claim ISO or ASME drawing compliance. See `ORTHOGRAPHIC_DRAWINGS.md`, `STANDARDS_CHECKS.md`, and `QA_MATRIX.md`.

## v1.17.0 selectable section depth - 2026-10-27

- Added a user-selected section location from 1% to 99% through the body's Y extent. The cut-plane callout reports the percentage and model-space Y coordinate; invalid boundary positions are rejected.
- Windows x64 Release build passed; focused drawing regression passed 29 assertions; full CTest passed 43/43; source audit passed 184 checks with 118/118 public core operations referenced. Live UI exported a 35% section and confirmed its Y coordinate. Fresh raster sheet review remains pending.

## v1.18.0 cylindrical centerlines and center marks - 2026-10-27

- Added centerline strokes from cylindrical faces in projected orthographic views and center marks where the cylinder axis points toward the view.
- Windows x64 Release build passed; focused drawing regression passed 32 assertions; full CTest passed 43/43; source audit passed 186 checks with all 118 public core operations referenced. Live UI exported a third-angle A4 drawing from a 10 mm radius × 35 mm cylinder. SVG validation found 31 polylines, including five centerline/center-mark strokes. Rendered review confirmed the annotations and title block fit; the PNG and source SVG are in `docs/evidence/`.
- Updated ISO/ASME readiness messages to accurately state that exported drawing sheets are not inspected by those checks; the live combined run still reported 0 errors and 12 expected metadata warnings.

## v1.19.0 exported drawing readiness screen - 2026-10-27

- Added an Inspect command that screens SVG drawings for valid XML, A4/A3 metric dimensions with a matching viewBox, required orthographic and auxiliary-view labels, projection convention, X/Y/Z envelope dimensions, and populated title-block fields. ISO and ASME checks are reported separately and explicitly disclaim clause-level compliance.
- Added regressions for complete sheets, malformed SVG, missing views, incomplete title blocks, mismatched page/viewBox sizes, and unsupported profile selection.
- Windows x64 Release build passed; focused drawing regression passed 39 assertions; full CTest passed 43/43 with `-C Release`; source audit passed 190 checks and 118/118 public core regression references. The final v1.19 UI relaunch remains unverified because Computer Use was stopped by physical Escape; a prior UI run exercised both profile screens on the drawing.

## v1.23.0 procurement-aware BOM - 2026-10-27

- Added per-body Manufactured/Purchased classification, supplier, and supplier part number fields to the editor, grouped BOM, per-body parts list, and both CSV exports. Purchasing data participates in BOM rollup identity and duplicate inheritance.
- BOM data-quality checks report missing supplier and catalogue number for purchased items. Native schema v5 persists the values; schema v1-v4 scenes continue to load with legacy items defaulting to Manufactured.
- Live Windows UI opened a schema-v4 scene, edited it to a purchased component, displayed the new fields in both report tabs, ran the BOM data-quality profile (0 errors; 2 expected revision/material warnings), and saved/inspected a schema-v5 file.
- Windows Release build passed; BOM/parts/standards regression passed 69 assertions; drawing regression passed 54 assertions; full CTest passed 43/43; source audit passed 209 checks with 118/118 public core operations referenced.
- The update does not add pricing, stock management, purchase orders, supplier catalogs, or ERP integration.

## v1.29.0 persistent component occurrences and drawing source references - 2026-10-27

- Added persistent component-definition identity, occurrence identity, fixed XYZ placement, scene schema v6 storage, Assembly menu/ribbon commands, and component-linked BOM grouping/columns.
- Added source body/geometry revision metadata to SVG exports and grouped each exported view for easier selection in SVG editors. This is snapshot provenance, not a live model link.
- Added focused regressions: 42 assembly assertions cover component insertion, placement, BOM/parts-list CSV links, undo/redo, v5 migration and v6 persistence, and malformed IDs; 66 drawing assertions cover source metadata and selectable view/annotation groups. Full CTest and source audit results are in `QA_MATRIX.md`.
- Rigid/revolute joints and a persistent editable drawing-sheet model remain unimplemented.

## v1.30.0 persistent drawing setup and SVG annotation groups - 2026-10-27

- Added persisted drawing setup per body: projection, A4/A3 sheet, isometric/section view and depth, notes, and title-block values. Reopening the export dialog uses the saved settings.
- Added strict settings validation and schema-v7 storage while retaining migration support for earlier scene versions.
- SVGs record source body and geometry token, with semantic groups for projected views, overall dimensions, and general notes. The artifact remains a static snapshot.
- Windows Release build passed; 57 assembly/drawing-settings assertions and 66 drawing assertions passed; full CTest passed 46/46; source audit passed 250 checks across 123 public operations. Live UI verification remains pending.




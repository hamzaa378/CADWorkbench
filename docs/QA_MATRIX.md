## CADWorkbench v1.48 parametric Y-direction patterns (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built with Visual Studio 18, Qt 6.11.2, and OCCT 8.0.1. |
| Parametric foundation | PASS (113 assertions) | X/Y hole patterns, axis persistence, schema-v18 migration, boundaries, volume, atomic rebuild, invalid-file rejection, undo/redo. |
| Full Windows CTest | PASS (53/53) | Latest Release suite completed in 34.99 seconds. |
| Source audit | PASS (332 checks) | All 145/145 public core operations are referenced by regression coverage. |
| Live UI tests | NOT RUN | User requested coding, builds, and regression tests only. |
| Scope | PARTIAL | Parametric history remains a focused rectangle/extrusion/hole/pattern chain, without a general constraint solver or arbitrary editable timeline. |

## CADWorkbench v1.47 parametric patterns, sheet-metal relief, and IGES exchange (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built with Visual Studio 18, Qt 6.11.2, and OCCT 8.0.1; IGES runtime DLL copied by the build. |
| Parametric foundation | PASS (96 assertions) | X-direction hole-pattern count/spacing, limits, containment, volume, atomic rebuild, schema-v18 persistence, invalid-file rejection, undo/redo. |
| Multi-bend sheet metal | PASS (41 assertions) | One-bend SVG, multi-bend rule calculation, DXF geometry, optional rectangular end reliefs, export, and invalid dimensions. |
| IGES exchange | PASS (15 assertions) | Multi-solid export/import round-trip, solid validity and volume, atomic undo/redo, missing input and empty-scene handling. |
| OBJ mesh import | PASS (17 assertions) | Polygon triangulation, negative indices, closed-solid volume, undo/redo, and open/malformed/missing-file rejection. |
| Full Windows CTest | PASS (53/53) | Release suite completed in 45.64 seconds. |
| Source audit | PASS (332 checks) | All 145/145 public core operations are referenced by regression coverage. |
| Live UI tests | NOT RUN | User requested coding, builds, and regression tests only. |
| Scope | PARTIAL | Parametric history remains a focused rectangle/extrusion/hole/pattern chain; sheet-metal solids and springback are absent; IGES handles solids only and flattens hierarchy. |

## CADWorkbench v1.46 assembly, drawing-source, and project-readiness update (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Built `CADWorkbench.exe` and `cadcore_tests.exe` with Visual Studio 18, Qt 6.11.2, and OCCT 8.0.1. Existing OCCT/Qt deprecation warnings remain. |
| Full Windows CTest | PASS (51/51) | Release suite completed in 45.19 seconds. |
| Assembly solver and motion tracks | PASS (133 focused assertions) | Rigid/revolute/prismatic tree joints, limits, nested descendant motion, parent-first mixed pose solve, linear keyframe interpolation, scene-v17 save/reopen, malformed reference rejection, atomic undo/redo, and reusable geometry synchronization. |
| Drawing source refresh | PASS (104 persistent-assembly assertions) | Current/stale/missing source status, explicit revision refresh, undo/redo of refresh and geometry edits, and existing saved-sheet migration/persistence checks. |
| Configurable model drawing readiness | PASS (93 BOM/standards assertions) | Required saved-sheet fields/conventions, missing-sheet and stale-source warnings, source refresh, saved project policy persistence, and explicit ISO/ASME scope boundaries. |
| Orthographic drawings | PASS (120 focused assertions) | Existing placed views, details/sections, selectable geometry-derived diameter references, linked BOM/balloons, and PDF/SVG output. |
| Closed STL import | PASS (17 focused assertions) | Faceted solid validity, volume, triangle count, atomic undo/redo, missing input, invalid tolerance, and open-mesh rejection. |
| Other focused priority regressions | PASS | Parametric foundation 77, multi-bend sheet metal 34, STEP assembly import 28. |
| Source audit | PASS (332 checks) | All 141/141 public core operations are referenced by regression coverage. Textual audit complements compiled runtime tests. |
| Live UI | NOT RUN | Explicitly excluded by the user for this work session. |
| Scope | PARTIAL | Assembly solving supports acyclic rigid/revolute/prismatic trees, not closed-loop or over-constrained mechanisms; motion is evaluated at requested keyframe times, not played in real time. Occurrence orientation is encoded in transformed B-rep geometry. Drawings still require re-export after model edits, and readiness checks are not formal ISO/ASME certification. |

See [`PERSISTENT_ASSEMBLIES.md`](PERSISTENT_ASSEMBLIES.md), [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md), [`STANDARDS_CHECKS.md`](STANDARDS_CHECKS.md), and [`local_verification.json`](local_verification.json).

## CADWorkbench v1.44 atomic parametric edits and saved drawing source status (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Built `CADWorkbench.exe` and `cadcore_tests.exe` with Visual Studio 18 2026, QtSvg/PrintSupport, and OCCT. Existing OCCT/Qt deprecation warnings remain. |
| Full Windows CTest | PASS (50/50) | Suite completed in 48.58 seconds after single-body saved-source status coverage was added. |
| Parametric foundation | PASS (59 focused assertions) | Batch update of block width and hole center succeeds together; invalid coupled dimensions preserve both parameters and solid; one-step undo/redo and unchanged-value no-op are covered. |
| Assembly joints | PASS (71 focused assertions) | Parent translation/rotation carries rigid and revolute children through a nested chain, updates revolute joint frames, and supports atomic undo/redo; existing limit, reference, persistence, and cycle checks pass. |
| Persistent assembly and drawing settings | PASS (101 focused assertions) | Single-body and multi-body sheets report no-sheet/current/stale/missing states; undo, deletion, save/reopen, schema migrations, and persisted drawing annotations are covered. |
| Orthographic drawing | PASS (120 focused assertions) | Detail/section rendering and placement, geometry-derived cylindrical dimension IDs, PDF output, source metadata and other existing drawing checks. |
| Existing priority regressions | PASS | Assembly joints 32, STEP XDE assembly import 28, one-bend sheet metal 14. |
| Source audit | PASS (309 checks) | 132/132 public core operations are referenced by regression coverage. Textual audit only. |
| Live UI | NOT RUN | Explicitly excluded by the user for this work session. |
| Scope | PARTIAL | Parametric editing still supports a fixed rectangle/extrusion/one-hole chain, not a general sketch solver or arbitrary feature graph. Drawing source check reports revision status; saved SVG/PDF files still need re-export after model changes. |

See [`PARAMETRIC_FOUNDATION.md`](PARAMETRIC_FOUNDATION.md), [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md), and [`local_verification.json`](local_verification.json).

## CADWorkbench v1.43 projected drawing section and detail views (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Built `CADWorkbench.exe` and `cadcore_tests.exe`; QtSvg/PrintSupport deployment remains available for SVG/PDF output. |
| Full Windows CTest | PASS (50/50) | Final Windows x64 Release suite completed in 45.94 seconds. |
| Orthographic drawings, detail views, sections, feature dimensions, and PDF | PASS (120 focused assertions) | X/Y/Z axis cut lines, correct TOP/FRONT source views, section labels and hatching, detail rendering/clipping, geometry-derived cylindrical feature IDs across re-export and geometry change, invalid-depth and outside-source rejection, existing annotations and A4/A3 PDF output. |
| Persistent assembly/drawing settings | PASS (92 focused assertions) | Detail and section IDs/values survive schema-v15 save/reopen; invalid axes and out-of-sheet placements are rejected; v13/v14 migrations preserve earlier drawing records. |
| Existing priority regressions | PASS | BOM/standards 83, STEP XDE 28, sheet-metal flat pattern 14, parametric foundation 51, assembly joints 32. |
| Source audit | PASS (292 checks) | 131/131 public core operations are referenced by regression coverage. Textual audit only. |
| Live UI | NOT RUN | Explicitly excluded by the user for this work session. |
| Scope | PARTIAL | Detail and X/Y/Z axis-aligned section records are editable and feed exported SVG/PDF snapshots; cylindrical diameter callouts have geometry-derived feature IDs. Exported files do not refresh after a model change; arbitrary section paths and user-attached edge/face dimensions remain unimplemented. |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) and [`local_verification.json`](local_verification.json).

## CADWorkbench v1.42 orthographic drawing PDF export (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Built `CADWorkbench.exe` and `cadcore_tests.exe`; QtSvg and Qt PrintSupport are included and deployed. |
| Full Windows CTest | PASS (50/50) | Windows x64 Release suite completed in 46.44 seconds. |
| Orthographic drawings and PDF | PASS (109 assertions) | Existing source/annotation regressions plus single-page A4/A3 PDF output, PDF header/trailer, malformed SVG rejection, and existing-file preservation. |
| Project checklist policy | PASS | Schema-v13 save/load, v12 default migration, validation, and undo/redo remain covered by the BOM/standards regression. |
| Source audit | PASS (272 checks) | 131/131 public core operations are referenced by regression coverage. Textual audit only. |
| Live UI | NOT RUN | Explicitly excluded by the user for this work session. |
| Scope | PARTIAL | PDF is a fixed one-page rendering of the SVG sheet. It does not create editable PDF annotations or update after model changes. |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) and [`local_verification.json`](local_verification.json).

## CADWorkbench v1.41 project drawing checklist defaults (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Built `CADWorkbench.exe` and `cadcore_tests.exe` with Visual Studio 18 2026, Qt 6.11.2, OCCT 8.0.1, and vcpkg. |
| Full Windows CTest | PASS (50/50) | Windows x64 Release suite completed in 43.65 seconds. |
| Project drawing policy | PASS (14 new assertions) | Accepts known checklist keys, rejects duplicates/unknown keys, marks project dirty, supports undo/redo, persists through schema-v13 save/reopen, migrates schema v12 to empty defaults, and rejects malformed policy loads transactionally. |
| BOM / standards checks | PASS (83 assertions) | Existing BOM and ISO/ASME SVG structural-readiness tests plus persistent project checklist regression. |
| Existing priority regressions | PASS | STEP XDE 28, sheet-metal flat pattern 14, parametric foundation 51, assembly joints 32, persistent assembly/drawing settings 81, orthographic drawings 98 assertions. |
| Source audit | PASS (268 checks) | 131/131 public core operations are referenced by regression coverage. Textual audit only. |
| Live UI | NOT RUN | Explicitly excluded by the user for this work session. |
| Scope | PARTIAL | Checklist defaults persist, but the checks remain structural/content screens and do not certify ISO/ASME clauses, tolerances, datums, or GD&T. |

See [`STANDARDS_CHECKS.md`](STANDARDS_CHECKS.md) and [`local_verification.json`](local_verification.json).

## CADWorkbench v1.40 persistent drawing BOM balloons (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Built `CADWorkbench.exe` and `cadcore_tests.exe` with Visual Studio 18 2026, Qt 6.11.2, OCCT 8.0.1, and vcpkg. |
| Full Windows CTest | PASS (50/50) | Windows x64 Release suite completed in 43.22 seconds. |
| STEP XDE assembly import | PASS (28 assertions) | Names, nested assembly paths, repeated component definitions, distinct occurrence IDs, placements, solid volume, atomic undo, and malformed input. |
| One-bend sheet-metal flat pattern | PASS (14 assertions) | Bend allowance/deduction, outline and bend-line metadata, bounds, input rejection, and SVG file output. |
| BOM and standards/readiness checks | PASS (69 assertions) | BOM quality rules, configurable checklist profiles, exported-SVG structural screening, and non-certification scope reporting. |
| Persistent assembly/drawing settings | PASS (81 assertions) | Source set persistence, BOM quantity linking, balloon IDs/source body IDs/positions, schema-v11 migration, and invalid source/placement rejection. |
| Orthographic drawings | PASS (98 assertions) | Source compound and aggregate revision, source-scoped BOM rows, balloon item number and body identity in SVG, source-set mismatch/staleness, and existing projections/annotations. |
| Parametric foundation / joints | PASS (51 / 32 assertions) | Rebuilt and reran the focused parameter/history and rigid/revolute joint suites. |
| Source audit | PASS (263 checks) | 130/130 public core operations are referenced by regression coverage. Textual audit only. |
| Live UI | NOT RUN | Explicitly excluded by the user for this work session. |
| Scope | PARTIAL | Balloons are attached to source-body BOM identities and exported as SVG snapshots; sheets do not update automatically, and there are no feature-attached dimensions, independent detail views, PDF export, or formal standards certification. |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md), [`PERSISTENT_ASSEMBLIES.md`](PERSISTENT_ASSEMBLIES.md), and [`local_verification.json`](local_verification.json).

## CADWorkbench v1.39 parametric hole, assembly joints, and multi-body drawing sources (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Built `CADWorkbench.exe` and `cadcore_tests.exe` with Visual Studio 18 2026, Qt 6.11.2, OCCT 8.0.1, and vcpkg. Build emitted existing OCCT/Qt deprecation warnings and a Qt translations notice; no build errors. |
| Full Windows CTest | PASS (50/50) | Windows x64 Release suite completed in 44.07 seconds after source-scoped drawing BOM coverage was added. |
| Parametric foundation | PASS (51 assertions) | Rectangle/extrusion plus circular sketch and through-hole; named diameter/center parameters, rebuild, persistence, containment validation, stale-history handling, undo/redo. |
| Persistent assemblies | PASS (75 assertions) | Existing occurrence/BOM workflows plus scene joint persistence, malformed relationship handling, and grouping selected repeated occurrences in a source-scoped BOM. |
| Rigid/revolute joints | PASS (32 assertions) | Relationship validation, cycle/self/duplicate rejection, axis/limit handling, revolute drive and undo/redo. |
| Orthographic drawing | PASS (97 assertions) | Composite two-body source shape, volume/bounds, aggregate geometry token, source-ID export metadata, current/mismatched/stale multi-body comparisons, and source-scoped BOM validation. |
| Source audit | PASS (257 checks) | 130/130 public core operations are referenced by regression coverage. Audit is textual and complements, but does not replace, the build and runtime suite. |
| Live UI | NOT RUN | Explicitly excluded by the user for this work session. |
| Scope | PARTIAL | Parametrics are a focused four-step template; assembly joints are a bounded relationship/drive implementation, not a general solver; multi-body drawing exports do not auto-update, though their selected source set and aggregate revision can be checked together. |

See [`PARAMETRIC_FOUNDATION.md`](PARAMETRIC_FOUNDATION.md), [`PERSISTENT_ASSEMBLIES.md`](PERSISTENT_ASSEMBLIES.md), and [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md).

## CADWorkbench v1.36 linked BOM unit mass in drawings (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built with Visual Studio 18 after the v1.36 mass-column change. |
| Orthographic drawing regression | PASS | Checks the `MASS (kg)` header, `data-bom-unit-mass-kg`, and rendered nonzero fixture value `0.024`. |
| Full CTest | PASS (49/49) | Windows x64 Release suite; 43.74 seconds. |
| Source audit | PASS (250 checks) | All 126 public core operations are referenced by regression coverage. |
| Live UI export | PASS | v1.36 opened the saved two-occurrence scene and exported `Testing/v1_36_linked_parts_mass.svg`; the linked table shows `MASS (kg)` and unit mass `0` because no material density is assigned. |
| Scope | PARTIAL | Mass is taken from the existing BOM mass-property calculation; no density is inferred. Drawings remain one selected body per sheet and SVG exports are snapshots. |

## CADWorkbench v1.35 BOM-linked drawing parts list (2026-10-28)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built with Visual Studio 18. |
| Full CTest | PASS (49/49) | Windows x64 Release suite; all geometry, persistence, BOM, STEP, parametric, sheet-metal, and drawing targets passed. |
| Source audit | PASS (250 checks) | All 126 public core operations are referenced by regression coverage. |
| Live UI setup | PASS | Created a 40 × 30 × 20 mm box, selected it, enabled the linked parts list, saved the default 12 × 12 mm placement; editor confirmation and selection requirement observed. |
| Live UI export | PASS | Exported `Testing/v1_35_linked_parts_list.svg`, then created a reusable component and second occurrence 50 mm away. `Testing/v1_35_bom_grouped_occurrences.svg` is well-formed SVG and shows one grouped BOM row, quantity 2, and the shared component-definition ID. |
| Scene save/reopen | PASS | Saved and reopened `Testing/v1_35_linked_bom_ui.cadscene`; scene schema v11 retained two fixed occurrences sharing one definition, enabled table state, stable table ID, and 12 × 12 mm placement. UI editor reopened with the checkbox checked. |
| Schema migration | PASS | Schema-v10 regression preserves saved drawing dimension records and adds a hidden default linked-parts-list record. |
| Scope | PARTIAL | SVG is an export snapshot, one selected body per sheet, up to 18 BOM rows; no drawing balloons or live refresh of existing exports. |

## CADWorkbench v1.34 persistent overall drawing dimensions (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully with Visual Studio 18. |
| Persistent drawing setup | PASS (68 assertions) | Stable X/Y/Z dimension IDs and offsets, save/reopen, schema-v10 writing, schema compatibility, undo/redo, and invalid offset rejection. |
| Orthographic drawing | PASS (78 assertions) | SVG includes the saved dimension ID and offset alongside stable sheet/view/note metadata; XML and existing drawing checks pass. |
| Full CTest | PASS (49/49) | Windows x64 Release tests, 42.24 seconds. |
| Source audit | PASS (250 checks) | All 126 public core operations are referenced by regression coverage. Textual audit; not a substitute for compilation/runtime tests. |
| Live UI | NOT RUN | The dimensions menu/ribbon editor and generated SVG need a desktop click-through. |
| Scope | PARTIAL | Persistent dimensions represent overall bounding-envelope extents. They are not attached to selected edges/features; no feature dimensioning, tolerances, datums/GD&T, multi-part sheets, balloons, linked BOM, detail views, PDF, or live SVG refresh. |

## CADWorkbench v1.32 persistent placed-view drawing sheets (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Source implementation | ADDED | Scene schema v9 stores a stable drawing-sheet ID, linked source geometry token, four stable projected-view IDs, and editable paper-space view centers. |
| Layout UI | ADDED | **File → Arrange saved drawing views...** edits top/front/right/auxiliary view centers and explicitly refreshes a stale source link on save. |
| SVG output | ADDED | Export uses saved placements and includes the sheet ID and stable per-view IDs. SVG remains a snapshot; model edits do not rewrite existing files. |
| Windows Release build | PASS | v1.32 app and tests built successfully. |
| Automated tests | PASS (49/49) | Full Windows Release suite passed at v1.32 and passed again after the v1.34 schema/dimension changes. |
| Source audit | NOT RUN | The v1.32 implementation preceded the final source-audit run. |
| Live UI | NOT RUN | Layout editor and exported placement IDs need a future desktop verification pass. |
| Scope | PARTIAL | Per-body, single-part sheets with shared auto-fit scale. Feature-linked dimensions, linked BOM tables, balloons, detail views, PDF, and live-refresh of existing SVG files remain future work. |

## CADWorkbench v1.31 parametric/manufacturing/drawing checks (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` rebuilt successfully with Visual Studio 18; app version v1.31.0 |
| STEP XDE assembly round-trip | PASS (28 assertions) | Source component names, nested hierarchy, repeated definition identity, distinct occurrences, placement, solid volume, and atomic undo |
| Parametric foundation | PASS (33 assertions) | Named dimensions, constrained XY rectangle, editable extrusion, undo/redo, schema-v8 geometry identity, stale-history safety, and corrupt mismatch rejection |
| One-bend sheet-metal flat pattern | PASS (14 assertions) | Bend allowance/deduction, developed length, SVG metadata, input bounds, and file output |
| Drawing checklist/source comparison | PASS (72 assertions) | Configurable required fields/conventions; malformed SVG, current/stale/wrong-body/missing source identity |
| Persistent assemblies and BOM/standards | PASS (59 / 69 assertions) | Fixed occurrence persistence and BOM links; metadata, grouping, and bounded ISO/ASME content checks |
| Full Windows CTest | PASS (49/49) | `cadcore_geometry_and_persistence` and all focused regressions passed in Release; 41.94 seconds |
| Source audit | PASS (250 checks) | All 126 public core operations referenced by regression |
| Live UI | NOT RUN | This update was verified with Windows Release build, automated regressions, and source audit only. Earlier v1.30 UI results remain historical and do not verify the new v1.31 UI flows. |
| Scope | PARTIAL | One rectangle/extrusion parametric chain; one-bend calculator/SVG (not folded solid/DXF); STEP preservation is limited to structures represented in STEP; drawing requirements apply per review and source comparison identifies snapshots, not live-updating sheets. Formal ISO/ASME conformity and tolerance tables are not claimed. |

See [`PARAMETRIC_FOUNDATION.md`](PARAMETRIC_FOUNDATION.md), [`SHEET_METAL.md`](SHEET_METAL.md), [`PERSISTENT_ASSEMBLIES.md`](PERSISTENT_ASSEMBLIES.md), [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md), and [`STANDARDS_CHECKS.md`](STANDARDS_CHECKS.md).

## CADWorkbench v1.30 persistent drawing setup and SVG source annotations (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built with schema-v7 settings; app version v1.30.0 |
| Assembly/drawing setup regression | PASS (59 assertions) | Occurrence identity/translation, BOM/parts list, v5/v6 migration, v7 persistence, drawing setup undo/redo and validation, malformed-load rejection and reopen |
| SVG drawing regression | PASS (66 assertions) | Source body/revision metadata; selectable view, dimension, and note groups |
| Full CTest | PASS (46/46) | Windows x64 Release suite |
| Source audit | PASS (250 checks) | All 123 public core operations referenced by regression |
| Live UI | PASS (focused workflow) | v1.30.0: converted a 40 × 30 × 20 mm box to a component and inserted a fixed occurrence at X=50 mm; Properties showed X bounds 50–90 mm; BOM showed Qty 2 / 48,000 mm³ total; parts list showed both occurrences. Exported A4 SVG with TOP/FRONT/RIGHT/SECTION A–A groups, source body/geometry IDs, and selectable dimensions/notes. Reopened export setup restored section, note, and title-block values. CAD scene save/reopen was not exercised in the UI. |
| Scope | PARTIAL | Settings persist but no scene-based placed views, live model update, balloon/parts-list drawing entities, rigid/revolute joints, or PDF export |

## CADWorkbench v1.29 persistent component occurrences and SVG source identity (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built in `build_materials_round/Release`; app version v1.29.0 |
| Assembly focused regression | PASS (42 assertions) | Component identity, fixed XYZ placement, BOM/parts-list linkage, CSV, undo/redo, geometry-divergence grouping, schema-v6 save/reopen, v5 migration, and duplicate-ID rejection |
| Drawing focused regression | PASS (66 assertions) | Existing projection/dimension checks plus source body/token metadata, selectable view and annotation groups |
| Full CTest | PASS (46/46) | Windows x64 Release suite including `cadcore_persistent_assemblies` |
| Source audit | PASS (245 checks) | All 122 public core operations referenced by regression |
| Live UI | PENDING | Assembly command workflows and generated SVG metadata reserved for final UI window |
| Scope | PARTIAL | Translation-only placements; no rigid/revolute joints or persistent drawing sheet / live model update |

See [`PERSISTENT_ASSEMBLIES.md`](PERSISTENT_ASSEMBLIES.md) and [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md).

## CADWorkbench v1.28 planar face relationship measurement (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Application and regression target rebuilt; version v1.28.0 |
| Focused regression | PASS (50 assertions) | Face-normal angle, parallel-plane gap, invalid/stale references, curved-face rejection, and read-only behavior |
| Full CTest | PENDING | Run after cylindrical-face measurement was added |
| Source audit | PASS (232 checks) | All 120 public core operations referenced by regression |
| Live UI | PENDING | Menu/ribbon control and two-face measurement reserved for the UI pass |

See [`FACE_ANGLE.md`](FACE_ANGLE.md) for operation details and scope.

## CADWorkbench v1.28 cylindrical face measurement (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Application and regression target rebuilt; version v1.28.0 |
| Focused regression | PASS (19 assertions) | Cylindrical side radius/diameter/axis, planar cap rejection, and stale-reference handling |
| Full CTest | PENDING | Run after this update |
| Source audit | PASS (232 checks) | All 120 public core operations referenced by regression |
| Live UI | PENDING | Menu/ribbon control and cylinder-face result reserved for the UI pass |

See [`CYLINDRICAL_FACE.md`](CYLINDRICAL_FACE.md) for operation details and scope.

## Distance between selected vertices (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Updated `CADWorkbench.exe` and `cadcore_tests.exe` built in `build_resume/Release` |
| Focused core regression | PASS (12 assertions) | Box diagonal, null output, missing body, stale references |
| Full CTest | PASS (37/37) | Includes `cadcore_vertex_distance` |
| Source audit | PASS | 136 checks; all 112 public `SceneDocument` operations referenced by regression |
| Live UI | PARTIAL | Inspect command visible; fewer-than-two selection rejected; numeric two-point pick pending because desktop input cannot Ctrl-click a second subshape |

See `VERTEX_DISTANCE.md` for behavior and the live UI limit.

## Selected vertex coordinates (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Updated `CADWorkbench.exe` and `cadcore_tests.exe` built in `build_resume/Release` |
| Focused core regression | PASS (59 assertions) | Box vertex coordinates, bounds and corners; invalid/missing/stale references; null output |
| Full CTest | PASS (37/37) | Includes `cadcore_vertex_coordinates` |
| Source audit | PASS | 136 checks; all 112 public `SceneDocument` operations referenced by regression |
| Live UI | PASS | Picked box corner reported X 0.000, Y 0.000, Z 20.000 mm |

See `VERTEX_COORDINATES.md` for usage and reference lifetime.

## Selected edge length (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Updated `CADWorkbench.exe` and `cadcore_tests.exe` built in `build_resume/Release` |
| Focused core regression | PASS (38 assertions) | Edge lengths, invalid/missing/stale references, unchanged document state |
| Full CTest | PASS (37/37) | Includes `cadcore_edge_length`, `cadcore_vertex_coordinates`, and `cadcore_vertex_distance` |
| Source audit | PASS | 136 checks; all 112 public `SceneDocument` operations referenced by regression |
| Live UI | PASS | Picked edge 5 on a 40 × 30 × 20 mm box; reported 20.000 mm, matching its Z extent |

See `EDGE_LENGTH.md` for usage and reference lifetime.

## Two-vector rectangular pattern (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release build | PASS | Application and regression targets compiled |
| Focused core regression | PASS (44 assertions) | Validation, positions, topology/volume, unique names, undo/redo, count edge case |
| Full CTest | PASS (27/27) | Includes `cadcore_vector_rectangular_pattern` |
| Source audit | PASS | 136 checks; all 102 public `SceneDocument` operations referenced by regression |
| Live UI | PASS | Two vectors `(1,1,0)` and `(0,0,1)`, counts 2 × 3; browser showed six bodies; selected copy bounds and 24,000 mm³ volume matched expected translation |
| Live Undo | PASS | Returned to the original one-body box |

See `VECTOR_RECTANGULAR_PATTERN.md` for details.

## Multi-face shell (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Core regression | PASS | Empty and duplicate openings rejected without mutation; opposing Z faces shell to a valid solid; material removed; undo restores original |
| Full CTest | PASS | Windows Release CTest passed 26/26 |
| Source audit | PASS | 136 source checks; regression references for all 101 public `SceneDocument` operations |
| Live UI shell | PASS | Opposing Z faces at 2 mm: 5,280 mm³; adjacent +X/+Z faces at 2 mm: 6,216 mm³; both results 10 faces/48 edges |
| Live Undo | PASS | Both cases restored 24,000 mm³; 6 faces/24 edges |

See `MULTI_FACE_SHELL.md` for details.

# CADWorkbench v1.9 validation matrix

## Selected face area (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built in `build_resume/Release` |
| Focused core regression | PASS (12 assertions) | Exact face area, invalid/missing/stale references, unchanged state, fresh reference after transform |
| Full CTest | PASS (34/34) | Includes `cadcore_face_area` |
| Source audit | PASS | 136 checks; all 109 public `SceneDocument` operations referenced by regression |
| Live UI | PASS | Picked the top of a 40 × 30 × 20 mm box; reported 1,200.000 mm² |

See `FACE_AREA.md` for usage and reference lifetime.

## Surface type summary (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Updated `CADWorkbench.exe` and `cadcore_tests.exe` built in `build_resume/Release` |
| Focused core regression | PASS (22 assertions) | Box, cylinder, sphere, cone, and torus classifications and area sums; invalid inputs; read-only behavior |
| Full CTest | PASS (34/34) | Includes `cadcore_surface_summary` |
| Source audit | PASS | 136 checks; all 109 public `SceneDocument` operations referenced by regression |
| Live UI | PASS | Box: 6 planar faces / 5,200 mm²; cylinder: 2 caps / 628.319 mm² and 1 cylindrical side / 2,199.115 mm² |

See `SURFACE_TYPE_SUMMARY.md` for scope.

## Planar section contour measurement (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` linked in `build_resume/Release` |
| Focused core regression | PASS (15 assertions) | X/Z section perimeter, nonintersecting plane, invalid inputs, unchanged history |
| Full CTest | PASS (34/34) | Includes `cadcore_planar_section` |
| Source audit | PASS | 136 checks; all 109 public `SceneDocument` operations referenced by regression |
| Live UI | PASS | 40 × 30 × 20 mm box; Z-normal plane at 10 mm reported 4 edges and 140.000 mm perimeter |

See `PLANAR_SECTION.md` for usage and scope.

## Geometry health and topology (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release build | PASS | Updated `CADWorkbench.exe` and `cadcore_tests.exe` linked in `build_resume/Release` |
| Focused core regression | PASS (20 assertions) | Unique solid topology, volume, area, bounds, centroid; invalid IDs/null output rejected; history unchanged |
| Full CTest | PASS (34/34) | Includes `cadcore_body_diagnostics` and density-based mass regression |
| Live UI | PASS | 40 × 30 × 20 mm box reported a valid closed solid; 1 solid, 1 shell, 6 faces, 6 wires, 12 edges, 8 vertices; volume 24,000 mm³; area 5,200 mm²; center (20, 15, 10) mm |

See `GEOMETRY_DIAGNOSTICS.md` for scope and limits.

## Density-based mass properties (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release build | PASS | Updated application and core-test targets linked in `build_resume/Release` |
| Focused core regression | PASS (14 assertions) | Density conversion, exact box mass and centroid, invalid density/body/output rejected, no history mutation |
| Full CTest | PASS (34/34) | Includes `cadcore_density_mass_properties` |
| Source audit | PASS | 136 checks; all 109 public `SceneDocument` operations referenced by regression |
| Live UI | PASS | 40 × 30 × 20 mm box at 7,850 kg/m³ reported 0.188400 kg, volume 24,000 mm³, center (20, 15, 10) mm |

See `MASS_FROM_DENSITY.md` for units and calculation details.

## Interference volume inspection (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release build | PASS | Isolated Release output folder avoided the running app's file lock |
| Qt runtime staging | PASS | Qt and Open CASCADE DLL/plugins staged; optional translations unavailable |
| Focused core regression | PASS (27 assertions) | Exact overlap and centroid, contained volume and centroid, disjoint and face-touching pairs, invalid inputs, no history mutation |
| Full CTest | PASS (34/34) | Includes interference and minimum-distance regressions |
| Source audit | PASS | 136 checks; all 109 public `SceneDocument` operations referenced by regression |
| Live UI | PASS | 840 mm³ overlap at centroid (7.5, 6, 7) mm; face contact returned zero |

Deferred UI cases and expected numeric results are listed in `INTERFERENCE_VOLUME.md`.

See `INTERFERENCE_VOLUME.md` for scope and limitations.

## Minimum distance inspection (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release build | PASS | Updated app and regression target built in isolated `build_resume` output |
| Focused core regression | PASS (23 assertions) | Separated, intersecting, face-touching, and vertically separated bodies; closest-point consistency; invalid inputs; no history mutation |
| Full CTest | PASS (34/34) | Includes `cadcore_minimum_distance` and interference regression |
| Source audit | PASS | 136 checks; all 109 public `SceneDocument` operations referenced by regression |
| Live UI | PASS | 5.0000 mm clearance; points (10, 0, 14) and (15, 0, 14) mm |

See `MINIMUM_DISTANCE.md` for scope and limitations.

## Through-all extrusion Cut (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Focused core regression | PASS | `cadcore_tests --oriented-extrusion`: six plane/direction cases, exact volume change, valid topology, commit/undo, invalid New Body and away-direction rejection |
| Live UI Cut | PASS | 5 × 5 mm XY profile through 40 × 30 × 20 mm box: volume 24,000 → 23,500 mm³; faces/edges 6/24 → 10/48 |
| Live Undo | PASS | Restored 24,000 mm³ and 6 faces/24 edges |
| Dialog behavior | PASS | Through All disables fixed extrusion height and is unavailable for New Body, symmetric, or tapered extrusion |
| Full CTest | PASS | Windows Release CTest passed 26/26 after adding forward/reverse cases on all three planes |

See `THROUGH_ALL_EXTRUSION_CUT.md` for usage and verification details.

## Oriented sketch-plane extrusion (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Focused geometry regression | PASS (249 assertions) | `cadcore_tests --oriented-extrusion`: XY/XZ/YZ at W=0 and W=7 mm in forward/reverse directions, symmetric extrusion at W=7 mm, reverse circle/taper geometry, through-all cuts in both directions on all planes, bounds/volume, topology, preview non-mutation, commit, undo/redo, invalid-plane/offset rejection |
| CTest | PASS | `cadcore_oriented_extrusion`; full suite 26/26 |
| Source audit | PASS | 136 checks; regression references for all 101 public `SceneDocument` operations |
| Live UI workflow | PASS for XZ at W=0 and W=7, forward/reverse | Preview/apply; Properties reported 24,000 mm³; W=7 forward bounds X 5…45, Y −27…−7, Z −4…26 mm; reverse Y −7…13 mm |
| Reverse Cut and Undo | PASS | Live 10 × 10 mm profile, 5 mm reverse cut: 24,000 → 23,600 mm³, 6/24 → 11/48 faces/edges; Undo restored the original volume and topology |
| YZ plane offset and reverse | PASS | Live 10 × 8 × 12 mm extrusion at U=10, V=−5, W=3 mm: 960 mm³; bounds X −9…3, Y 10…20, Z −5…3 mm; 6 faces/24 edges |
| XY plane offset and reverse | PASS | Live 8 × 6 × 4 mm extrusion at U=−3, V=7, W=−2 mm: 192 mm³; bounds X −3…5, Y 7…13, Z −6…−2 mm; 6 faces/24 edges |
| Symmetric-mode control behavior | PASS | Checking Symmetric about sketch plane disables Reverse extrusion direction in the live dialog |

See `ORIENTED_SKETCH_EXTRUSION.md` for controls and behavior.

## Multi-edge fillet and chamfer (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Focused geometry regression | PASS (31 assertions) | `cadcore_tests --multi-edge-blends`: validates two selected edges on one body, rejects empty/duplicate/mixed-body/stale/missing references, and checks valid geometry plus undo/redo for both tools |
| CTest | PASS | `cadcore_multi_edge_blends`; full suite 25/25 |
| Live UI workflow | Pending | Core preview, apply, undo, and redo pass; viewport Ctrl+click multi-edge selection remains to be verified live |

Select edges with Ctrl+click in Select → Edges mode, then use Fillet selected edges or Chamfer selected edges. See `MULTI_EDGE_BLENDS.md`.

## Asymmetric edge chamfer (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Focused geometry regression | PASS (27 assertions) | `cadcore_tests --asymmetric-chamfer`: validation, asymmetric/symmetric geometry comparison, invalidation, and undo/redo |
| CTest | PASS | `cadcore_asymmetric_chamfer`; full suite 25/25 |
| Live UI workflow | PASS | Preview and apply shown; Properties reported volume 24,000 to 23,960 mm³, 7 faces, 30 edges; undo and redo visually restored each state |

The reference side is selected deterministically from the edge's adjacent faces. See `ASYMMETRIC_CHAMFER.md` for behavior and limits.

## Custom-axis circular pattern (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Focused geometry regression | PASS (48 assertions) | `cadcore_tests --custom-axis-circular-pattern`: invalid-input rejection, full-circle geometry/bounds/volume, and undo/redo |
| CTest | PASS | `cadcore_custom_axis_circular_pattern`; full suite 23/23 |
| Source audit | PASS | 136 source checks; regression references found for all 99 public `SceneDocument` operations |
| Live UI workflow | PASS | Modify menu command created four visible 40 × 30 × 20 mm boxes around the world Z axis; undo restored one body and redo restored four |

See `CUSTOM_AXIS_CIRCULAR_PATTERN.md` for controls and scope.

## Nonuniform scale (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Focused geometry regression | PASS (23 assertions) | Invalid/missing factors, bounds and volume, identity no-op, undo/redo |
| CTest | PASS | `cadcore_nonuniform_scale`; full suite 22/22 |
| Live UI workflow | PASS | Modify menu command created scale X=2, Y=0.5, Z=3; volume 24,000 to 72,000 mm³; bounds 40×30×20 to 80×15×60 mm; undo/redo passed |

See `NON_UNIFORM_SCALE.md` for control limits and operation behavior.

## Elliptical loft (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Focused geometry regression | PASS (19 assertions) | Invalid dimensions, topology, expected bounds and analytical volume, undo/redo |
| CTest | PASS | `cadcore_elliptical_loft`; full suite 21/21 |
| Live UI workflow | PASS | Menu and ribbon commands found; default offset loft created; volume 17,886.131 mm³; bounds X −20…20, Y −12…12, Z 0…35 mm; 3 faces / 6 edges; undo/redo passed |

See `ELLIPTICAL_LOFT.md` for control definitions and limits.

## Linear countersink pattern (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Focused geometry regression | PASS (23 assertions) | `cadcore_tests --linear-countersink-pattern`: invalid inputs, through/blind cuts, valid shape, volume, undo/redo, and later-cutter rollback |
| CTest | PASS | `cadcore_linear_countersink_pattern`; full suite 20/20 |
| Live UI workflow | PASS | Three countersinks visible on 40 × 30 × 20 mm box; volume 24,000 to 23,315.987 mm³; topology 6/24 to 12/54; undo/redo passed |

Additional live blind-depth variant passed: three countersinks at 8 mm depth changed volume to 23,682.423 mm³ and topology to 15 faces / 54 edges; undo/redo passed.

See `LINEAR_COUNTERSINK_PATTERN.md` for dimensions and scope.

## Linear counterbore pattern (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully |
| Focused geometry regression | PASS (23 assertions) | `cadcore_tests --linear-counterbore-pattern`: invalid inputs, through/blind cuts, valid shape, volume, undo/redo, and later-cutter rollback |
| CTest | PASS | `cadcore_linear_counterbore_pattern`; full suite 20/20 |
| Live UI workflow | PASS | Three counterbores visible on 40 × 30 × 20 mm box; volume 24,000 to 23,111.418 mm³; topology 6/24 to 15/60; undo/redo passed |

See `LINEAR_COUNTERBORE_PATTERN.md` for dimensions and scope.

## Linear keyhole-slot pattern (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `cadcore_tests` and `CADWorkbenchLiveTest8.exe` built successfully |
| Focused geometry regression | PASS (22 assertions) | `cadcore_tests --linear-keyhole-slot-pattern`: invalid dimensions/count/spacing, missing body, late missed-slot rollback, rotated geometry, valid pattern solid, volume removal, undo/redo |
| CTest | PASS | `cadcore_linear_keyhole_slot_pattern` |
| Live UI workflow | PASS | Three head-and-throat profiles visible on a 40 x 30 x 20 mm box. Volume changed from 24,000 to 22,506.725 mm^3; topology from 6 faces / 24 edges to 30 / 138. Undo restored the box and redo restored the cuts |

See `LINEAR_KEYHOLE_PATTERN.md` for controls and scope.

## Circular counterbore pattern (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `cadcore_tests` and `CADWorkbenchLiveTest7.exe` built successfully |
| Focused geometry regression | PASS (21 assertions) | `cadcore_tests --circular-counterbore-pattern`: invalid dimensions/depth/count, missed body, full-circle through holes, blind partial-arc holes, valid topology, expected volume bounds, undo/redo |
| CTest | PASS | `cadcore_circular_counterbore_pattern` |
| Live UI workflow | PASS | Four stepped seats visible on a 40 x 30 x 20 mm box. Volume changed from 24,000 to 22,819.264 mm^3; topology from 6 faces / 24 edges to 18 / 72. Undo restored the box and redo restored the counterbores |

An additional live parameter check passed with three holes over a 180° partial arc, 8 mm blind depth, and 3 mm counterbore depth: volume 24,000 to 23,480.883 mm^3. Undo/redo restored both states.

See `CIRCULAR_COUNTERBORE_PATTERN.md` for controls and scope.

## Circular countersink pattern (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `cadcore_tests` and `CADWorkbenchLiveTest6.exe` built successfully |
| Focused geometry regression | PASS (28 assertions) | `cadcore_tests --circular-countersink-pattern`: invalid parameters, missing body, later missed cutter rollback, full-circle through pattern, blind partial-arc pattern, valid solids, expected volume removal, undo/redo for through and blind patterns |
| CTest | PASS | `cadcore_circular_countersink_pattern` |
| Live UI workflow | PASS | Four countersinks visible on a 40 x 30 x 20 mm box. Volume changed from 24,000 to 23,087.983 mm^3; topology from 6 faces / 24 edges to 14 / 64. Undo restored the original and redo restored the cuts |

Additional live parameter check passed: three blind countersinks over a 180° partial arc (8 mm depth) reduced volume to 23,682.423 mm^3 and produced 15 faces / 54 edges; undo/redo restored both states.

See `CIRCULAR_COUNTERSINK_PATTERN.md` for controls and scope.

## Linear rounded-slot pattern (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `cadcore_tests` and `CADWorkbenchLiveTest5.exe` built successfully |
| Focused geometry regression | PASS (29 assertions) | `cadcore_tests --rounded-slot-pattern`: invalid input, valid preview, material removal, rotated array, missed body, stale-preview handling, commit, undo, and redo |
| Related CTests | PASS (5/5) | Dovetail groove, T-slot groove, rounded slot, keyhole slot, and rounded-slot pattern |
| Live UI workflow | PASS | Three slots visible on a 40 x 30 x 20 mm box. Volume changed from 24,000 to 21,655.221 mm^3; topology from 6 faces / 24 edges to 33 / 120. Undo restored the original and redo restored all three slots |

See `ROUNDED_SLOT_PATTERN.md` for controls and scope.

## T-slot groove implementation (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `cadcore_tests` and `CADWorkbenchLiveTest2.exe` built successfully |
| Focused geometry regression | PASS (28 assertions) | `cadcore_tests --t-slot-groove`: invalid-input rejection, valid preview, volume change, rotation, missed body, stale-preview protection, undo, and redo |
| Groove CTests | PASS (2/2) | `cadcore_dovetail_groove` and `cadcore_t_slot_groove` |
| Live UI workflow | PASS | Created a 40 x 30 x 20 mm box, applied the default cut, observed volume 24,000 to 23,135.040 mm^3 and topology 6/24 to 15/72 faces/edges; undo restored 24,000 mm^3 and redo restored 23,135.040 mm^3 |

See `T_SLOT_GROOVE.md` for controls and scope.

## Rounded slot implementation (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `cadcore_tests` and `CADWorkbenchLiveTest3.exe` built successfully |
| Focused geometry regression | PASS (31 assertions) | `cadcore_tests --rounded-slot`: invalid input, preview validity, volume change, circular end case, rotation, missed body, stale-preview handling, undo, and redo |
| Related CTests | PASS (3/3) | `cadcore_dovetail_groove`, `cadcore_t_slot_groove`, and `cadcore_rounded_slot` |
| Live UI workflow | PASS | Created a 40 x 30 x 20 mm box and applied the default rounded slot. Volume changed from 24,000 to 23,218.407 mm^3, topology from 6 faces / 24 edges to 15 / 56; undo restored the original and redo restored the cut |

See `ROUNDED_SLOT.md` for controls and scope.

## Keyhole slot implementation (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows Release core and UI build | PASS | `cadcore_tests` and `CADWorkbenchLiveTest4.exe` built successfully |
| Focused geometry regression | PASS (28 assertions) | `cadcore_tests --keyhole-slot`: invalid head/throat proportions, preview validity, material removal, rotation, missed body, stale-preview handling, undo, and redo |
| Related CTests | PASS (4/4) | Dovetail groove, T-slot groove, rounded slot, and keyhole slot |
| Live UI workflow | PASS | Created a 40 x 30 x 20 mm box and applied the default keyhole cut. Volume changed from 24,000 to 23,357.480 mm^3, topology from 6 faces / 24 edges to 14 / 62; undo restored the original and redo restored the cut |

See `KEYHOLE_SLOT.md` for controls and scope.

## Dovetail groove implementation (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Core library compile | PASS | `cadcore` Release target compiled |
| MainWindow source compile | PASS | `MainWindow.cpp` compiled as part of the application target |
| Application link | PASS (alternate name) | `build/Release/CADWorkbenchLiveTest.exe` linked; the original `CADWorkbench.exe` was left untouched after its output path was locked |
| Focused geometry regression | PASS (28 assertions) | `cadcore_tests --dovetail-groove`: rejection, preview validity, material removal, rotation, stale-preview handling, undo, and redo |
| Live UI workflow | PASS; repeat attempt blocked | Earlier `CADWorkbenchLiveTest.exe` pass created a 40 x 30 x 20 mm box, previewed/applied the groove, measured volume 24,000 to 23,008.360 mm^3, and verified undo/redo. A repeat attempt after regression was blocked when Computer Use reported the open menu as a different target window |

See `DOVETAIL_GROOVE.md` for dimensions and implementation scope. Geometry and earlier live UI checks passed; one later UI repeat was blocked by a target-window mismatch.

The native STEP importer is covered by the geometry/persistence CTest: two-body fixture import, append behavior, body-volume check, undo, and malformed-file rollback. File ribbon presence and the STEP/STP file picker were confirmed in the live app; picker cancellation left the scene unchanged. Full UI import remains a manual check. See `STEP_IMPORT.md`.

## Automated Windows validation

Latest circular-hole-pattern update and live UI coverage are recorded below. Older per-feature sections retain historical results and are superseded where the latest update provides new verification.

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench` and `cadcore_tests` targets built |
| CTest | PASS (20/20) | Modeling tools, Wing rib, sweeps, form features, vector pattern, circular hole patterns, dovetail groove, T-slot groove, rounded slot, keyhole slot, rounded-slot pattern, circular and linear countersink/counterbore patterns, keyhole-slot pattern, geometry/persistence |
| Direct core regression | PASS (4,155 default-suite assertions; circular-hole focused test adds 15) | `cadcore_tests` and `cadcore_tests --circular-hole-pattern` |
| Component template defaults | PASS (405/405) | Invalid dimensions, topology, positive volume, insertion, and undo per template |
| Public core API coverage | PASS (96/96) | Every public `SceneDocument` operation appears in the regression suite |
| Source audit | PASS (136 checks) | `scripts/source_audit.py` |

Component catalog breakdown: 30 Mechanical, 30 Advanced, 35 Specialized, 70 Industrial, 70 Extended, 70 Precision & Utility, and 100 Production templates.

## Live Windows UI validation

The packaged Windows x64 application launched. The Create menu and Create ribbon each exposed all 100 Production Components; the Precision & Utility group remained available. Live workflows covered box creation and measured properties; save/open/new; undo/redo; STEP/STL export; Top view, orbit, zoom, and Fit All; selection/properties, rename, color, and visibility; duplicate, move, rotate, scale, and mirror; linear and circular patterns; split, shell, through-all hole, hole grid, selected-edge fillet/chamfer previews; two-body union; mass-properties measurement; Create-menu component-group visibility; counterbore and countersink holes; three patterned rounded slots; four-hole countersink and counterbore patterns; and a three-slot keyhole pattern. See `manual_qa_results.md` for observed values.

This was a workflow smoke test. The added operations were checked through the packaged Windows UI, including undo for scale, fillet, union, and circular pattern. It did not individually open every command dialog. The core regression covers the default geometry for all 405 component templates, including every Production template.

## Release gate still required

The W01-W24 manual release checklist and clean-VM deployment have not been completed. No approver or corresponding release log paths have been entered in `windows_verification.json`; that file is intentionally absent rather than fabricated. The release gate is therefore blocked pending genuine evidence and sign-off.

## Extended reviewer pass (2026-10-25)

| Check | Result | Evidence |
|---|---|---|
| Fresh Windows Release build | PASS | `scripts/build_windows.ps1`; application and native test targets compiled |
| Native CTest | PASS (1/1) | `cadcore_geometry_and_persistence` |
| Static source audit | PASS (81/81) | `scripts/source_audit.py` |
| Loft, revolve, extrusion, and component creation | PASS | Six generated bodies visible in the model browser |
| Countersink hole | PASS | Preview/apply succeeded; volume 24,000 to 23,586.095 mm^3; faces/edges 6/24 to 8/34 |
| Boolean intersection and subtract | PASS | Results added; undo restored original body count for each |
| Top/Front/Right/Isometric camera presets | PASS | Each preset changed the viewport orientation |
| Wireframe and XY reference grid toggles | PASS | Display mode and grid visibly switched on/off |
| Production Wing rib fidelity correction | PASS | Dedicated airfoil profile, spar opening and two lightening holes; see targeted verification below |

### Focused Wing rib correction

The Wing rib now exposes chord, airfoil depth, plate thickness, spar opening radius, and lightening hole radius. The generated geometry is one valid extruded airfoil solid with three through-openings. The focused Windows regression passed 14 assertions; the rebuilt UI showed the domain-specific dialog, generated the part, and undid it successfully. Other features were not retested in this focused pass.

No source code was changed in this pass. Build warnings and the component-fidelity issue are recorded in `manual_qa_results.md`. The extended UI sampling does not cover every command dialog, and W01-W24, clean-VM deployment, and approver sign-off remain open.

## Scope and comparison

The 100 Production commands generate illustrative dimensioned direct-solid templates using nine reusable geometry families. See `FEATURES_V1_9.md` for the catalog. The comparison benchmark defines 30 capability areas: 6 implemented, 4 partial, and 20 not implemented. Threads, seals, certified profiles, internal mechanisms, joints, load ratings, tolerances, and manufacturing details may be simplified or omitted.

## Profile sweep verification (2026-10-25)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench` and `cadcore_tests` targets built |
| Focused profile sweep test | PASS (11 assertions) | `cadcore_tests --sweeps` |
| Full native CTest suite | PASS (4/4) | `cadcore_modeling_tools`, `cadcore_wing_rib`, `cadcore_sweeps`, `cadcore_geometry_and_persistence` |
| Live Create menu/ribbon | PASS | Both circular and rectangular sweep commands present |
| Live dialogs and generated viewport geometry | NOT RUN | Core geometry is tested; interactive dialog flow remains a manual check |

The two sweep operations create separate solid bodies from generated smooth paths. Arbitrary sketch paths, twist control, and join/cut modes are not implemented. See `PROFILE_SWEEP.md`.

## Helical sweep update (2026-10-25)

| Check | Result | Evidence |
|---|---|---|
| Focused helical sweep regression | PASS (9 assertions) | Invalid dimensions, topology, insertion/name, undo, and redo |
| Full CTest | PASS (5/5) | Modeling tools, Wing rib, sweeps, helical sweep, geometry/persistence |
| Live command/menu/ribbon | PASS | Entry visible in Create menu and ribbon; parameter dialog opened |
| Live solid and history | PASS | Default coil visible in viewport/browser; Ctrl+Z removed and Ctrl+Y restored it |

See `HELICAL_SWEEP.md` for dimensions and scope limitations.

## Variable-pitch spring update (2026-10-25)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Application and native test targets compiled and packaged |
| Focused geometry regression | PASS (10 assertions) | Input rejection, valid topology, insertion/name, undo, and redo |
| Full CTest | PASS (6/6) | Includes the new `cadcore_variable_pitch_helix` regression |
| Source audit | PASS (131 checks) | `scripts/source_audit.py` |
| Live app workflow | PENDING | Menu and ribbon actions compile; app launch was unavailable through this session |

See `VARIABLE_PITCH_HELIX.md` for the operation controls and scope.

## Elliptical sweep and tapered spring update (2026-10-25)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Application and core test targets compiled |
| Sweep regression | PASS (17 assertions) | Circular, rectangular, and elliptical sweep validation, topology, insertion, undo |
| Tapered spring regression | PASS (10 assertions) | Input validation, topology, body insertion, undo, redo |
| Full CTest | PASS (7/7) | Includes the `cadcore_tapered_helix` test plus expanded sweep coverage |
| Source audit | PASS (116 checks) | `scripts/source_audit.py` |
| Live UI workflows | PENDING | Menu/ribbon actions compile; the app was unavailable to relaunch from this session |

See `ELLIPTICAL_SWEEP.md` and `TAPERED_HELIX.md` for controls and limits.

## Five profile and transition tools (2026-10-25)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Application and test targets compiled |
| Focused form-feature regression | PASS (30 assertions) | All five new direct-solid tools, invalid input, topology, positive volume, naming, and undo |
| Full CTest | PASS (8/8) | Includes `cadcore_form_features` |
| Source audit | PASS (131 checks) | `scripts/source_audit.py` |
| Live UI click-through | PENDING | Menu and ribbon actions compile; dialogs not exercised in the live app during this turn |

See `FORM_FEATURES.md` for tool controls and limits.

## Vector linear pattern and API coverage update (2026-10-25)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | App and test targets compiled |
| Vector pattern regression | PASS (17 assertions) | Invalid inputs, positions, topology, one-step undo and redo |
| Full CTest | PASS (9/9) | Includes `cadcore_vector_pattern` |
| Public core API references | PASS (80/80) | Automated declaration-to-regression source audit; see its scope in `local_verification.json` |
| Source audit | PASS (131 checks) | `scripts/source_audit.py` |
| Live UI workflow | PENDING | Vector dialog was not exercised in the running app |

See `VECTOR_PATTERN.md` for operation details.

## Live UI verification of previously untested features (2026-10-25)

The rebuilt Windows Release app was exercised in an empty, disposable Untitled document. Each dialog accepted its default parameters, and the resulting body was confirmed in the model browser and viewport:

| Feature | Live result |
|---|---|
| Variable-pitch spring | Visible `Variable Pitch Helix` body created |
| Tapered spring | Visible `Tapered Helix` body created |
| Elliptical profile sweep | Visible `Elliptical Sweep` body created |
| Elliptical prism | Body created |
| Elliptical tube | Body created |
| Circular loft | Body created |
| Rectangular-to-circular transition | Body created |
| Hollow loft reducer | Body created |
| Linear pattern along vector | Dialog showed direction, instance count, and spacing; selected prism produced two additional pattern bodies |
| Circular hole pattern | Four through-all holes cut the selected prism; volume changed from 16,964.600 to 16,116.370 mm³ and face/edge counts from 3/6 to 7/30 |

This verifies these dialogs and default UI workflows. The Create ribbon showed all eight related commands, and the Modify ribbon showed the vector and circular hole pattern buttons. Ribbon buttons were not individually activated; the 100 Production Component dialogs were not each opened. The broader W01-W24 release checklist and clean-VM install remain outstanding.
## Body schedule CSV export (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` rebuilt in `build_resume/Release` |
| Focused regression | PASS (14 assertions) | CSV metrics, escaped names, visibility, row count, invalid path and empty-document handling |
| Focused CTest | PASS (1/1) | `cadcore_body_schedule_csv` |
| Full CTest | PASS (38/38) | Complete configured Windows Release suite |
| Source audit | PASS | 141 checks; all 113 public core operations referenced by regression |
| Live UI | PARTIAL | File menu command and CSV save dialog opened for a test box; saving through the dialog is pending |

See [`BODY_SCHEDULE_CSV.md`](BODY_SCHEDULE_CSV.md) for the exported columns and limitation.

## Per-body material presets (2026-10-26)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` rebuilt in `build_materials_round/Release` |
| Focused regression | PASS (50 assertions) | Preset/custom and multi-body assignment, history, persistence, density, invalid input, duplicate/mirror/pattern inheritance, matching and mixed-material unions |
| Focused CTest | PASS (1/1) | `cadcore_body_materials` |
| Full CTest | PASS (39/39) | Complete Windows Release suite |
| Source audit | PASS | 145 checks; all 114 public core operations referenced by regression |
| Live UI | PASS | Assigned Aluminum 6061 to a 40 × 30 × 20 mm box; properties showed 2,700 kg/m³ and mass dialog defaulted to 2,700 kg/m³. A custom UI Test Alloy at 7,850 kg/m³ remained visible in body properties and mass inspection reported 0.188400 kg for 24,000 mm³. |

See [`MATERIAL_ASSIGNMENTS.md`](MATERIAL_ASSIGNMENTS.md) for behavior and scope.

## CADWorkbench v1.10.0 custom-plane modeling (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Application and core-test target rebuilt in `build_materials_round/Release` |
| Custom-plane extrusion regression | PASS (24 assertions) | Oblique frame, centroid and volume, non-mutation, commit/undo/redo, invalid frame, forward/reverse through-all Cut |
| Custom-axis revolve regression | PASS (18 assertions) | Oblique axis, analytical volume/centroid, partial 90° sweep, invalid direction, undo/redo |
| Existing oriented extrusion regression | PASS (249 assertions) | XY/XZ/YZ modes, reverse, offsets, taper, Through All Cut and history behavior retained |
| Full CTest | PASS (41/41) | All configured Windows Release tests |
| Source audit | PASS | 145 checks; all 115 public core operations referenced by regression |
| Live custom-plane UI | PASS | Angled 40 × 30 × 20 mm rectangle created; 24,000 mm³, six faces, 24 edges; undo/redo verified |
| Live custom-axis revolve UI | PASS | Cylinder around (1, 1, 1); 6,283.185 mm³, three faces, six edges; undo/redo verified |

See [`FEATURES_V1_10.md`](FEATURES_V1_10.md), [`CUSTOM_PLANE_EXTRUSION.md`](CUSTOM_PLANE_EXTRUSION.md), and [`CUSTOM_AXIS_REVOLVE.md`](CUSTOM_AXIS_REVOLVE.md).

## CADWorkbench v1.11.0 BOM and parts data (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` rebuilt in `build_materials_round/Release` |
| BOM/parts/standards regression | PASS (42 assertions) | Metadata, inheritance, quantities, volume/mass, CSV escaping and failures, quality findings, conflict detection, and v3 persistence |
| Full CTest | PASS (42/42) | All configured Windows Release tests, including `cadcore_bom_parts_standards` |
| Live BOM and parts list | PASS | Aluminum 6061 box: 24,000 mm³, 0.064800 kg; duplicated part grouped to quantity 2, 48,000 mm³, 0.129600 kg; two rows in parts list |
| Live CSV exports | PASS | BOM and parts-list dialogs saved files; both CSV contents inspected and matched the UI values |
| Live BOM data profile | PASS | Populated test model; 0 current errors and 0 warnings |
| Live ISO readiness profile | PASS with scope limit | 0 current model/BOM errors and warnings; details say ISO title block, projection, dimensions, and tolerance rules are not checked before drawings |
| Live ASME readiness profile | PASS with scope limit | 0 current model/BOM errors and warnings; details say Y14 title-block, dimensioning, and GD&T rules are not checked before drawings |
| ISO dimensional tolerances / ASME drawing and GD&T | NOT IMPLEMENTED | Requires the 2D drawing entities and a specified tolerance convention; see `STANDARDS_CHECKS.md` |

See [`BOM_PARTS_LIST.md`](BOM_PARTS_LIST.md) and [`STANDARDS_CHECKS.md`](STANDARDS_CHECKS.md).

## CADWorkbench v1.12.0 orthographic drawing export (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Application and test target rebuilt in `build_materials_round/Release`; live app title reports v1.12.0 |
| Drawing export regression | PASS (16 assertions) | A4 SVG, orthographic views, visible/hidden edges, metadata, X/Y/Z envelope dimensions and error cases |
| Full CTest | PASS (43/43) | All configured Windows Release tests |
| Live Windows UI export | PASS | Exported selected 40 × 30 × 20 mm box; status bar confirmed success |
| Live SVG artifact | PASS | XML parsed; front, top, right, `PLATE-001`, revision B, three dimension annotations, and 24 model-edge polylines present |
| Overall-envelope dimensions | PASS | Live 40 × 30 × 20 mm box export and regression both confirm X 40.000, Y 30.000, Z 20.000 mm labels |
| Standards scope | PARTIAL | Only bounding-envelope dimensions; no feature dimensions, tolerances, datums, projection symbol, or GD&T; ISO/ASME drawing checks remain unimplemented |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) for details and limits.

## CADWorkbench v1.13.0 projection layouts (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | App and test executable built; app title reports v1.13.0 |
| Focused drawing regression | PASS (19 assertions) | First/third-angle labels and view placement, A4 SVG, envelope dimensions, metadata, and export failures |
| Full CTest | PASS (43/43) | All configured Windows Release tests |
| Live projection picker | PASS | UI offered both conventions; exported one of each from the selected test body |
| Projection SVG artifacts | PASS | Both XML files contain front/top/right, X/Y/Z envelope dimensions and 24 model-edge polylines |
| Rendered layout review | PASS | Both SVG previews inspected; view placement and title-block positions match each projection convention |
| Standards scope | PARTIAL | No standardized projection symbol, feature dimensions, tolerances, datums, GD&T or standards validation |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) for detailed scope.

## CADWorkbench v1.21 projection-convention symbol (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Rebuilt v1.21.0 CADWorkbench.exe and cadcore_tests.exe in build_materials_round/Release |
| Focused drawing regression | PASS (41 assertions) | Covers projection-symbol identity and circular-end-view placement for both conventions, along with prior drawing features |
| Full CTest | PASS (43/43) | Windows x64 Release configuration |
| Live Windows SVG export | PASS | Exported first-angle and third-angle A4 drawings from a 10 mm radius × 35 mm cylinder; saved SVGs contain matching symbols at cx=282 and cx=261, and Ø20.000 mm callouts |
| Standards scope | LIMITED | Schematic projection marker only; standards conformance is not claimed |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) for supported drawing content and limits.

## CADWorkbench v1.20 cylindrical feature diameter callouts (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Rebuilt CADWorkbench.exe and cadcore_tests.exe in build_materials_round/Release |
| Focused drawing regression | PASS (40 assertions) | Includes true diameter annotation on an end-on cylindrical feature |
| Full CTest | PASS (43/43) | Windows Release configuration |
| Source audit | PASS | Updated drawing-source coverage; public SceneDocument API references remain 118/118 |
| Live Windows export | PASS | 10 mm radius × 35 mm cylinder exported to A4 third-angle sheet; SVG contains `Ø20.000 mm` |
| BOM and parts-list live verification | PASS | 40 × 30 × 20 mm box produced one BOM row and one parts-list row with 24,000 mm³ volume |
| ISO/ASME exported SVG screens | PASS (readiness only) | Each profile returned 0 structural errors and 3 missing title-block metadata warnings |
| Standards scope | LIMITED | No clause-level ISO/ASME tolerance, datums, or GD&T certification |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md), [`BOM_PARTS_LIST.md`](BOM_PARTS_LIST.md), and [`STANDARDS_CHECKS.md`](STANDARDS_CHECKS.md).

## CADWorkbench v1.19 exported drawing readiness screen (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | v1.19.0 CADWorkbench.exe and cadcore_tests.exe rebuilt in build_materials_round/Release |
| Focused drawing regression | PASS (39 assertions) | Complete/malformed SVG, missing view, metadata, sheet/viewBox mismatch, and unsupported-profile cases |
| Full CTest | PASS (43/43) | Windows Release using CTest -C Release |
| Source audit | PASS | 190 source checks; all 118 public core operations referenced by regression |
| Live ISO/ASME SVG screen | PARTIAL | Earlier UI run on the saved A4 cylinder: 0 errors and 3 expected blank metadata warnings per profile. Final v1.19 relaunch not verified because Computer Use stopped on physical Escape |
| Combined drawing profiles | PASS | Earlier UI run showed 0 errors and 6 warnings, with separate ISO/ASME details and scope notes |
| Standards scope | LIMITED | Structural drawing readiness only; clause-level ISO/ASME, tolerances, datums, and GD&T are not checked |

## CADWorkbench v1.14.0 drawing sheet sizes (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Rebuilt application and test targets in `build_materials_round/Release`; live app title reports v1.14.0 |
| Focused drawing regression | PASS (22 assertions) | A4/A3 dimensions and scale, both projection layouts, metadata, envelope dimensions, and error cases |
| Full CTest | PASS (43/43) | All configured Windows Release tests, including A3 paper-size assertions |
| Live Windows UI export | PASS | Selected First-angle and A3 landscape for the 40 × 30 × 20 mm `PLATE-001` test body; status bar confirmed export |
| Live SVG validation | PASS | Parsed XML; 420 × 297 mm canvas, all views, X/Y/Z dimensions, `SCALE 1.41:1`, and 24 model-edge polylines |
| Rendered A3 sheet review | PASS | Headless SVG render inspected; views, dimensions, and title block fit on the landscape sheet |
| Standards scope | PARTIAL | Feature dimensions, tolerances, datums, projection symbol, GD&T, and ISO/ASME drawing validation remain unimplemented |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) for scope and limitations.

## CADWorkbench v1.18.0 cylindrical centerlines (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Rebuilt CADWorkbench.exe and cadcore_tests.exe in build_materials_round/Release |
| Focused drawing regression | PASS | 32 assertions, including cylindrical-axis and end-view center marks |
| Full CTest | PASS | 43/43 Windows Release tests |
| Source audit | PASS | 186 source checks; all 118 public core operations referenced by regression tests |
| Live Windows UI export | PASS | Created and selected a 10 mm × 35 mm cylinder; exported third-angle A4 isometric drawing; status bar confirmed success |
| Live SVG validation | PASS | XML parsed as 297 × 210 mm; 31 polylines, including five centerline/center-mark strokes |
| Rendered sheet review | PASS | A4 preview inspected: top-view center mark, front/right centerlines, and isometric axis line are visible; sheet geometry and title block fit. See `docs/evidence/orthographic-cylinder-centerlines-v1.18.png` |
| Standards scope regression and live UI | PASS | BOM/standards regression passed 44 assertions; combined UI on a metadata-incomplete cylinder showed 0 errors, 12 warnings, three separate profiles, and the updated drawing-inspection scope note |
| Standards scope | PARTIAL | Actual feature dimensions, tolerances, datums, GD&T, and ISO/ASME validation remain unimplemented |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) for detailed scope.

The combined “All available checks” UI was also exercised on the test box: 0 errors, 12 expected missing-metadata warnings, and separate BOM, ISO, and ASME readiness findings.



## CADWorkbench v1.21 ISO and ASME live UI verification (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| ISO drawing readiness, first-angle SVG | PASS | 0 errors, 3 warnings; blank part number, revision, and material |
| ASME drawing readiness, first-angle SVG | PASS | 0 errors, 3 warnings; profile detail includes its scope limitation |
| ISO drawing readiness, third-angle SVG | PASS | 0 errors, 3 warnings; no structural errors |
| ASME drawing readiness, third-angle SVG | PASS | 0 errors, 3 warnings; profile detail includes its scope limitation |
| Combined BOM / ISO / ASME model check | PASS | Active cylinder: 0 errors, 12 expected missing-metadata warnings; expanded view shows separate profile headings and four warnings per profile |
| Standards scope | LIMITED | Readiness/content screening only; no clause-level ISO/ASME, tolerance-table, datum, or GD&T compliance verification |

See [standards check limits](STANDARDS_CHECKS.md) and the [v1.21 exported drawing evidence](ORTHOGRAPHIC_DRAWINGS.md#verification).

## CADWorkbench v1.27 issue-date validation (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` rebuilt; app version is v1.27.0 |
| Focused drawing regression | PASS (64 assertions) | Valid date accepted; malformed export date rejected; imported malformed date produces `DRAWING_ISSUE_DATE_FORMAT` |
| Full CTest | PASS (43/43) | Windows x64 Release suite |
| Source audit | PASS (222 checks) | All 118 public core operations referenced |
| Final v1.27 SVG export | PASS | Revision A, date `2026-10-27`; XML parsed; ISO and ASME each 0 errors / 0 warnings |
| Live malformed-date screen | PASS | ISO and ASME each reported 0 structural errors and 1 `DRAWING_ISSUE_DATE_FORMAT` warning; non-certification details verified |
| Standards scope | LIMITED | Date-format check only; no ISO/ASME clause-level certification |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) and [`STANDARDS_CHECKS.md`](STANDARDS_CHECKS.md).

## CADWorkbench v1.26 assembly hierarchy report (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built; window reports v1.26.0 |
| Full CTest | PASS (43/43) | Windows x64 Release suite |
| Source audit | PASS (219 checks) | All 118 public core operations referenced by regression |
| Live report tree | PASS | `Powertrain → Drive → Drive housing` displayed from assembly path metadata |
| Live assembly roll-up | PASS | One 24,000 mm³ part showed quantity 1; after duplicate, parent and leaf showed quantity 2 and 48,000 mm³ |
| Cleanup | PASS | Temporary duplicate discarded; saved scene left unchanged |
| Remaining scope | LIMITED | Tree is derived from path labels, not persistent editable assembly entities or subassembly definitions |

See [`BOM_PARTS_LIST.md`](BOM_PARTS_LIST.md) for the report behavior and limitations.

## CADWorkbench v1.25 drawing revision field (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Rebuilt `CADWorkbench.exe` and `cadcore_tests.exe`; app title reports v1.25.0 |
| Focused drawing regression | PASS (61 assertions) | Revision override and XML escaping, default part revision, and 16-character validation |
| Focused BOM/parts/standards regression | PASS (69 assertions) | Procurement metadata, assembly paths, grouping, CSV, migration, and readiness checks |
| Full CTest | PASS (43/43) | Windows x64 Release test suite |
| Live title-block export | PASS | Exported `title_block_ui_test_v125.svg` with revision A, author, checker, drawing number, and date; XML parsed and values confirmed |
| Live ISO and ASME drawing screens | PASS (0 errors, 0 warnings each) | Revision warning cleared; scope remains basic drawing-content screening, not clause-level conformance |
| Source audit | PASS (217 checks) | All 118 public core operations referenced by regression |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) and [`STANDARDS_CHECKS.md`](STANDARDS_CHECKS.md) for implemented scope and remaining work.

## CADWorkbench v1.24 drawing title-block metadata (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Rebuilt `CADWorkbench.exe` and `cadcore_tests.exe` with the title-block fields and v1.24 app version |
| Focused drawing regression | PASS (60 assertions) | Metadata, XML escaping, optional part-number fallback, missing drawing-number finding, and field-length validation |
| Focused BOM/parts/standards regression | PASS (69 assertions) | Procurement metadata, assembly paths, grouping, CSV, migration, and standard-readiness checks |
| Full CTest | PASS (43/43) | Full Windows Release test suite |
| Live SVG export | PASS | `title_block_ui_test.svg` includes entered general note, author, checker, issue date, and default part-based drawing number; XML parsed successfully |
| Live ISO and ASME drawing screens | PASS (0 errors, 1 warning each) | Both screens found no structural errors; the sole warning is the test body's missing revision. Scope details disclaim standards certification. |
| Standards coverage | LIMITED | No ISO 7200 conformance, clause-level checks, tolerance lookup, ASME Y14, datum, or GD&T verification |

See [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) and [`STANDARDS_CHECKS.md`](STANDARDS_CHECKS.md) for current scope and limits.

## CADWorkbench v1.22 assembly-aware BOM paths (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | Rebuilt `CADWorkbench.exe` and `cadcore_tests.exe` in `build_materials_round/Release` |
| BOM/parts/standards regression | PASS (53 assertions) | Path normalization and validation, duplicate inheritance, path-sensitive BOM rows, CSV fields, and schema-v4 save/reopen |
| Drawing regression | PASS (54 assertions) | Existing orthographic/projection checks plus note escaping, length, line-count, and control-character validation |
| Live metadata editor | PASS | Entered part number `PUMP-BOX-001`, description `Drive housing`, and path `Powertrain/Drive` |
| Live BOM and parts list | PASS | Both report tabs displayed `Powertrain/Drive` in the assembly-path column |
| Live scene persistence | PASS | Save As wrote `assembly_path_ui_test.cadscene`; inspected scene identifies schema v4 and retains the part number and assembly path |
| Live drawing notes with ISO and ASME screens | PASS | Exported two user-entered notes into SVG; both screens returned 0 errors and 2 missing-metadata warnings |
| Full CTest and source audit | PASS | 43/43 Windows Release tests; 205 static checks; 118/118 public operations referenced |
| Scope | LIMITED | Slash-separated ancestry labels guide flat BOM grouping; no editable tree or recursive subassembly quantity rollups |

## CADWorkbench v1.23 procurement-aware BOM (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` rebuilt in `build_materials_round/Release` |
| BOM/parts/standards regression | PASS (69 assertions) | Supplier validation, grouping, duplicate inheritance, missing-supplier findings, CSV fields, and schema-v5 roundtrip |
| Drawing regression | PASS (54 assertions) | Orthographic/projection, section, readiness, and general-note validation |
| Live part editor | PASS | Schema-v4 scene opened; part edited as Purchased with supplier `ACME UI Supply` and supplier number `UI-420` |
| Live BOM and parts list | PASS | Both report tabs showed assembly path, procurement type, supplier, and supplier part number |
| Live BOM quality check | PASS | 0 errors and 2 expected warnings for unset revision and material; expanded details inspected |
| Live scene persistence | PASS | Saved and inspected `purchased_item_ui_test.cadscene`; schema v5 retained purchasing and assembly-path fields |
| Full CTest and source audit | PASS | 43/43 Windows Release tests; 209 static checks and 118/118 public core operations referenced |
| Scope | LIMITED | No supplier catalog, prices, inventory, purchase orders, ERP links, or recursive assembly rollups |
## CADWorkbench v1.33 persistent drawing note annotations (2026-10-27)

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench.exe` and `cadcore_tests.exe` built successfully after adding persistent note records and editor. |
| Persistent drawing regression | PASS (63 assertions) | Sheet/view IDs, saved centers, note text/IDs/positions, save/reopen, undo/redo, and malformed placement/duplicate-ID rejection. |
| Orthographic drawing regression | PASS (77 assertions) | SVG exports stable sheet/view/note IDs, applies saved centers, formats paper coordinates, and XML-escapes note text. |
| Full Windows CTest | PASS (49/49) | Release suite completed in 48.70 seconds. |
| Source audit | PASS (250 checks) | All 126 public core operations are referenced by regression coverage. |
| Live UI | NOT RUN | Note editor/ribbon and scene save/reopen remain to be checked in the desktop app. |
| Scope | PARTIAL | Notes and view positions are persistent; dimensions remain generated SVG annotations, sheets remain single-body, and no balloon, linked BOM table, detail view, PDF, or live SVG refresh is included. |

## CADWorkbench v1.32 persistent placed-view drawing sheets (2026-10-27)

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
| Focused core regression | PASS (57 assertions) | Box vertex coordinates, bounds and corners; invalid/missing/stale references; null output |
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

The 100 Production commands generate illustrative dimensioned direct-solid templates using nine reusable geometry families. See `FEATURES_V1_9.md` for the catalog. The comparison benchmark defines 30 capability areas: 6 implemented, 3 partial, and 21 not implemented. Threads, seals, certified profiles, internal mechanisms, joints, load ratings, tolerances, and manufacturing details may be simplified or omitted.

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

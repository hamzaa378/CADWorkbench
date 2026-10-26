# CADWorkbench v1.9 validation matrix

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
| CTest | PASS (18/18) | Modeling tools, Wing rib, sweeps, form features, vector pattern, circular hole patterns, dovetail groove, T-slot groove, rounded slot, keyhole slot, rounded-slot pattern, countersink/counterbore patterns, keyhole-slot pattern, geometry/persistence |
| Direct core regression | PASS (4,155 default-suite assertions; circular-hole focused test adds 15) | `cadcore_tests` and `cadcore_tests --circular-hole-pattern` |
| Component template defaults | PASS (405/405) | Invalid dimensions, topology, positive volume, insertion, and undo per template |
| Public core API coverage | PASS (94/94) | Every public `SceneDocument` operation appears in the regression suite |
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

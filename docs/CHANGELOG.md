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

## Verification and source package refresh - 2026-10-26

- Refreshed the Windows verification snapshot to CTest 18/18 and the source audit to 136 checks, covering all 94 public `SceneDocument` operations with regression references.
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



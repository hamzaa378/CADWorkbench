# Changelog

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



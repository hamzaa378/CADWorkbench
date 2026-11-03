# Historical README

This document preserves the earlier project README. Its feature status and links may be outdated; use the root README and CONTINUE_HERE.md for the current snapshot.

# CADWorkbench

Current source package: **v1.55.0 with the latest working-tree additions**. Read [CONTINUE_HERE.md](CONTINUE_HERE.md) first for the current feature set, Windows build commands, verified tests, and next-chat handoff. Some detailed tables below describe earlier stages and have not yet been refreshed for every recent addition.

CADWorkbench is a native Windows 3D CAD application built with C++20, Qt 6, and Open CASCADE Technology (OCCT). It focuses on direct solid modeling and dimensioned engineering templates.

## Current capabilities

- Create and inspect solids, profiles, and dimensioned component templates.
- Extrude sketch profiles on XY, XZ, YZ, or a user-defined 3D plane with local U/V positioning, signed plane offset W, reverse/symmetric directions, and through-all cuts.
- Revolve a rectangular profile around world or arbitrary 3D axes with an editable axis origin, direction, and sweep angle.
- Edit models with Boolean operations, holes, patterns, mirror, single- or multi-face shell, split, draft, fillet, chamfer, and transforms.
- Scale selected bodies independently along X, Y, and Z, or use uniform scaling.
- Pattern selected bodies around an arbitrary 3D axis with an editable origin, direction, instance count, and sweep angle.
- Inspect the minimum clearance between two selected bodies and read a closest-point pair.
- Measure the shared solid volume and centroid of two selected bodies to inspect geometric interference.
- Inspect selected face area, selected edge length, and picked vertex coordinates in the viewport.
- Measure the oriented angle between two planar faces and the separation between their planes when parallel. See [`docs/FACE_ANGLE.md`](docs/FACE_ANGLE.md).
- Measure the radius, diameter, and axis of a selected cylindrical face. See [`docs/CYLINDRICAL_FACE.md`](docs/CYLINDRICAL_FACE.md).
- Assign persistent material and density presets to bodies; mass-property inspection uses the assigned density by default.
- Measure straight-line distance between two vertex references; the core test passes, while numeric live UI verification awaits a Ctrl-click capable desktop input path.
- Navigate with mouse orbit, pan, zoom, and standard camera views.
- Save and reopen native `.cadscene` documents; use undo and redo.
- Import/export STEP assemblies and IGES solids; import closed STL and OBJ meshes as faceted solids and export STL. See [`docs/NEUTRAL_FILE_EXCHANGE.md`](docs/NEUTRAL_FILE_EXCHANGE.md).
- Export a CSV body schedule with stable body IDs, names, visibility, volume, area, extents, and center coordinates.
- Maintain persistent part numbers, descriptions, revisions, purchased/manufactured status, supplier details, and assembly paths; view grouped BOM quantities, per-body parts lists, and a derived assembly tree with quantity/volume/mass roll-ups; export BOM and parts lists as CSV. See [`docs/BOM_PARTS_LIST.md`](docs/BOM_PARTS_LIST.md).
- Store reusable component identities and fixed XYZ-offset occurrences in the scene; explicit definition synchronization updates instance geometry and shared BOM/material data while preserving per-occurrence placement and visibility. Linear and world-Z circular component occurrence patterns create up to 100 BOM-linked placements in one undoable command. Rigid, revolute, and prismatic parent-child joints persist; combined poses solve parent-first, and saved motion tracks interpolate angle/linear keyframes. STEP import/export uses OCCT XDE to retain names, nested assembly paths, reusable definitions, occurrence IDs, and placements where the STEP file provides them. Closed manifold STL meshes import as faceted solids. Drawing sheets store source IDs and separate geometry and BOM/material revisions, projected views, positioned notes, overall dimensions, a source-scoped BOM table, and persistent BOM balloons with source/item identities. See [`docs/PERSISTENT_ASSEMBLIES.md`](docs/PERSISTENT_ASSEMBLIES.md), [`docs/STL_MESH_IMPORT.md`](docs/STL_MESH_IMPORT.md), and [`docs/ORTHOGRAPHIC_DRAWINGS.md`](docs/ORTHOGRAPHIC_DRAWINGS.md).
- Create a focused parametric XY rectangle, base extrusion, through-hole, optional X- or Y-direction linear hole pattern, and either an optional all-edge chamfer or fillet. Named dimensions, constraints, and feature history persist and rebuild atomically with undo/redo. See [`PARAMETRIC_FOUNDATION.md`](PARAMETRIC_FOUNDATION.md).
- Develop one-bend SVG and two-to-fifty-bend DXF flat patterns from per-bend rules; multi-bend DXF supports optional rectangular end reliefs at each bend station. Faceted folded sheet solids preserve editable flange/bend inputs, rebuild safely, and export their current definition to a one-to-fifty-bend DXF blank with a selected K-factor. Exports are snapshots without reliefs; springback and tooling remain out of scope. See [`docs/SHEET_METAL.md`](docs/SHEET_METAL.md).
- Run BOM data-quality and ISO/ASME drawing-readiness checks, plus configurable required title-block fields and eight annotation-presence conventions, including sections, details, linked parts lists, and BOM balloons. Saved-sheet checks flag each missing requirement and stale/missing model sources; requirements can be saved as project defaults. A source link can be refreshed before re-export. SVG exports remain snapshots. These checks do not claim standards compliance. See [`docs/STANDARDS_CHECKS.md`](docs/STANDARDS_CHECKS.md).
- Export an A4 or A3 landscape SVG drawing from one to fifty selected solids as a composite source, with selectable first-angle or third-angle layout, front/top/right views, an isometric or section view, visible/hidden edges, cylindrical centerlines and diameter callouts, notes, title-block metadata, overall envelope dimensions, and a linked parts list scoped to the selected source bodies. Source IDs and aggregate revisions can be checked against current bodies. Issue dates use `YYYY-MM-DD`; SVGs remain export snapshots. See [`docs/ORTHOGRAPHIC_DRAWINGS.md`](docs/ORTHOGRAPHIC_DRAWINGS.md).
- Saved scene sheets retain source-body IDs, aggregate geometry revision, stable view identities, editable paper-space centers, up to four selectable circular detail views, and up to four separately positioned X/Y/Z axis-aligned section cuts. Exported SVG/PDF files remain snapshots and do not update automatically with model changes. Feature-linked dimensions, arbitrary section planes/paths, and automatic drawing refresh are not implemented.
- Create a dimensioned Wing rib with an airfoil outline, spar opening, and two lightening holes.
- Sweep circular or rectangular profiles along a smooth generated 3D path.
- Sweep elliptical profiles along a smooth generated 3D path.
- Create a solid helical sweep for coils and springs, with editable wire radius, coil radius, pitch, and turn count.
- Create variable-pitch springs with separate start and end pitch values.
- Create tapered springs with a coil radius that changes along the spring.
- Create elliptical prisms and hollow elliptical tubes, plus circular and rectangle-to-circle loft transitions.
- Create hollow offset reducers from lofted inner and outer circular sections.
- Loft two offset elliptical sections into a tapered or eccentric solid.
- Pattern a body along any normalized 3D direction vector, with one-step undo/redo.
- Cut repeated through-all or blind holes around a bolt circle in one validated, undoable operation.
- Cut a rotated dovetail groove with separately controlled mouth and undercut widths, depth, and length.
- Cut a straight or rotated T-slot groove with independent neck, undercut, depth, and length controls.
- Cut a straight or rotated rounded slot with semicircular ends, adjustable width, overall length, and depth.
- Cut a rotated keyhole slot with a circular head, narrower throat, and adjustable depth.
- Cut a linear array of rounded slots in one validated, undoable operation.
- Cut circular bolt-circle arrays of countersunk through or blind holes with an editable included angle.
- Cut circular bolt-circle arrays of stepped counterbore holes.
- Cut linear arrays of through or blind counterbores along an editable 2D spacing vector.
- Cut linear arrays of through or blind countersinks with adjustable included angle.
- Cut a linear array of keyhole slots with adjustable head and throat dimensions.

## BOM, standards, and drawing status (v1.49)

| Area | Implemented and regression-verified | Still missing |
|---|---|---|
| BOM | Persistent part and purchasing fields, supplier grouping, BOM quality findings, CSV export, and schema-v5 storage; UI editing, reporting, save/reopen, and checks exercised. | Pricing, inventory, purchase orders, and ERP/MRP integration. |
| Parts list / assemblies | Per-body list, path-based assembly tree, reusable component IDs, fixed XYZ occurrences, bounded world-Z yaw for free occurrences, persistent rigid/revolute/prismatic joints, full-scene BOM grouping, joint guards, scene persistence, and XDE STEP assembly import/export. | General six-degree transform editor, joint solver/automatic assembly positioning, shared parametric definition edits, subassembly graph editor, motion/interference study. |
| Parametric foundation | XY rectangle, extrusion, through-hole with X/Y linear pattern or editable blind-hole depth; named dimensions, recorded constraints, chained scale-plus-offset dimension links editable through the UI, undo/redo, and stale-history protection. A 2D point/segment solver drives trapezoid, triangle, and vertex-defined polygon extrusions. Sketch constraints, boundaries, and depth persist; Edit can change dimensions and anchored vertices after reopening. | The UI cannot yet add or remove non-fixed geometric constraints or draw arcs/splines; expressions, a general feature graph, and arbitrary feature reorder/insert remain outstanding. |
| Sheet metal | One- and multi-bend flat-pattern calculations, per-bend rules, relief validation, SVG/DXF export, and editable faceted folded solids with save/reopen and undoable rebuild. | Exact analytic bend faces, flat/folded associativity, full feature tree, relief-to-fold integration, material libraries, springback/tool compensation. |
| Drawing checks / source link | Configurable project checks and persistent sheets with source IDs/revisions, stable view/annotation identities, view placements, notes, envelope dimensions, source-scoped BOM and balloons, detail/section views, SVG/PDF snapshots, export preflight, and stale-source preservation. | Automatically updating associative sheets, feature-linked dimensions, arbitrary section planes/paths, formal standards certification. |
| ISO/ASME checks | BOM/model readiness checks and an exported-SVG content screen; required fields and annotation conventions can be saved as per-project checklist defaults. | Clause-level ISO compliance, ISO 7200 title-block certification, ASME Y14/GD&T, tolerance, and datum validation. |
| 2D drawings | A4/A3 SVG and PDF export combines up to 50 selected bodies with first/third-angle layouts, orthographic and auxiliary views, title-block fields, source/revision identity, selectable groups, persistent view/note/overall-dimension/linked-parts-list/balloon/detail/section records, and geometry-linked cylindrical diameter callouts. The BOM table includes item, part number, description, quantity, material, per-unit mass, and revision. | User-attached dimensions on arbitrary edges/faces, automatic model-change updates, arbitrary section planes/paths, and formal standards validation. |

The Windows Release build and regression suite are recorded in [`docs/QA_MATRIX.md`](docs/QA_MATRIX.md) and [`docs/local_verification.json`](docs/local_verification.json). Detailed workflow limits are documented in [`BOM_PARTS_LIST.md`](BOM_PARTS_LIST.md), [`STANDARDS_CHECKS.md`](STANDARDS_CHECKS.md), [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md), and [`SHEET_METAL.md`](SHEET_METAL.md). Live UI verification was not part of this round. These implemented areas are useful workflows, not full Fusion parity.

Component templates are editable direct solids, not feature timelines or certified manufacturing designs. CADWorkbench does not claim feature parity with Fusion. See [`docs/FEATURE_GAP_COMPARISON.md`](docs/FEATURE_GAP_COMPARISON.md) for the current scope and known gaps.

## Selected geometry inspection

Use **Faces (inspection)** to measure one picked face area, **Edges (inspection)** to measure one picked edge length, or **Vertices (inspection)** to read a picked vertex's X/Y/Z coordinates. The Inspect ribbon includes a distance-between-two-vertices command. These queries are read-only and reject stale references after body geometry edits. Windows regressions cover all four tools. Live UI verified face area, edge length, and vertex coordinates; the distance command and its insufficient-selection guard are visible and verified, while the numeric two-pick result remains pending because the current desktop input cannot Ctrl-click a second subshape; manually Ctrl-click two opposite corners of the test box to verify the expected 53.852 mm diagonal. See [`docs/FACE_AREA.md`](docs/FACE_AREA.md), [`docs/EDGE_LENGTH.md`](docs/EDGE_LENGTH.md), [`docs/VERTEX_COORDINATES.md`](docs/VERTEX_COORDINATES.md), and [`docs/VERTEX_DISTANCE.md`](docs/VERTEX_DISTANCE.md).

## STEP import

Import STEP solids into the current scene as separate selectable bodies. Imports are validated transactionally and can be undone in one step. See [`docs/STEP_IMPORT.md`](docs/STEP_IMPORT.md) for coverage and current limitations.

## Profile sweeps

Create circular or rectangular solid sweeps with adjustable profile dimensions, path length, lateral offset, and vertical rise. Both commands are available in the Create menu and ribbon. See [`docs/PROFILE_SWEEP.md`](docs/PROFILE_SWEEP.md) for tested behavior and current limitations.

## Helical sweep

Create a spring-like solid from a circular profile using a specified wire radius, coil radius, pitch, and number of turns. The command is available from the Create menu and ribbon. Windows core regression, undo/redo, and live UI creation are verified; see [`docs/HELICAL_SWEEP.md`](docs/HELICAL_SWEEP.md).

## Variable-pitch spring

Create a helical solid whose pitch changes smoothly from the specified start pitch to end pitch. Windows Release build, focused geometry/history regression, full CTest, and live UI creation are verified; see [`docs/VARIABLE_PITCH_HELIX.md`](docs/VARIABLE_PITCH_HELIX.md).

## Elliptical sweep and tapered spring

Sweep an elliptical profile along the generated 3D path, or make a conical spring with independently set start and end coil radii. Windows Release build, core geometry/history regression, full CTest, and live UI creation are verified. See [`docs/ELLIPTICAL_SWEEP.md`](docs/ELLIPTICAL_SWEEP.md) and [`docs/TAPERED_HELIX.md`](docs/TAPERED_HELIX.md).

## Five profile and transition tools

Create elliptical prisms and tubes, circular lofts, rectangular-to-circular transitions, and hollow loft reducers. These direct solids are available from Create > Profiles. Windows build, 30 focused geometry/history assertions, full CTest, and live UI creation for all five tools are verified. See [`docs/FORM_FEATURES.md`](docs/FORM_FEATURES.md).

## Elliptical loft

Connect two elliptical sections with independently sized semi-axes, height, and top-section XY offset. Available from Create > Profiles in the menu and ribbon. The focused Windows geometry regression checks dimensions, bounds, volume, invalid-input rejection, and undo/redo. See [`docs/ELLIPTICAL_LOFT.md`](docs/ELLIPTICAL_LOFT.md) for current verification status.

## Nonuniform scale

Scale one selected body by independent X, Y, and Z multipliers about the world origin. The Modify menu and ribbon expose the operation; focused regression covers invalid factors, resulting bounds and volume, and undo/redo. See [`docs/NON_UNIFORM_SCALE.md`](docs/NON_UNIFORM_SCALE.md).

## Custom-axis circular pattern

Create rotated copies around an arbitrary 3D axis by entering an axis origin, direction, instance count, and total angle. The complete pattern is committed as one undoable operation. See [`docs/CUSTOM_AXIS_CIRCULAR_PATTERN.md`](docs/CUSTOM_AXIS_CIRCULAR_PATTERN.md) for validation and test status.

## Oriented sketch extrusion

Create supported sketch profiles on the XY, XZ, or YZ plane. Profile dimensions and origin use local U/V coordinates; signed W offsets the plane along its normal, and extrusion follows that normal or its reverse. Windows Release regression covers bounds and volume for every orientation, offset, and direction, plus topology, preview, commit, undo/redo, and invalid inputs. Live UI checks cover all three planes, including offset and reverse direction; XZ was also checked forward and reverse at W=0 and W=7 mm. See [`docs/ORIENTED_SKETCH_EXTRUSION.md`](docs/ORIENTED_SKETCH_EXTRUSION.md).

Sketch cuts can now use a Through All end condition, computed against the target body's bounds in the chosen orientation. The focused Windows regression covers both directions on all three planes. A live UI Cut through a 40 × 30 × 20 mm body reduced volume by 500 mm³ and passed Undo. See [`docs/THROUGH_ALL_EXTRUSION_CUT.md`](docs/THROUGH_ALL_EXTRUSION_CUT.md).

## Vector linear pattern

Create equally spaced copies along any 3D direction vector. The operation normalizes the direction, preserves body appearance, validates instance count and spacing, and records the copies as one undoable change. Windows regression and live UI creation are verified. See [`docs/VECTOR_PATTERN.md`](docs/VECTOR_PATTERN.md).

## Circular hole pattern

Cut a bolt-circle array of through-all or blind holes from a selected body. The command validates each cut and commits the entire pattern as one undoable operation. Windows core regression and live UI geometry change are verified; see [`docs/CIRCULAR_HOLE_PATTERN.md`](docs/CIRCULAR_HOLE_PATTERN.md).

## Dovetail groove

Cut a rotated dovetail groove into a selected body with configurable position, mouth and undercut widths, depth, and length. The Windows geometry regression passed 28 assertions, and the live workflow verified the visible cut plus undo/redo. See [`docs/DOVETAIL_GROOVE.md`](docs/DOVETAIL_GROOVE.md).

## T-slot groove

Cut a straight or rotated T-slot into a selected body with independent neck width, undercut width and height, total depth, and length. The focused Windows regression passed 28 assertions. Live UI verification on a 40 × 30 × 20 mm box measured a reduction from 24,000 to 23,135.040 mm³ and verified undo/redo. See [`docs/T_SLOT_GROOVE.md`](docs/T_SLOT_GROOVE.md).

## Rounded slot

Cut an obround pocket into a selected body, with semicircular ends and adjustable width, overall length, depth, and rotation. Windows Release regression and live UI verification passed, including undo/redo. See [`docs/ROUNDED_SLOT.md`](docs/ROUNDED_SLOT.md).

## Keyhole slot

Cut a circular head joined to a narrower straight throat into a selected body, with adjustable location, head diameter, throat width and length, depth, and rotation. Windows regression and live UI verification passed, including undo/redo. See [`docs/KEYHOLE_SLOT.md`](docs/KEYHOLE_SLOT.md).

## Linear rounded-slot pattern

Cut 2–100 equally spaced obround pockets along a 2D direction, with configurable first center, orientation, width, overall length, and depth. The Windows regression passed 29 assertions; focused slot-pattern CTests passed 5/5. Live UI verification on a 40 × 30 × 20 mm box produced three visible slots, changed volume from 24,000 to 21,655.221 mm³, and verified undo/redo. See [`docs/ROUNDED_SLOT_PATTERN.md`](docs/ROUNDED_SLOT_PATTERN.md).

## Circular countersink pattern

Cut a bolt-circle array of countersunk holes with configurable drill and head diameters, included angle, through-all or blind depth, instance count, and arc. The Windows regression passed 28 assertions, including partial failure rollback and blind-pattern undo/redo. Live UI checks verified four 90° through countersinks and three blind countersinks over a 180° arc. See [`docs/CIRCULAR_COUNTERSINK_PATTERN.md`](docs/CIRCULAR_COUNTERSINK_PATTERN.md).

## Circular counterbore pattern

Cut a bolt-circle array of stepped holes with independent drill depth, counterbore diameter and counterbore depth, plus count and arc controls. Focused Windows regression passed 21 assertions. Live UI verification created four recesses and a separate three-hole blind partial-arc pattern, with undo/redo confirmed. See [`docs/CIRCULAR_COUNTERBORE_PATTERN.md`](docs/CIRCULAR_COUNTERBORE_PATTERN.md).

## Linear counterbore pattern

Cut 2–100 counterbored holes along a 2D spacing vector. Choose through-all or blind drill depth and independently set the counterbore diameter and depth. Windows Release focused regression passed 23 assertions, including rollback on a missed later cutter. Live UI showed three stepped seats on a 40 × 30 × 20 mm box and verified undo/redo. See [`docs/LINEAR_COUNTERBORE_PATTERN.md`](docs/LINEAR_COUNTERBORE_PATTERN.md).

## Linear countersink pattern

Cut 2–100 countersunk holes along a 2D spacing vector, with configurable drill/head diameter, included angle, and through or blind depth. Windows Release focused regression passed 23 assertions. Live UI showed three tapered seats on a 40 × 30 × 20 mm box and verified undo/redo. See [`docs/LINEAR_COUNTERSINK_PATTERN.md`](docs/LINEAR_COUNTERSINK_PATTERN.md).

## Linear keyhole-slot pattern

Cut 2–100 keyhole profiles along a 2D spacing vector with configurable head, throat, depth, and orientation. Focused Windows regression passed 22 assertions, including rollback when a later slot misses the body; live UI verification created three cuts and confirmed undo/redo. See [`docs/LINEAR_KEYHOLE_PATTERN.md`](docs/LINEAR_KEYHOLE_PATTERN.md).

## Modeling tools

The Create menu and Create ribbon include 30 dimension-driven tools for structural sections, plates, flanges, pipe fittings, gears, and mechanical parts. Their feature list and per-tool verification status are in [`docs/MODELING_TOOLS.md`](docs/MODELING_TOOLS.md).

## Requirements

- Windows 10 or 11, 64-bit.
- Visual Studio 2022 or later with the **Desktop development with C++** workload and CMake tools.
- Git and PowerShell.
- vcpkg, bootstrapped for the current Windows environment.
- An internet connection for the first dependency install.

The project uses its checked-in `vcpkg.json` manifest to install Qt and OCCT dependencies. Do not include `build/` or `vcpkg_installed/` in a source upload; these are generated locally.

## Build on Windows

Open PowerShell in the extracted source folder. Install vcpkg once if it is not already available:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = 'C:\vcpkg'
```

If vcpkg already exists, skip the clone and bootstrap steps. Set the environment variable for each new PowerShell session, then build:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\build_windows.ps1
```

The script configures an x64 Release build, installs manifest dependencies, compiles the application and tests, stages Qt runtime plugins beside the executable, and runs CTest. The executable is `build\Release\CADWorkbench.exe`.

To build without running the full CTest suite:

```powershell
.\scripts\build_windows.ps1 -SkipTests
```

The Wing rib change has a focused regression mode:

```powershell
$env:PATH = "$PWD\vcpkg_installed\x64-windows\bin;$env:PATH"
.\build\Release\cadcore_tests.exe --wing-rib
```

Run the GUI from `build\Release` after the build script has staged Qt. Launching the executable from a different folder may fail to locate Qt platform plugins.

## Tests and review

Run the complete native test suite with:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

An optional independent geometry reference check uses CadQuery/OCP in a separate Python 3.12 environment. It does not modify the Windows or system Python installation:

```powershell
py -3.12 -m venv .venv-cadquery
.\.venv-cadquery\Scripts\python.exe -m pip install -r requirements-kernel-reference.txt
.\.venv-cadquery\Scripts\python.exe tests\kernel_contract_smoke.py
```

The reference check writes `docs/kernel_reference_v02.json`. Its result is supplemental; it does not replace the native C++ tests or GUI verification.

The app uses Open CASCADE for solid construction and validity checks. A passing build or core test suite does not by itself confirm every menu workflow or a clean-machine installation. See [`docs/QA_MATRIX.md`](docs/QA_MATRIX.md) and [`docs/manual_qa_results.md`](docs/manual_qa_results.md) for tested workflows and remaining release checks.

## Current local verification (2026-10-31)

The Windows Release application and test targets build. All 69 CTest targets pass, including editable feature suppression, document revision history, persistent surface bodies, surface construction, mesh cleanup, and repaired OBJ import. The 15 source-wiring contract checks pass. CadQuery 2.8.0 is installed in the ignored `.venv-cadquery` environment; its independent kernel smoke check passes six geometry and exchange checks.

The current working-tree follow-up broadens the v1.55 feature set across eight areas: ordered chamfer-plus-fillet history; solid-guided splitting of sewn multi-face shells; reusable component synchronization of geometry and BOM/material data; separate drawing geometry and BOM/material source revisions; undoable linear and world-Z circular component patterns; chained scale-plus-offset parametric links; drawing readiness requirements for section/detail views, linked parts lists, and BOM balloons; and editable parametric blind-hole depth. The Windows Release regression result is recorded in [`docs/QA_MATRIX.md`](docs/QA_MATRIX.md). The bounded sketch model, assembly solver, drawing system, and surface tools retain the limits listed in [`FEATURE_GAP_COMPARISON.md`](FEATURE_GAP_COMPARISON.md). These changes do not establish full Fusion 360 parity or live UI verification.

The ribbon now has a high-contrast light command surface, slate header, workspace picker, command search, and overflow menus for large tool families. The revised UI compiles, but visual confirmation of the new palette is still pending because the live computer-use session stopped before the final review. Earlier manual QA records describe earlier builds; drawing readiness checks are structural screens, not ISO/ASME certification.

Some individual manual checks remain: the body-schedule CSV save dialog has not been exercised through a save in the UI, and the numeric two-vertex measurement has not been verified live because the available desktop input could not Ctrl-click the second subshape. See [`docs/BODY_SCHEDULE_CSV.md`](docs/BODY_SCHEDULE_CSV.md) and [`docs/VERTEX_DISTANCE.md`](docs/VERTEX_DISTANCE.md).

## Upload the source to GitHub

1. Create an empty repository on GitHub. Do not initialize it with a README, license, or `.gitignore`, since this folder already contains those files.
2. In PowerShell, change to the extracted project source folder.
3. Initialize Git, create the first commit, and add the GitHub remote:

```powershell
git init
git add README.md LICENSE .gitignore CMakeLists.txt vcpkg.json src tests scripts docs .github
git commit -m "Prepare CADWorkbench source release"
git branch -M main
git remote add origin https://github.com/USERNAME/REPOSITORY.git
git push -u origin main
```

Replace `USERNAME` and `REPOSITORY` with the repository owner and name. GitHub may ask you to authenticate through its browser or credential manager.

For later updates:

```powershell
git add .
git commit -m "Describe the update"
git push
```

Before pushing, review `git status` and confirm generated build folders, dependency installs, personal files, and test models are not staged. The project `.gitignore` excludes common build outputs.

## License

Project source is licensed under the MIT License in [`LICENSE`](LICENSE). Qt and OCCT have their own licenses and redistribution terms; review them before distributing a compiled application.




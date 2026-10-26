# CADWorkbench

CADWorkbench is a native Windows 3D CAD application built with C++20, Qt 6, and Open CASCADE Technology (OCCT). It focuses on direct solid modeling and dimensioned engineering templates.

## Current capabilities

- Create and inspect solids, profiles, and dimensioned component templates.
- Edit models with Boolean operations, holes, patterns, mirror, shell, split, draft, fillet, chamfer, and transforms.
- Navigate with mouse orbit, pan, zoom, and standard camera views.
- Save and reopen native `.cadscene` documents; use undo and redo.
- Import and export multi-body STEP files; export STL.
- Create a dimensioned Wing rib with an airfoil outline, spar opening, and two lightening holes.
- Sweep circular or rectangular profiles along a smooth generated 3D path.
- Sweep elliptical profiles along a smooth generated 3D path.
- Create a solid helical sweep for coils and springs, with editable wire radius, coil radius, pitch, and turn count.
- Create variable-pitch springs with separate start and end pitch values.
- Create tapered springs with a coil radius that changes along the spring.
- Create elliptical prisms and hollow elliptical tubes, plus circular and rectangle-to-circle loft transitions.
- Create hollow offset reducers from lofted inner and outer circular sections.
- Pattern a body along any normalized 3D direction vector, with one-step undo/redo.
- Cut repeated through-all or blind holes around a bolt circle in one validated, undoable operation.
- Cut a rotated dovetail groove with separately controlled mouth and undercut widths, depth, and length.
- Cut a straight or rotated T-slot groove with independent neck, undercut, depth, and length controls.
- Cut a straight or rotated rounded slot with semicircular ends, adjustable width, overall length, and depth.
- Cut a rotated keyhole slot with a circular head, narrower throat, and adjustable depth.
- Cut a linear array of rounded slots in one validated, undoable operation.
- Cut circular bolt-circle arrays of countersunk through or blind holes with an editable included angle.
- Cut circular bolt-circle arrays of stepped counterbore holes.
- Cut a linear array of keyhole slots with adjustable head and throat dimensions.

Component templates are editable direct solids, not feature timelines or certified manufacturing designs. CADWorkbench does not claim feature parity with AutoCAD or Fusion 360. See [`docs/FEATURE_GAP_COMPARISON.md`](docs/FEATURE_GAP_COMPARISON.md) for the current scope and known gaps.

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

The app uses Open CASCADE for solid construction and validity checks. A passing build or core test suite does not by itself confirm every menu workflow or a clean-machine installation. See [`docs/QA_MATRIX.md`](docs/QA_MATRIX.md) and [`docs/manual_qa_results.md`](docs/manual_qa_results.md) for tested workflows and remaining release checks.

Current Windows verification snapshot (2026-10-26): Release build succeeded and CTest passed 18/18. Focused results for the recent pattern updates are recorded per feature. The source audit passed 136 checks and found regression references for all 94 public `SceneDocument` operations. Live UI workflows are sampled and documented with measured geometry, undo, and redo evidence; every command dialog has not been individually exercised. The W01-W24 release checklist, clean-VM deployment, and approver sign-off remain outstanding. See [`docs/local_verification.json`](docs/local_verification.json) for the structured summary and [`docs/QA_MATRIX.md`](docs/QA_MATRIX.md) for detailed results.

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




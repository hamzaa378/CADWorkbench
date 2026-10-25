# CADWorkbench

CADWorkbench is a native Windows 3D CAD application built with C++20, Qt 6, and Open CASCADE Technology (OCCT). It focuses on direct solid modeling and dimensioned engineering templates.

## Current capabilities

- Create and inspect solids, profiles, and dimensioned component templates.
- Edit models with Boolean operations, holes, patterns, mirror, shell, split, draft, fillet, chamfer, and transforms.
- Navigate with mouse orbit, pan, zoom, and standard camera views.
- Save and reopen native `.cadscene` documents; use undo and redo.
- Export STEP and STL files.
- Create a dimensioned Wing rib with an airfoil outline, spar opening, and two lightening holes.

Component templates are editable direct solids, not feature timelines or certified manufacturing designs. CADWorkbench does not claim feature parity with AutoCAD or Fusion 360. See [`docs/FEATURE_GAP_COMPARISON.md`](docs/FEATURE_GAP_COMPARISON.md) for the current scope and known gaps.

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



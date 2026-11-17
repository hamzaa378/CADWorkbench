# CADWorkbench v1.64 — verification record (source candidate)

## Starting baseline

User-confirmed native Windows Release result for **v1.63**: 83/83 CTests passed, executable SHA-256 `93E4833247F045ED067838E4FEBF56B707721575C6377A965A4321311188B47C`. The baseline source is packaged in `CADWorkbench_v1_63_Inertia_And_Assembly_Engineering_Source_20261105.zip`. Baseline source files are preserved except for explicitly documented integration changes.

## New code in this increment

- `WorkshopEngineering.h/.cpp` — Qt/OCCT-free section, process and tolerance engine.
- `WorkshopGeometry.h/.cpp` — OpenCascade B-rep cylindrical-surface inventory and validity report.
- `WorkshopPlanningDialog.h/.cpp` — new five-tab Qt engineering workbench.
- `workshop_engineering_v164_tests.cpp` — portable numeric regression suite.
- `workshop_geometry_v164_tests.cpp` — native OCCT geometry regression suite.
- `v164_workshop_preflight.py` — source integration guards.
- `MainWindow.h/.cpp` — one new Inspect menu action, read-only source-body association.
- `CMakeLists.txt` — new source registrations, four CTests, version 1.64.
- Release docs, version-specific expectation fixes, default expected Windows CTest count.

## Checks performed outside Windows

- Strict GCC C++20 compilation + numeric assertions: **1006/1006**.
- Strict Clang C++20 compilation + numeric assertions: **1006/1006**.
- Clang AddressSanitizer + UndefinedBehaviorSanitizer: **1006/1006**.
- Previously implemented portable engines compiled/run: **302** mass inertia, **135** parametric expressions/graph, **135** guided study, **554** engineering calculation checks.
- Existing source-contract suite: **32** checks passed.
- Source-preflight suite: **41** checks passed (before the last docs-only update).
- v1.64 focused source-preflight suite: **7** checks passed.
- CMake parser passed through the project declaration; Linux intentionally stops at the project's Windows-only guard. This does NOT establish a successful Windows CMake configure.

## Not yet independently verified

- Windows MSVC 19.5x Release C++ compile and Qt MOC.
- OpenCascade v8.0.1 `WorkshopGeometry` compile and B-rep integration test.
- Windows GUI launch, menu handling, copying candidate cylinders, and export.
- Full **87/87** CTests.

Do not mark v1.64 as verified until `verify_release_windows.ps1 -ExpectedTests 87` succeeds, reports all tests passing, and produces an executable SHA-256 and JUnit report. Keep v1.63 as the last confirmed passing baseline.

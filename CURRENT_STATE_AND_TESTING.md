> **CURRENT SOURCE AND VERIFICATION, 2026-11-17:** Start with [`README.md`](README.md) and [`LATEST_VERIFIED_BASELINE.md`](LATEST_VERIFIED_BASELINE.md). Sprint 11 builds on Windows, 130/130 native CTests and 952/952 Python/source checks passed. Manual engineering acceptance remains 0/60. The historical notes below have been superseded for current build status.

---

> **MSVC status update, 2026-11-06:** Windows Visual Studio 2026 successfully compiled and linked `build\Release\CADWorkbench.exe` on the previous Round 2 source. The user-supplied full CTest log reported **124/130 passed** and six failed. The Fix 3 source in this package addresses those six cases, but has **not been compiled or run on Windows**. GCC/Clang portable tests and Python/source checks are distinct evidence; they do not replace MSVC acceptance. See `docs/V170_ROUND3_SIX_NATIVE_TEST_FIXES.md` and run `RUN_PREVIOUS_FAILURES.ps1` followed by `RUN_WINDOWS_TESTS.ps1`.

# CADWorkbench v1.70 — complete current-state source and test instructions

This is the full consolidated CADWorkbench source from the latest **Integrated Engineering Source** ZIP, plus the **Drawing Associativity and Tolerance Stack** patch (already applied). It is NOT an executable release and NOT a verified Windows build.

## What is included

- Full C++/Qt/OpenCascade project, assets, CMakeLists.txt and vcpkg.json
- All scripts and test source from the current state
- Bounded full-circle drawing reference recovery and guided tolerance stack from the latest patch
- SHA-256 file manifests (SHA256SUMS.txt and SOURCE_MANIFEST files)
- RUN_WINDOWS_TESTS.ps1: hash validation, native MSVC build, 130-test CTest run and JUnit report
- docs/V170_IMPLEMENTATION_STATUS.md: the 50-target acceptance ledger

## One command to build and test (Windows PowerShell)

Open PowerShell **inside the extracted CADWorkbench_v170_Integrated folder**:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

Prerequisites: Windows 10/11, Visual Studio 2022 C++ desktop development workload, CMake, bootstrapped vcpkg at C:\vcpkg (or specify a different path). vcpkg.json lists qtbase, qtsvg and opencascade. Required dependencies may be installed during CMake configuration.

If the build and all tests pass, run the interactive executable:

```powershell
.\build\Release\CADWorkbench.exe
```

The CTest evidence is written to `build/cadworkbench_release_ctest.xml`. A successful exit from this script confirms the native build and 130 CTests in your environment, **not** that the GUI or all 50 requirements have passed acceptance testing. Check new, open, save/reopen, feature edits, assembly editing/refresh, drawing regeneration and undo/redo manually.

### Portable source checks (optional, on Linux/macOS with GCC/Clang)

```bash
./scripts/verify_portable_consolidation.sh g++
./scripts/verify_portable_consolidation.sh clang++
```

These do not compile the complete Windows Qt application.

## Current release gaps

The authoritative acceptance ledger currently counts 1 target implemented within defined scope, 44 partial, 5 outstanding. **The 5 outstanding:** #29 exploded sequences and #37–40 production CAM (drilling, pocket/contour, stock/fixture verification, post-processing). All partial targets remain subject to their original acceptance criteria.

The most important broad partial requirements: general topological naming (#1), feature graph and multi-body rebuild (#2–3, #11), arbitrary sketch-driven revolve/sweep/loft and sketch constraints (#4–10), general interactive assembly authoring (#22–28), sheet-metal workflows (#31–33), drawing associativity/GD&T (#41–45), and release CI/UI acceptance (#50).

The ledger contains historical descriptions that may predate later bounded increments. A partial status does not mean no implementation exists; nor can it be upgraded to 'complete' without end-to-end native evidence. Preserve the last known Windows-verified executable.

## Local packaging checks (source environment)

- Patch-source SHA-256 manifest was verified before packaging.
- Source: current full Integrated ZIP plus the most recent patch; no dependency on an earlier ZIP after extraction.
- Python/independent-geometry unittest suite: 321 passed in the current work session.
- Native Windows compilation and 130 CTests: not executed in this environment.

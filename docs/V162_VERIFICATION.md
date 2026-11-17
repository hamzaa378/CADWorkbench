# v1.62 verification evidence — 2026-11-05

## Verified in this source-processing environment

- **Previous native Windows baseline**: user's log shows v1.60 Release compiled and 78/78 CTests passed. This is historical baseline evidence only, not v1.61/v1.62 validation.
- **Portable expression/configuration/material/graph engine**: GCC C++20 `-Wall -Wextra -Wconversion -Werror -pedantic`, 135/135 checks passed. Clang C++20 equivalent 135/135. Clang ASan+UBSan 135/135.
- **Guided engineering study engine**: GCC C++20 strict 135/135. Clang strict 135/135. Clang ASan+UBSan 135/135.
- **Unmodified previous engineering design tools**: GCC standalone 554/554 tests passed over 50 existing calculation modes.
- **Source-contract suites**: twelve (seven `*contract.py` and five `*preflight.py`), all passed individually. The new v1.62 preflight covers CMake registration and Qt command wiring. Static source checks are not native C++ compilation.
- **Pygments C++ lexical delimiters**: all new headers, implementation/test files and four affected UI functions scanned; no unmatched delimiter or lexer error found. This is not a compiler check.
- **CMake parse**: `cmake -S ... -B ...` advanced to the repository's intentional `if (NOT WIN32) message(FATAL_ERROR ...)` guard; exit 1 is expected on Linux and cannot verify Windows configure.

## Still not verified

- Windows/MSVC compile of new Qt dialog code and all CMake targets.
- Qt GUI launch and interactive mouse operations.
- 81/81 native Windows CTests for this candidate.
- Save/reopen of configurations in the CAD document (intentionally unsupported; sidecar `.cadcfg` only).
- Actual B-rep selective regeneration using the new planner (not connected to the production executor).
- Full 50-feature roadmap (not delivered by v1.62).

## Windows acceptance command

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 81
```

The acceptance script fails on CMake errors, MSVC build errors, unexpected or duplicate tests, or any failing CTest. It generates an executable SHA-256 fingerprint and a JUnit XML report on success. **Do not label v1.62 build-verified until the full command succeeds on Windows**.

## Native smoke test after CTest

Create a parametric body, evaluate and apply width/height formulas, undo/redo and reopen; compare and load/save `.cadcfg` sidecars; run a guided bracket, shaft and weld study; export the design review, confirm body/part/material state did not change; confirm unmodified v1.60 geometry and assembly workflows still run.

# CADWorkbench v1.58 — Windows MSVC test-fixture correction

## Source of the error

The 2026-11-04 Windows build log shows the `cadcore` library and `CADWorkbench.exe` compiled successfully, but the `cadcore_tests.cpp` target failed at line 335 and related locations with MSVC C2440. The test fixture declared `cad::SketchPlane`, an independent datum-frame structure, while initialized and assigned it from values of `cad::ExtrusionSketchPlane` (an enum controlling legacy extrusion orientation).

## Changes

- `tests/cadcore_tests.cpp`: `PlaneCase::plane` and `ThroughAllCase::plane` are now `cad::ExtrusionSketchPlane`; all existing test cases retain their original XY/XZ/YZ expectations. These are the ONLY replacements needed in the test target; preserving `cad::SketchPlane` in new datum-plane tests is intentional.
- `tests/v158_source_preflight.py`: added regression checks for both fixture type declarations and the absence of the incorrect datum-plane fixture.
- No production geometry code or behavior was changed in this correction.

## Windows acceptance commands

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
.\scripts\build_windows.ps1
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
```

The full native Windows compilation and CTest result are **not** established by static checks. Retain the previously passing v1.56 baseline until v1.58 passes. The original log included non-fatal warnings about OpenCascade and Qt deprecations and C4702 unreachable code, which are not addressed by this targeted correction.

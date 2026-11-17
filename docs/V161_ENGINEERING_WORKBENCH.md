# CADWorkbench v1.61 — Preliminary Engineering Workbench

The first v1.70 foundation increment adds a searchable, unit-labelled engineering calculation workspace, accessible from **Inspect → Engineering workbench: 50 preliminary calculations**.

**Scope:** This is *one integrated engineering subsystem with 50 calculation modes*, **not 50 major parametric CAD features**. It does not replace CAD geometry, certified calculations, structural FEA, or standards-specific engineering verification.

## What runs

- 50 independent mathematical operations in five categories, using a Qt-independent C++20 engine.
- A searchable Qt dialog with per-tool inputs, units, a worked default, assumptions and numerical result.
- If exactly one body is selected, the dialog displays its true bounding XYZ dimensions and B-rep volume. The user may explicitly populate width, height, length or assigned density. It does **not** infer structural section properties, actual wall thickness, hole diameter, material strength or load cases from a bounding box.
- Copy or export a text report with exact input units, calculation result, assumptions and an engineering verification warning.
- All inputs reject NaN, infinity, out-of-range values, invalid tube geometry and nonintegral fastener counts. Operations reject nonfinite results.
- All 50 formulas have numerical golden cases and negative-input checks in `tests/engineering_design_tools_tests.cpp`, registered as `cadcore_engineering_preliminary_design`.

## Real engineering examples

1. **Shaft preliminary sizing:** Use tube/solid shaft torsional shear and section moment calculations; compare to separately verified material allowable stress. Do not assume these cover stress concentration, fatigue or keyways.
2. **Simple bracket or beam check:** Use rectangle inertia, beam deflections, bending stress and yield safety factor for an explicitly specified idealized load case. Do not assume a real joint behaves as a perfect support.
3. **Bolted plate:** Use ISO approximate tensile area, bolt tensile stress, nominal direct shear, torque-preload estimate and fit clearance; check friction, vibration and fastener standards separately.
4. **Fabrication planning:** Use sheet bend allowance/deduction and drill/turn/mill machining feed time estimates. Confirm bend tooling, process limits and non-cutting times.
5. **Thermal fit:** Use signed thermal expansion, ideal restrained thermal stress, and minimum/maximum fit clearances. Verify temperature gradients and plasticity separately.

## Unit convention

Dimensions are millimetres, force newtons, stress MPa (N/mm²), modulus GPa, density kg/m³, temperature difference kelvin, torque N·mm except the preload torque output in N·m, feed and cutting speed explicitly labelled. A separate label is shown on every field. **Distributed load is N/mm, not N/m.**

## Verification evidence

- Baseline: user-supplied MSVC Release v1.60 build and **78/78 CTest PASS** on November 4, 2026.
- v1.61: independent Linux `g++ -std=c++20 -Wall -Wextra -Werror -pedantic` compilation and execution of the new pure C++ module and its regression executable: **554 checks passed**.
- v1.61: legacy Python source-contract checks execute separately; see the actual per-suite results in `docs/V161_VERIFICATION.md`.
- v1.61 Windows Release application build, Qt GUI click testing and full CTest suite: **NOT VERIFIED HERE**. CMake registration predicts **79 native CTests**. Compile the ZIP on Windows before calling v1.61 a stable baseline.

## Building

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
PowerShell.exe -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
```

The workbench is a preliminary-design convenience. Never use its estimates as sole evidence for safety-critical or code-regulated engineering.

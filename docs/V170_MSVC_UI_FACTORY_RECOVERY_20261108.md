# CADWorkbench v1.70: MSVC joint-definition editor compile fix

Date: 2026-11-08. This is a **preliminary source-only** follow-up to the combined three-workstream overhaul. The previous source archive is preserved.

## Grounded Windows build evidence

The uploaded PowerShell build output shows successful CMake generation, successful compilation of `src/core/SceneDocument.cpp`, and successful production of `build/Release/cadcore.lib`. The native **CADWorkbench GUI** target then failed at `src/ui/MainWindow.cpp:2761` with MSVC errors `C3535` and `C2440`, followed by failed `make(...)` calls (`C2064`) and dependent initialization errors (`C3536`, `C2737`). The report did **not** reach native CTest execution.

## Correction

In `MainWindow::editSavedArticulatedJointDefinition`, the lambda factory for dialog `QDoubleSpinBox` controls was incorrectly declared as `auto* make=[&](...)`. A lambda expression is a closure object, not a pointer, so the factory variable must be `auto make=[&](...)`. Returned controls remain `auto* ax`, `auto* yawBox` etc. Only the invalid factory type was modified; the joint editor's validation, preview, and commit semantics were preserved.

New `tests/v170_msvc_joint_dialog_compile_contract.py` enforces the corrected declaration and scans C++ source for `auto*` lambda declarations to catch this error class in source-level CI. The actual lambda body was also extracted into a minimal Qt-type stub and passed a `clang-cl /std:c++20 /Zs /W4 /WX` syntax check in MSVC-compatible mode. This is **not** an actual Windows MSVC/Qt integration compile.

## Verification for this package

- 479/479 Python tests passed (including source-contract and independent geometry reference cases).
- 16/16 GCC portable C++ test targets passed (without sanitizers).
- 16/16 Clang portable C++ targets passed with address/undefined-behavior sanitizers.
- Actual joint-editor lambda factory syntax passed under `clang-cl` in MSVC-compatible C++20 mode, using minimal Qt interface stubs.
- Windows MSVC production app compilation, 130 native CTests, native face-machining history regression, and formal 60-scenario acceptance are **not yet rerun for this corrected archive**.

## Next Windows command

From the newly extracted root, in PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

If another compiler error emerges, report the **first new** `error Cxxxx` or linker `error LNKxxxx` line and adjacent context. Do not treat 0/60 recorded scenarios as a 0% feature-failure rate when the build has blocked all tests. The provisional development estimates remain parametric 76%, assembly 83%, drawing/sheet metal 68%; these are not scored acceptance results.

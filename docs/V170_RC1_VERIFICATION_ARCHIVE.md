# CADWorkbench v1.70 consolidated engineering RC1 — verification (2026-11-06)

**Source candidate. Not a Windows-verified release; Priority 1 and 2 are not fully completed.**

Baseline: `CADWorkbench_v170_ToroidalSpline_AssociativeManufacturing_20261106.zip`, expanded without altering the last verified executable.

## Checks performed in the Linux working environment

- Python unit/source-preflight and independent OpenCascade references: **280 tests passed** (`python -m unittest discover -s tests -p '*.py' -q`).
- GCC (C++20, address/undefined-behavior sanitizers, `-O0 -g0`): **359** source-revision/feature-graph checks, **41** Boolean ancestry checks, **80** linked-body graph checks, **21** rigid-network checks, **27** articulated-network checks passed.
- Clang (same portable suite and sanitizer settings): **359**, **41**, **80**, **21**, **27** checks passed respectively.
- CMakeLists.txt still registers **130 native CTest tests**. The current environment's `cmake -S .` stops at the intentional Windows-only guard in line 10; this is NOT evidence that the Windows C++ application builds.
- **Not performed:** compilation of `SceneDocument.cpp`, `DrawingSheet.cpp`, Qt UI source, OpenCascade C++ integration, Windows MSVC Release, native CTests, GUI smoke, or Windows end-to-end persistence validation. New native regression scenarios are written but unexecuted.

### Reproduce portable checks

```bash
./scripts/verify_portable_consolidation.sh g++
CAD_SKIP_PYTHON=1 ./scripts/verify_portable_consolidation.sh clang++
```

### Required Windows release gate

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Keep the last user-confirmed passing Windows executable. Do not promote this source to a verified v1.70 release until native Windows checks and outstanding roadmap acceptance scenarios are complete. Review `V170_CONSOLIDATED_ENGINEERING_RC1.md` for remaining scope and compatibility limitations.

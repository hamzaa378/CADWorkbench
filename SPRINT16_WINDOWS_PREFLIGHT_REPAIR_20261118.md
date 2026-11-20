# Sprint 16 Windows Python preflight compatibility repair (2026-11-18)

This is a complete Sprint 16 source update. **No Sprint 15 repair/build is required.**

## Failure observed on Windows

On a Python 3.14 / Windows PowerShell 5.1 host, the eight Sprint 16 integrated-readiness tests previously failed before CMake/MSVC compilation:

* `UnicodeDecodeError: cp1252` reading `src/ui/MainWindow.cpp`, which is UTF-8;
* `g++.exe: unrecognized command line option '-std=c++20'` from a legacy g++ earlier on PATH.

These four failures were Python preflight/test-environment issues; they do not establish whether Sprint 16 native code compiles with MSVC.

## Corrective changes

1. Sprint 16 source contract tests now explicitly read/write UTF-8 instead of relying on locale defaults.
2. The Windows launcher and full audit enable Python UTF-8 mode from startup.
3. Portable regression tests probe each detected `g++` and `clang++` for actual C++20 support, trying Clang if GCC is too old; if neither qualifies, the two optional portable compilation tests clearly skip. The **mandatory MSVC build, native CTests and complete Python regression suite are not skipped**.
4. Portable test executables use `.exe` naming on Windows.
5. Three Python regression tests cover legacy compiler rejection, alternate compiler selection, and explicitly encoded source reads.

## Test and release status

Run only `RUN_SPRINT16_WINDOWS_TARGETED_BUILD.ps1 -FullAudit` in the extracted corrected Sprint 16 tree. The script must build `CADWorkbench.exe` and run 130 native CTests; those native results cannot be certified from Linux. A full automated pass may return exit code 3 while the 60 engineering acceptance scenarios remain outstanding.

Even if no portable g++/clang++ is installed, the required native Windows MSVC checks still run.

The independent diagnostic parser requires at least **1,049 source tests** to prevent the earlier 1,030-test floor from silently accepting an incomplete Sprint 16 inventory. `unittest` may report two skipped optional portable compilation tests on systems without a recent GCC/Clang; those skipped cases still count as discovered, and the Windows native gate is mandatory.

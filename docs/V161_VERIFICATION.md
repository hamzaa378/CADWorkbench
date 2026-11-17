# CADWorkbench v1.61 — Test and provenance record

## Previous verified baseline

The user supplied an actual Windows Visual Studio 18 Release build log for **v1.60** ending in **100% tests passed, 0 failed out of 78** (4 November 2026). This result belongs to the unmodified v1.60 baseline and **does not certify this modified v1.61 source**.

## Work completed in this environment

- Added a Qt-independent C++20 engineering calculation engine with **50 catalogued modes** in five groups.
- Compiled and ran its executable tests with Linux **GCC** using `-std=c++20 -Wall -Wextra -Werror -pedantic`: **554/554 checks passed**.
- Compiled and ran the same executable with **Clang** using equivalent strict flags: **554/554 checks passed**.
- Compiled and ran with GCC **AddressSanitizer and UndefinedBehaviorSanitizer**: **554/554 checks passed**.
- Legacy source-contract suites were run separately. Totals: v1.56 sketch 4, dependency v1.57 7, development audit 6, drawing export 3, drawing persistence 4, drawing revision 5, parameter editor 3, v1.58 preflight 6, v1.59 preflight 8, v1.60 preflight 7; new v1.61 preflight 4. **57/57 Python contract checks passed.**
- CMake syntax parsing reached the intentional Linux guard at `CMakeLists.txt:10` (`CADWorkbench targets Windows ...`), not a syntax error; platform-specific configure is unverified.
- Package integrity and source hashes must be checked during packaging; see `SOURCE_MANIFEST.sha256`.

## Unverified release gates — do not mark passed

- Windows MSVC Release compilation of the Qt dialog and the entire application.
- Native OpenCascade/Qt integration and all **79 registered CTests**, including `cadcore_engineering_preliminary_design`.
- Manual Qt GUI interaction: filter tools, calculate, selected-body context, report clipboard/export, high-DPI layout.
- Production engineering certification, applicable standards or FEA accuracy; these are strictly preliminary formulas.

## Commands to validate on Windows

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
PowerShell.exe -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
ctest --test-dir .\build -C Release -R '^cadcore_engineering_preliminary_design$' --output-on-failure
```

If the new test or UI build fails, keep the user-confirmed v1.60 source as the stable baseline. Avoid layering v1.70 feature work on an unverified v1.61 candidate.

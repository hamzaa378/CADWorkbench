# v1.70 ordered stack — verification record

Source: previously distributed 119-test `CADWorkbench_v1_70_Two_Stage_InPlace_And_Advanced_Mates_Source_20261106.zip`.

## Completed in this development environment

- `g++ -std=c++20 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror`: standalone linked-body graph/impact C++ regression passed **80 checks**.
- `clang++ -std=c++20 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror`: same **80 checks passed**.
- GCC AddressSanitizer plus UndefinedBehaviorSanitizer on the graph tests: **80 checks passed**.
- Python source preflight discovery `*preflight.py`: **118 assertions passed**.
- Python source contract discovery `*contract.py`: **32 assertions passed**.
- Independent OpenCascade/CadQuery reference regressions: 4 existing three-foundation cases, 8 boss cases, earlier pocket cases, and 2 new ordered-stack cases passed. These are oracle checks, **not native C++ execution**.
- CMake configuration parsed until the expected Windows-only guard on Linux. This is **not** a successful Windows CMake configuration.
- Native CTest registration checked by source analysis: **123 distinct names**.

## Not verified in this environment

- Full native C++ SceneDocument compilation and link with Qt and OpenCascade.
- Windows Release application build, `windeployqt`, GUI interactions and 123 native CTests.
- Windows-specific packaging/deployment behavior, and Windows executable hash.

## Windows acceptance (required)

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 123
```

Do not promote this development candidate over a confirmed passing Windows source until all registered native tests pass. This package does not represent completion of the full 50-feature v1.70 roadmap.

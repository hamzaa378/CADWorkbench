# v1.66 verification record

## Native Windows status

**Not run here.** This environment does not have Windows, MSVC, Qt6 Windows headers or OpenCascade 8.0.1 Windows libraries. The user has confirmed the starting v1.65 Release passed 88/88 CTests. The new v1.66 candidate registers **90 unique CTests**, including two new tests:

- `cadcore_v166_sparse_dependency_planner`
- `cadcore_v166_sparse_feature_regeneration`

Only the Windows Release build and full CTest result can establish v1.66 acceptance.

## Checks executed here

- New dependency-planning C++20 executable: **347 assertions passed, GCC, `-Wall -Wextra -Wpedantic -Werror`**.
- Same executable: **347 assertions passed, Clang, same warnings-as-errors flags**.
- Clang **AddressSanitizer + UndefinedBehaviorSanitizer: 347 assertions passed**.
- Python source preflight and legacy contracts: **16 suites, 81 unittest cases passed**. The older v1.58 string-lexer scan was changed from repeatedly slicing the entire remaining C++ file to using a bounded match at the current character, avoiding an O(n²) scan; no assertions were removed.
- Independent Python/OpenCascade geometry checks: **14** from the v1.65 geometry reference suite, **2** inertia reference cases and **3** edge-anchor reference cases passed.
- `kernel_contract_smoke.py`: six reference categories passed (primitive volumes, topology, transforms, Booleans, export/import, etc.). These are *Python/OCP reference checks*, not native C++ tests.
- CMake was parsed on Linux up to the intentional Windows-only guard. This is not equivalent to Windows CMake generation or an MSVC build.
- Source ZIP and per-file hashes verified after packaging. Git-compatible incremental patch tested on a fresh v1.65 archive extraction and compared byte-for-byte to v1.66. (See package verification log from release generation.)

## Areas requiring Windows acceptance

- Qt/OCCT compilation of the new `ShapeFeatureTree` and `SceneDocument` integration.
- Actual B-rep shared-handle reuse and independent branch-identity assertions.
- Whole-suite regression for the preceding 88 native tests, including interactive sketch and linked-body workflows.
- GUI's planned rebuild count and actual stage/commit behavior.

## Acceptance command

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 90
```

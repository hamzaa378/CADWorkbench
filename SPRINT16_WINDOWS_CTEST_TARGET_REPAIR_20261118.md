# Sprint 16 Windows CTest executable repair — 2026-11-18

## Observed Windows results

The uploaded `CADWorkbench_Sprint16_New_Full_Audit.log` shows:

- Source/hash and structural preflight: PASS.
- Python Sprint 16 focused preflight: 11 tests, two optional portable C++ compiler checks skipped on Windows, PASS.
- Visual Studio 2026 MSVC: `cadcore.lib` and `CADWorkbench.exe` built successfully.
- Focused CTest selection: 3 of 4 passed. `cadcore_v170_articulated_closed_loop` was **NOT_RUN**, not a failing assertion, because `build/Release/articulated_assembly_v170_tests.exe` did not exist.
- Full 130-native-CTest audit was not reached. No 130/130 PASS claim is justified.

## Root cause and fix

`CMakeLists.txt` registers `cadcore_v170_articulated_closed_loop` with the `articulated_assembly_v170_tests` executable, but `RUN_SPRINT16_WINDOWS_TARGETED_BUILD.ps1` built `articulated_assembly_scene_v170_tests` only. These are **different** executables. Add `articulated_assembly_v170_tests` to the targeted compile list **before** focused CTest.

Added `test_sprint16_targeted_native_build_covers_all_selected_ctests` to cross-check the four focused CTest registration commands against the executable names built by the launcher. Updated the independent full-audit diagnostic's expected Python suite count from 1049 to 1050 after adding this test.

No core CAD behavior or Qt/OCCT source code was altered in this repair. The Windows outcome of this new source still requires MSVC/CTest execution.

## Fastest way to verify while preserving the existing Windows build

Extract this new combined ZIP to a separate directory. From a new PowerShell window run:

```powershell
Set-Location "$env:USERPROFILE\Downloads"
$package = 'CADWorkbench_v170_Sprint16_CTestTargetCorrected_GitHub_READY_20261118.zip'
$extract = 'CADWorkbench_Sprint16_Repair_Source'
Expand-Archive -LiteralPath $package -DestinationPath $extract -Force
$existing = "$env:USERPROFILE\Downloads\CADWorkbench_Sprint16_New_Test_20261118_151507\CADWorkbench"
& ".\$extract\CADWorkbench\APPLY_SPRINT16_TARGET_FIX_TO_EXISTING_BUILD.ps1" -ExistingRoot $existing
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Repair failed' }
Set-Location $existing
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\RUN_SPRINT16_WINDOWS_TARGETED_BUILD.ps1' -VcpkgRoot 'C:\vcpkg' -Parallel 4 -FullAudit
```

The fast-path helper verifies the original source hashes, backs up every modified source, copies only changed tracked files and the two manifests, and leaves `build/` untouched for an incremental native build. Use the clean-ZIP extraction method instead if the existing source was changed manually or if you prefer to reconfigure from scratch.

The original 130 native CTest registrations are unchanged. Manual engineering acceptance remains NOT_RUN until separately exercised; automated PASS is not manufacturing approval.

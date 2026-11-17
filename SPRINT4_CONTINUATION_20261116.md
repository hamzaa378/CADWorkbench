# CADWorkbench v1.70 — three backend coding passes (Sprint 4, 2026-11-16)

**STATUS: PRELIMINARY SOURCE ONLY. DO NOT REPLACE THE VERIFIED WINDOWS BASELINE.**

This candidate extends the Sprint 3 combined source ZIP without removing any original source files. The user's last actual Windows transcript for Sprint 2 built the app and ran **129/130 native tests**, failing `cadcore_universal_feature_tree` (grouped unequal-size two-edge blend, check 70). Sprint 3 carried a source fix, but that source fix remains **NOT VERIFIED BY NATIVE WINDOWS CTEST**. No new MSVC/Qt/OpenCascade build or real native CTest was attempted for this Sprint 4 development candidate, as requested. The prior Windows 130/130 verified baseline is separate. Manual P/A/D acceptance remains 0/20 each.

## Coding pass A — foundational correctness and topology

- Corrected `src/core/BranchReferenceAgreement.h`: replaced `sum(delta*delta) <= tolerance*tolerance` with scaled `std::hypot` to prevent subnormal underflow and giant-value overflow when deciding whether two B-rep branch witnesses share a physical location. Added finite-coordinate rejection.
- `src/core/AssemblyToleranceBudget.h`: `std::midpoint` prevents overflow in the midpoint of large but individually finite fit limits.
- `src/core/WorkshopEngineering.cpp`: signed deviation center computation uses `std::midpoint`, avoiding unnecessary overflow when both individual deviations are finite.
- Compiled `tests/geometry_numeric_stability_portable_tests.cpp` under GCC and Clang. 16,010 focused assertions, including very small witness tolerances.

## Coding pass B — shop-constrained manufacturing fit planning

- Added `src/core/ShopToleranceAllocation.h` as a backend C++20 API. Supports per-dimension manufacturing minimum scale, relative tightening weights, asymmetric deviations and both +/− chain directions.
- Uses deterministic cost-weighted greedy allocation, validates results through the existing guaranteed worst-case fit budget, and falls back to the explicitly entered shop floor when the heuristic stalls or rounding would otherwise leave a false feasible claim.
- If even the floor cannot fit, reports it clearly rather than inventing a feasible plan. Every planned change retains a per-term audit trail, and invalid requests do not mutate the previous output.
- `tests/shop_tolerance_allocation_portable_tests.cpp`: 33,613 passed assertions including 1,600 deterministic randomized four-term chains. **The allocation heuristic is not a proven cost optimum, and entered manufacturing capability is not verified supplier capability.**

## Coding pass C — between-pose assembly clearance

- Added `src/core/MotionSweptClearance.h`, a continuous **translation-only** swept-AABB analysis: exact AABB contact interval under constant dimensions and linear translation, approximate minimum AABB gap with golden-section search, and a multi-sample tracker recording risky segments and first closest-approach time.
- Catches components crossing *between* saved poses even when both endpoints are clear. Rejects malformed boxes, reordering or duplication of bodies, out-of-order or overflowing timestamps, shape extent changes and invalid clearance without overwriting previous history.
- `tests/motion_swept_clearance_portable_tests.cpp`: 30,182 passed assertions, randomized interpolation witness checks and transactional failure tests.
- **Not** exact B-rep collision, rotating-body CCD, motion solver integration or a production clearance certification.

## Full code-tree audit and extended tests

- Added `tools/audit_full_source.py`: scans 388 C++/Python/PowerShell/CMake code files totaling roughly 82,900 source lines, finds malformed encodings, NUL, merge markers, case-insensitive Windows path collisions, missing CMake file references, test registry errors, missing new core APIs and unexpected changes to the 130 native CTest registration contract. Static scan: **PASS**; no failed checks. It does **not** semantically validate every function.
- Added 18 Python checks in `tests/test_sprint4_engineering_contract.py`; previous 812 Python tests became **830/830 PASS**.
- Added 3 genuine C++ executable regressions to `tools/run_portable_regressions.py`; **33/33** independent compiled programs passed on both GCC and Clang (previously 30/30).
- CMake `cadcore` registers the new header-only services and `BranchReferenceAgreement.h`; it keeps all 130 original native test names intact. CI now includes the static full-tree audit.
- `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1` now requires at least **830** Python tests in native evidence (previously 812). Old source ZIP remains unmodified and available as the rollback candidate.
- Preserved `/bigobj` on the MSVC application target, and Sprint 3 grouped-edge-fillet/chamfer source correction.

## Local evidence for this source (Linux container)

- `qa/sprint4_python_830.log`: 830 Python/source/OCP reference-kernel tests PASS.
- `qa/sprint4_gcc_33.log`: 33 C++ portability executables compiled and PASS using GCC.
- `qa/sprint4_clang_33.log`: 33 C++ portability executables compiled and PASS using Clang.
- `qa/sprint4_source_audit.json`: full-tree source-integrity scan PASS.
- `qa/SPRINT4_CHANGED_FILES.json`: exact file-by-file diff against original Sprint 3 input ZIP. Zero Sprint 3 source removals.
- A *portable* compiled test is not an MSVC+Qt+OCCT test. A Python test fixture printing "Native 130/130" is never evidence of native CTests.

## Windows full build AFTER coding passes — not performed in this source work

Use a **new, empty** extraction directory in Downloads. From a fresh PowerShell, paste:

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$archive = Join-Path $downloads 'CADWorkbench_v170_BackendSprint4_ThreePasses_COMBINED_PRELIMINARY_20261116.zip'
$destination = Join-Path $downloads 'CADWorkbench_Sprint4_Independent_Windows'
if (!(Test-Path -LiteralPath $archive)) { throw "Missing ZIP: $archive" }
if (Test-Path -LiteralPath $destination) { throw "Choose a new destination: $destination" }
Expand-Archive -LiteralPath $archive -DestinationPath $destination
$root = Join-Path $destination 'CADWorkbench_v170_BackendSprint4_ThreePasses_PRELIMINARY_20261116'
Set-Location -LiteralPath $root
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT4_DEFERRED_WINDOWS_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

It must compile the app on Windows, run **all 130 real native CTests**, all 830+ Python tests, and diagnose the known grouped-edge case #45. An audit exit **3** may indicate automated PASS but manual engineering acceptance not recorded; never call it a release. Any build error, missing native tests or source hash failure blocks acceptance. Keep `build/v170_comprehensive_audit` as evidence for a future chat.

## Future engineering backlog (not represented as completed)

1. Integrate the translation-only sweep with moving assembly occurrences, then add exact OCCT narrow-phase collision and rotational interpolation in a native-tested implementation.
2. Connect supplier tolerance plans to BOM and inspection worksheets, with provenance and real shop capability data.
3. Integrate drawing reference audit into the unissued drawing/export workflow; add revision refresh acceptance and coherent geometry/datum persistence.
4. Split oversized `SceneDocument.cpp` (~13,656 lines) and GUI `MainWindow.cpp` into modules only after native baseline is confirmed; avoid risking the frozen UI during this coding sprint.
5. Retest grouped unequal-size edge blends, topology split/merge, rollback, save/load, mates, motion, sheet metal and the 60 manual engineering P/A/D scenarios on a real Windows build.

Three **focused coding passes** were performed for this checkpoint. They are not represented as three timed 2-hour sessions, and there has been no deferred/background test activity.

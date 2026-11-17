# CADWorkbench v1.70 — source audit and folded-sheet crash recovery

**Checkpoint:** 9 November 2026. Based on the complete `CADWorkbench_v170_Comprehensive_WindowsQA_GapAudit_PRELIMINARY_20261109` source and the user-provided Windows logs `Pasted text(20261109-100430).txt` and `Pasted text(20261109-102613).txt`. No new features were added. The 130 CTests and 60 manual acceptance scenarios remain enabled/unchanged.

## Findings in the Windows evidence

1. The **earlier focused native retest** passed three tests (`cadcore_v170_articulated_scene_transaction`, `cadcore_v170_assembly_mate_document`, `cadcore_parametric_foundation`). `cadcore_folded_sheet_metal` still raised a Windows segmentation fault. Its last log line was `[folded edit] commit edited solid: before`. Accepted preview had completed; the failure is within the subsequent commit call, not proved to be the extrusion itself.
2. The **later comprehensive audit** verified 417 source hashes and completed CMake configuration, but the build-runner wrapper reported `Native build error: CMake Warning:`. It consequently **did not execute CTest**. Its resulting `0/130` is `NOT_RUN`, *not* 130 native failures and *not* 130 native passes. The apparent `130/130` messages within Python output are synthetic contract fixtures for the audit tool, not live CTest results.
3. The later Windows Python source suite recorded 424 tests with 51 errors and two skips. Most errors were missing `OCP`/`cadquery` modules or `UnicodeDecodeError` when `Path.read_text()` tried to decode UTF-8 C++ sources using Windows `cp1252`. These are test-environment/source-reader failures and do not establish broken C++ geometries.

## High-confidence folded-sheet fault: Qt copy-on-write iterator invalidation

In `SceneDocument::editFoldedSheetMetalWithDevelopments()` the original implementation saved a mutable iterator `found` into `parts_`, then copied `QVector<Part> proposed=parts_`, and **after that** evaluated `std::distance(parts_.begin(), found)` to index the staged copy. In Qt 6, `QVector` is an alias of implicitly shared `QList`. A non-const `parts_.begin()` may detach the shared storage. The old `found` then refers to a different/inactive allocation; using it in `std::distance` is undefined behavior and may crash. This sequence exists in the commit path but not in the successful read-only preview path, matching the observed symptom.

**Applied fix:** search with `parts_.cbegin()/cend()` and determine `sourceIndex` **before** copying `parts_`. Access the staged part as `proposed[sourceIndex]`. The source-side failure/rollback/linked-body propagation/undo code remains unchanged. Added a regression contract to prevent the unsafe expression being reintroduced.

**Evidence boundary:** this is a concrete memory-safety bug and a strong root-cause hypothesis, not a Windows-confirmed repair until MSVC Release CTest passes. If the crash remains, use the existing `[folded commit]` stage trace (enabled by the full runner) to locate the next fault among `makeFoldedSheetSolid`, OCCT shape validation, linked-body propagation, and `checkpoint()` BRep deep-copy. A Windows crash dump/stack trace would then be the best further evidence; no bypass, timeout, or ignored test is justified.

## Comprehensive-runner corrections

* Build output is captured with `Start-Transcript` rather than PowerShell 5.1's `2>&1 | Tee-Object` stderr pipeline. The latter can promote nonfatal CMake warnings to terminating errors under `ErrorActionPreference='Stop'`. Real build-script CMake exit codes and expected Release binaries are still checked.
* All Python tests that called `.read_text()` with no explicit encoding now use UTF-8, so Windows ANSI locale does not cause false test failures. The runner sets `PYTHONUTF8=1` as additional protection.
* Added `requirements-kernel-reference.txt` (CadQuery, which brings OCP bindings) and optional `-SetupPythonEnv`; this creates and installs into a project-local `.venv-cadquery` only when requested. No global packages are installed. If the geometry dependencies are missing, the runner records the Python suite as **not run** rather than incorrectly reporting a failed CAD algorithm.
* Full 130-native JUnit and the 60 acceptance evidence matrix are retained. Results are fresh, never backfilled with old successes. The acceptance scenarios require independent evidence; `0/60` verified is not evidence of missing functionality.

## Source inspection and verification scope

* Inspected the complete project tree, CMake 130-test inventory, folded editing/preview/snapshot/persistence code paths, Windows build and audit scripts, Python test discovery, and capability-gap documentation.
* Removed the identified unsafe Qt iterator usage in the folded commit and searched the source for that exact problematic pattern.
* Full Python/source+OCP independent suite, bytecode compilation of tests, and portable GCC and sanitized Clang test suites executed in this environment. See the accompanying verification log for exact counts; these are not MSVC/Qt native passes.
* **Limitations:** This environment does not have the Windows MSVC+Qt 6.11+OCCT 8.0.1 toolchain. Its native executable, interactive GUI, and real CTests cannot be compiled or run here. A source-level review of thousands of lines cannot certify absence of all bugs.

## Windows verification sequence

From a freshly extracted source folder in Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot C:\vcpkg -SetupPythonEnv
```

`-SetupPythonEnv` installs only into `.venv-cadquery` and may need network access. Omit the flag if that environment is already installed. The runner builds all native targets, runs **every one of the 130 tests**, runs Python source/reference checks if dependencies are present, and refreshes the 130-CTest and 60-acceptance results in `build/v170_comprehensive_audit/`. It does not silently disable or skip any registered native test. Inspect `ctest_output.log`, `cadworkbench_release_ctest.xml`, `python_source_tests.log` and `v170_remaining_work.md`. If folded-sheet still crashes, share its last `[folded commit]` marker along with the stack/dump if possible.

## What remains before a final release

1. Confirm the folded-sheet memory-safety repair on Windows; rerun the full 130 native CTests, not only the focused case. Investigate **every** new failure independently; do not suppress failing tests.
2. Run and record all 60 manual acceptance scenarios P01–P20, A01–A20 and D01–D20. Test real GUI workflows, output files, undo/redo, persistence, cross-feature references, and invalid-input rollback. The automated acceptance matrix only associates relevant native test leads; it does **not** pass scenarios on their behalf.
3. From the existing implementation-status ledger, ongoing capability gaps include generalized arbitrary topological naming and feature-tree editing; persistent general multi-level assembly and fully certified continuous motion/collision; fully associative dimensions/GD&T and analytically accurate production sheet-metal geometry/nesting. Manufacturing CAM and postprocessing, rendering and other broad CAD workspaces also remain outside the present acceptance scope. These are source-ledger limitations, not new findings of this crash investigation.
4. Perform normal Windows GUI smoke, app launch/deployment, clean-machine installation and release artifact verification. Retain the last known passing baseline until all gating evidence is complete.

**Verdict:** source patch and test-harness corrections are ready for Windows retest; v1.70 is not yet certified or approved for final release.

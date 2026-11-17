# CADWorkbench v1.70 — Windows 130/130 results and remaining QA work

**Evidence reviewed:** User-provided `v170_comprehensive_audit.zip` and Windows PowerShell output from 9 November 2026. The native results and the Python errors below refer to the preceding `WindowsAudit_FoldedCommitFix` build; this checkpoint only updates Python QA setup, a source-contract test, and documentation. All production C++ files are unchanged.

## Proven Windows-native results

- Windows Release application and test executables built successfully.
- **130/130 distinct CTests passed**, 0 failed, 0 skipped, 0 missing. Confirmed against both the native-test matrix and the full CTest log, which ends in `100% tests passed, 0 tests failed out of 130` (61.12 seconds).
- The previous crashing **`cadcore_folded_sheet_metal` now PASSES** (2.23 seconds). Parametric foundation, articulated assembly scene and assembly-mate document tests also pass.
- This demonstrates recovery from the known native regressions, not complete GUI acceptance or proof that every unsupported engineering use case works.

## Windows Python issue and repair

The virtual environment correctly installed CadQuery/OCP, and geometry-reference smoke tests ran. The Windows Python test command reported **517 discovered tests, three import errors** in `v159_source_preflight`, `v160_source_preflight` and `v161_engineering_preflight`. All three import the `pygments` lexer; that package was missing from the isolated environment's requirements. The audit's fatal exit was therefore caused by Python dependencies, not a failing native CAD regression.

This checkpoint makes three changes:

1. Adds `pygments>=2.17,<3` to `requirements-kernel-reference.txt` alongside CadQuery.
2. Extends the comprehensive audit's dependency probe to require OCP, CadQuery **and Pygments**, so missing lexer dependencies are reported before running test discovery.
3. Adds `tests/v170_windows_python_dependencies_contract.py` to prevent the omission recurring. The Python suite ran **535/535 successfully** in the verification environment with all dependencies present. Native Windows tests of this new ZIP have not been rerun, though its production C++ source is byte-for-byte unchanged.

## End-to-end acceptance is not yet demonstrated

| Workstream | Native-test leads | Manual scenarios completed | Critical scenarios still unverified | Minimum formal PASS criterion |
|---|---|---:|---|---:|
| Parametric modeling | All mapped native leads pass | 0/20 | P01, P02, P09, P10, P12, P13, P14, P18 | 17/20 |
| Assembly engineering | All mapped native leads pass | 0/20 | A03, A07, A08, A11, A12, A17 | 17/20 |
| Drawings and sheet metal | All mapped native leads pass | 0/20 | D04, D08, D09, D17 | 13/20 |

**All 60 scenarios are `NOT_RUN`, not `FAIL`.** The release gate demands evidence for all critical scenarios and the workstream thresholds; comprehensive engineering acceptance should still attempt and document all 60 scenarios. The old estimated 85%/90%/81% functional completion figures are not formal evidence-derived scores.

### Recommended next evaluation order

1. **Critical GUI workflows first:** the 18 scenarios listed above. Exercise editing, regeneration, invalid-input rollback, undo/redo and save/reopen where applicable.
2. **Remaining 42 cases:** complete each workflow and capture a concise result file, output artifact, screenshot or log.
3. **Cross-cutting checks:** application launch and deployment, larger projects, persisted documents, STEP/IGES compatibility, units and tolerance handling, SVG/DXF/PDF export consistency, and invalid geometry handling.
4. Mark a scenario `PASS` only with valid evidence. A corresponding CTest passing is supporting evidence, not an automatic GUI pass. Re-run the gap audit after updating `build/V170_ACCEPTANCE_RESULTS.csv`.

### Distinguish unverified from known scope limitations

The existing engineering notes identify generalized arbitrary face/edge identity across topology splits/merges, general persistent assembly constraint/gear-mate solving and fully associative drawings/manufacturing geometry as important areas requiring careful acceptance review. They should be evaluated as **potential feature gaps**, not labeled failed solely because the scenario matrix has not been filled.

## Next Windows command

From the newly extracted source ZIP:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot C:\vcpkg -SetupPythonEnv
```

`-SetupPythonEnv` updates an existing `.venv-cadquery` or creates one. It now installs Pygments as well as CadQuery/OCP. The runner again builds and executes all 130 native tests, runs all Python tests and outputs refreshed evidence matrices. Alternatively, on the old source folder, install `pygments` into the existing isolated virtual environment to resolve the three import errors.

**Release decision:** Native gate **PASSED** based on uploaded Windows evidence. Python test-dependency issue **FIXED IN SOURCE, Windows rerun pending**. 60-scenario acceptance gate **NOT YET SATISFIED**. Not a final release.

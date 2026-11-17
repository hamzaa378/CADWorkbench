# Four-feature 85% milestone — evidence update (November 10, 2026)

**Not yet accepted at 85%.** This is an incremental candidate based on the November 10 FourFeatures source snapshot. The per-feature goal remains >=17 of 20 accepted checks (including every critical case); no native Windows or GUI/manual check has been marked passed from this Linux development session. Native inventory remains 130. The separate 60-scenario manual engineering gate is also unchanged.

## This increment's functional source changes

| ID | Development | New validation evidence | Major outstanding requirement |
|---|---|---|---|
| F01 | Rigid transformation of analytic/freeform curved shell references now preserves the original transported `curved_witness_` and optional second witness instead of recapturing a shifted midpoint; two-stage curved native regression added | Python source contracts, existing independent OCCT reference suite pass; added `curved_face_lineage_v170_tests.cpp` assertions require Windows execution | End-to-end topology split/merge with save/reopen, native Windows tests, GUI evidence; no universal naming guarantee |
| F02 | Two consecutive rigid rotations of spline-edge references checked for witness position, tangent/curvature stability, and no identity drift | Native `selected_edge_v160_tests.cpp` assertions added; local integration contracts pass | Native Windows B-rep run, survival through complex Boolean split + subsequent fillet/chamfer, GUI edits |
| F05 | New common `PlanarMateSelection.h` adapter creates physical center, actual oriented normal and deterministic auxiliary guide from selected planar B-rep faces; mate dialog offers geometry prefill. It **explicitly asks** before adding user-defined center/twist locking constraints and refuses nonplanar/mismatched/stale picks | Native `assembly_mate_document_v170_tests.cpp` observations and rejection checks added; source integration contract pass | Native Windows compilation and solver test, live GUI face selection and saved-scene replay; true *partially constrained* geometric mates and automatic face reattachment remain unimplemented |
| F08 | Batch drawing previews optionally produce both SVG and PDF with full-batch rollback, index records file mappings and current geometry tokens; source docs stress unissued and non-certified | Existing native PDF renderer tests remain registered, six new Python/source integration checks, total 603 passed | Windows PDF rendering, multi-sheet save/reopen workflows, reviewed issue/approval UI, PDF stress testing |

## Verified automated evidence and limitations

- Python/source/reference: **603/603 PASS**, run via `python -m unittest discover -s tests -p '*.py' -q` in the source root. Some tests intentionally simulate failure reports as negative tests; their printed 129/130 lines are not native runs.
- Portable C++: **18/18 executable suites PASS** under Linux GCC, independent of Qt/OpenCascade Windows builds.
- CTest CMake inventory: **130 registrations**, not 130 passes for this new source.
- Windows MSVC/Qt/OpenCascade compile: **NOT RUN** for this candidate.
- New C++ topology and face-adapter assertions: **SOURCE ONLY / NOT EXECUTED** until native Windows compilation.
- Manual per-feature 20-check certification: **NOT RUN**; this new candidate does not claim >=17/20 for any of four features.
- Manual workstream engineering scenarios: **P 0/20 NOT_RUN; A 0/20 NOT_RUN; D 0/20 NOT_RUN**.

## What "85%" means in this project

An engineer must record 17 or more independently reviewable results per feature's **20-point** checklist (see `FOUR_FEATURE_85_TARGET_20261110.md`), and every critical test must pass. No percentage rises automatically due to more source lines or passing local contracts. To accelerate this gate, run the new native audit first, then record targeted geometry/GUI acceptance evidence for F01/F02/F05/F08. The **final workstream release gate remains separate and stricter**.

## Windows fresh-source verification

1. Extract the `...85Progress_GitHub_PRELIMINARY...zip` into its own directory. Leave the confirmed Fix 2 baseline untouched.
2. Open PowerShell in the extracted directory containing `CMakeLists.txt`.
3. Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot C:\vcpkg -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

**Target:** 130/130 native CTests and 603/603 Python/source/reference tests. An audit exit code **3** means automated checks passed and 60 manual scenario acceptances remain pending. Exit **1** is a native/source failure. Neither has been claimed for the new snapshot. Save the full audit log, and do not skip failing native tests or edit the integrity manifest.

## Package status

The full source ZIP can be committed as an **explicitly preliminary** GitHub development branch; the guarded upgrade ZIP applies only to the immediately previous FourFeatures snapshot. Neither is a Windows application installer. Final fresh-install ZIP is deferred until the full ten-feature and engineering-release gates have been met.

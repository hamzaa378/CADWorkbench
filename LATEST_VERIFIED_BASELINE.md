# Verified CADWorkbench checkpoint — 17 November 2026

**Source:** `CADWorkbench_v170_BackendSprint11_EngineeringWorkflow_COMBINED_PRELIMINARY_20261117.zip`, repackaged for GitHub. Project version `1.70.0`, Sprint 11, full consolidated source.

## User-reported actual Windows automated results

- MSVC compiled and produced `build/Release/CADWorkbench.exe` in the user's Windows environment.
- **130/130 native CTests PASSED**, including historical topology/regeneration test `cadcore_universal_feature_tree` (#45).
- **952/952 full Python/source geometry-reference tests PASSED**.
- Independent diagnostic result: `AUTOMATED_PASS`.
- Overall audit exit code `3`: **manual engineering acceptance pending** (by design; not a failed automated test).
- All 60 engineering acceptance cases remain formally `NOT_RUN` (0/60 verified). Parametric P: 0/20, assembly A: 0/20, drawing/sheet-metal D: 0/20. Critical scenarios are not exempt.

The Windows output supplied in the conversation is the evidence for these outcomes; no Windows build has been run by the GitHub packaging process. Do not upload private Windows console transcripts or files containing personal system paths unless intentionally sanitized.

## Release policy

Automated verification passing is **not** manufacturing readiness. No claim of exact continuous rotating-solid collision detection, universal persistent CAD topology naming, certified GD&T drawings or industrial safety qualification is made. Verify real-use scenarios in the GUI and record supporting evidence in `build/V170_ACCEPTANCE_RESULTS.csv` before release approval.

## Repo integrity

`SHA256SUMS.txt` and `SOURCE_MANIFEST_SHA256.txt` cover the GitHub source tree, excluding the manifests themselves. GitHub excludes build output and installed dependencies. Check the exact extracted archive with `python tools/preflight_major_build.py --root .`, and verify the new Git clone separately.

## Next development

Preserve the baseline and follow 75% integration, 20% reliability/performance, 5% new capability; progress independent parametric, assembly, drawings/sheet-metal streams with all established regression gates intact.

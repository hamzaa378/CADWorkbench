# CADWorkbench v1.70 — Preliminary development snapshot, 7 November 2026

**Type:** explicitly requested preliminary source snapshot, **NOT** an approved v1.70 release, verified Windows binary, or 85/85/65 acceptance completion.

## New in this pass: reviewable adaptive-motion intervals (A16)

- `AdaptiveMotionScan.h/.cpp` now returns an interval assessment for every adjacent final time sample, including time limits, smallest endpoint gap, the conditional Lipschitz clearance lower bound when a speed bound is supplied, sampled endpoint overlap, conditional above-margin status and whether further numerical refinement is requested.
- `SceneDocument::scanAssemblyMotionAdaptive` exposes the assessments through an optional output while retaining existing call compatibility. All caller outputs remain unchanged on validation or evaluation failure.
- The Qt assembly interference dialog summarizes the number of unproven intervals, sampled-overlap intervals and first three unproven time ranges. An **Export interval audit CSV...** action provides a separate, atomically saved report containing body IDs/names and every interval's assessment.
- Without an independently justified relative surface-speed bound, every interval remains unproven and its conditional lower-bound CSV field is blank. Reported bounds are conditional on input correctness and cannot certify continuous collision-free motion or machine safety. The existing sampled-motion CSV remains available.
- Existing motion/bend/sheet-metal/constraint tests are preserved, and additional portable, native-source and static-contract assertions cover success and failure/rollback.

## Verification completed in this environment

- Python independent reference and source-contract tests: **372/372 PASS** (`python -m unittest discover -s tests -p '*.py' -q`). Some of these inspect source code or use independent geometry references; they are **not** native GUI/OCCT acceptance.
- GCC portable C++ suite: **12 targets PASS** (`CAD_SKIP_PYTHON=1 bash scripts/verify_portable_consolidation.sh g++`) with AddressSanitizer and UndefinedBehaviorSanitizer. Focused adaptive-motion tests: **83 assertions**.
- Clang portable C++ suite: **12 targets PASS** with the same sanitizer configuration, including all 83 focused adaptive-motion assertions.
- Native CTest registrations: **130**, unchanged. The existing native assembly motion CTest now has assertions for interval report content, atomic failure behavior and CSV replacement rollback; those cases are **not executed** here.
- Source-archive hash manifest must be checked after packaging.

## Mandatory Windows follow-up (still pending)

From the extracted project root, with MSVC/Visual Studio, vcpkg Qt6, and OpenCascade installed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

Require the original `cadcore_v170_interleaved_history` face-machining regression to pass, then all 130 native CTests. Exercise the assembly scan dialog and CSV manually, including a false speed bound and output rollback. Preserve the previous verified installation as a separate backup. Do not treat this preliminary archive as a verified binary or final release.

## Remaining engineering work

Two workstreams must reach 17/20 formally accepted end-to-end scenarios each (85%), with the third at 13/20 (65%). None of those numerical completion thresholds has yet been evidenced. Main gaps remain persistent topology through arbitrary split/merge, unrestricted mixed feature history, GUI mate rebind/drive for large hierarchical assemblies, associative drawing dimensions and professional fold/refold sheet-metal regeneration. See `docs/V170_RELEASE_GATE_85_85_65.md`.

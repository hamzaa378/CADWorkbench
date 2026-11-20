Read **[SPRINT16_CONTINUATION_20261117.md](SPRINT16_CONTINUATION_20261117.md)** first. Sprint 16 adds atomic synchronized multi-joint motion retiming and integrates existing native CAD geometry, saved drawing, folded/refold, model integrity and assembly keyframe-rate checks into one UNISSUED readiness report. Local Python and portable C++ gates passed; the Sprint 16 Windows MSVC and 60-case manual acceptance gates are pending. Preserve the verified Sprint 11 rollback; do not disrupt any concurrent Sprint 15 Windows build.

# LATEST PRELIMINARY CHECKPOINT — Sprint 15

Read **[SPRINT15_CONTINUATION_20261117.md](SPRINT15_CONTINUATION_20261117.md)** first. Sprint 15 provides complete GitHub-ready source and adds multi-joint mate authoring, strict multi-sheet current-geometry drawings, and a refold/OCCT volume check. Local Python/GCC/Clang regressions pass; Sprint 15 MSVC and 60 manual acceptance are pending. Preserve Sprint 11 as last Windows-verified automated source baseline.

> **LATEST PRELIMINARY SOURCE, 2026-11-17: Sprint 12 major cross-revision engineering integration.** Read `SPRINT12_CONTINUATION_20261117.md` and `README.md` first. Sprint 11 remains the last fully Windows-automated-verified checkpoint (130/130 native, 952/952 Python). Sprint 12 locally passed 985 Python and 41 GCC/Clang portable suites, but has NOT been MSVC compiled. Manual acceptance 0/60.

> **CURRENT SOURCE AND VERIFICATION, 2026-11-17:** Start with [`README.md`](README.md) and [`LATEST_VERIFIED_BASELINE.md`](LATEST_VERIFIED_BASELINE.md). Sprint 11 builds on Windows, 130/130 native CTests and 952/952 Python/source checks passed. Manual engineering acceptance remains 0/60. The historical notes below have been superseded for current build status.

---

# Current source checkpoint: Sprint 11, UNVERIFIED Windows native build

See `SPRINT11_CONTINUATION_20261117.md`. The previously user-verified Windows automated baseline remains Sprint 9 (130/130 native CTests). Sprint 10 and Sprint 11 need their own Windows rebuild before promotion.

# CADWorkbench — latest consolidated source checkpoint: Sprint 9

**Date: November 16, 2026. PRELIMINARY source only, NOT Windows/MSVC/Qt/OCCT verified.**

Start with `SPRINT9_CONTINUATION_20261116.md`. The new backend APIs are `HolePositionInspection.h` and `RigidMotionSpeedBound.h`. This is separate from the Sprint 8 source the user is building; do not replace that in-flight build or the last Windows-verified baseline. The new early build launcher is `RUN_SPRINT9_WINDOWS_TARGETED_BUILD.ps1`.

**Local evidence:** 900/900 Python tests, 37/37 portable GCC tests, 37/37 portable Clang tests, plus two isolated non-Qt native-style GCC target replays. The original 130 Windows CTests remain registered but **not executed for Sprint 9**; 60 manual engineering cases remain not run.

Feature limitation: manufacturing hole inspection requires user-supplied actual measurements and verified datum alignment; the motion velocity bounds must be independently verified over the whole time interval. Neither API is a user-facing UI module or a safety certification.

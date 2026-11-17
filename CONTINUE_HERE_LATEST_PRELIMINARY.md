> **CURRENT SOURCE AND VERIFICATION, 2026-11-17:** Start with [`README.md`](README.md) and [`LATEST_VERIFIED_BASELINE.md`](LATEST_VERIFIED_BASELINE.md). Sprint 11 builds on Windows, 130/130 native CTests and 952/952 Python/source checks passed. Manual engineering acceptance remains 0/60. The historical notes below have been superseded for current build status.

---

# Current source checkpoint: Sprint 11, UNVERIFIED Windows native build

See `SPRINT11_CONTINUATION_20261117.md`. The previously user-verified Windows automated baseline remains Sprint 9 (130/130 native CTests). Sprint 10 and Sprint 11 need their own Windows rebuild before promotion.

# CADWorkbench — latest consolidated source checkpoint: Sprint 9

**Date: November 16, 2026. PRELIMINARY source only, NOT Windows/MSVC/Qt/OCCT verified.**

Start with `SPRINT9_CONTINUATION_20261116.md`. The new backend APIs are `HolePositionInspection.h` and `RigidMotionSpeedBound.h`. This is separate from the Sprint 8 source the user is building; do not replace that in-flight build or the last Windows-verified baseline. The new early build launcher is `RUN_SPRINT9_WINDOWS_TARGETED_BUILD.ps1`.

**Local evidence:** 900/900 Python tests, 37/37 portable GCC tests, 37/37 portable Clang tests, plus two isolated non-Qt native-style GCC target replays. The original 130 Windows CTests remain registered but **not executed for Sprint 9**; 60 manual engineering cases remain not run.

Feature limitation: manufacturing hole inspection requires user-supplied actual measurements and verified datum alignment; the motion velocity bounds must be independently verified over the whole time interval. Neither API is a user-facing UI module or a safety certification.

> **CURRENT SOURCE AND VERIFICATION, 2026-11-17:** Start with [`README.md`](README.md) and [`LATEST_VERIFIED_BASELINE.md`](LATEST_VERIFIED_BASELINE.md). Sprint 11 builds on Windows, 130/130 native CTests and 952/952 Python/source checks passed. Manual engineering acceptance remains 0/60. The historical notes below have been superseded for current build status.

---

# CADWorkbench v1.70 — Backend rapid engineering candidate (PRELIMINARY)

**Start here:** `BACKEND_RAPID_ENGINEERING_SPRINT_20261116.md`.

This update fixes MSVC C1128 with target-scoped `/bigobj`, adds actionable source-change rebuild explanations, real manufacturing assembly fit budget checks, an offline Windows build failure classifier and self-contained development-session regression scripts. Frontend features remain frozen, all 130 native CTest registrations remain enabled, and the previous Windows-verified baseline is separate. This new source has **not** yet passed a Windows MSVC build.

`python tools/run_portable_regressions.py --jobs 3` compiles each portable C++ fixture. `python -m unittest discover -s tests -p '*.py' -q` runs Python, source and geometry-reference tests. Use `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1` on Windows for the full native + Python gate. Consult its diagnostic JSON on failures. All 60 manual acceptance cases remain NOT_RUN. No production certification is implied.


### Sprint 11 connected inspection, drawing, and motion workflows

See `SPRINT11_CONTINUATION_20261117.md`, `tools/review_measured_holes.py`, `tools/reconcile_drawing_handoff.py`, `tools/engineering_readiness_workflow.py`, and `RUN_SPRINT11_WINDOWS_TARGETED_BUILD.ps1`. These are explicitly **UNISSUED, REVIEW REQUIRED** tools. No manual 60-case engineering acceptance or native Sprint 11 Windows verification has been claimed.

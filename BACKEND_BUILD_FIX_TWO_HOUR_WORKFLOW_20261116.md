# CADWorkbench v1.70 — Windows build hotfix + repeatable backend coding sessions

## Why this update exists

The user's Windows build of `BackendEngineering_MAJOR_PRELIMINARY_20261116` compiled `cadcore.lib` but failed while compiling `src/ui/MainWindow.cpp`. The MSVC C2374, C2086, C2371 diagnostic reports a local `contextGrid` redeclaration in the same `MainWindow` constructor at lines 422 and 1559. Because the native executable was not successfully rebuilt, the comprehensive audit correctly reported zero of 130 CTests run, even though 771 Python/source tests passed. The pip upgrade notice is unrelated.

## Exact source correction

The `ContextActionsPanel` layout is created once near the beginning of the constructor. The later redeclaration that re-fetched the same layout was removed. All existing selection actions still use the original `contextGrid` Qt-owned layout. **No new UI feature** is added; frontend development remains frozen. `tools/preflight_major_build.py` now guards against the exact duplicate-variable regression before running the expensive Windows build.

## Strong two-hour coding workflow

New files: `tools/dev_session.py`, `scripts/START_TWO_HOUR_DEV_SESSION.ps1`, `scripts/FINISH_TWO_HOUR_DEV_SESSION.ps1`, `tests/two_hour_dev_session_tests.py`, and `docs/TWO_HOUR_BACKEND_DEV_WORKFLOW.md`.

1. START validates the exact clean source manifest and records hashes in `build/dev_sessions/active_session.json` (no Git requirement).
2. Code freely in `src/core`, add tests, and avoid UI source changes. No timer or automatic action interrupts development.
3. FINISH stages the updated source (both manifests), rejects unauthorized UI edits and inconsistent files, and verifies the preflight.
4. The untouched comprehensive audit compiles all Windows targets, runs every one of the 130 CTests and all Python/source tests, and generates the 60 manual-acceptance scenario reports.
5. Post-audit evidence verification refuses to close the session if tests failed, expected test counts are missing, results are stale or source changed since staging.
6. The session report records exactly which source files changed and distinguishes automated passing from engineering manual acceptance.

The `-SetupPythonEnv` option uses the isolated project virtual environment. The first clean build may install Windows dependencies; subsequent builds should reuse the local build and venv.

## Local verification and release status

- All 130 native CTest registrations preserved in `CMakeLists.txt`.
- 781/781 Python/source/reference tests passed, including 10 new development-session guard tests.
- 24 portable C++ regression executables to be rerun from the delivered ZIP before release.
- `tools/preflight_major_build.py` catches the known MSVC duplicate declaration.
- Windows Qt/MSVC recompilation of this fix is still **required**; do not claim the user's failed build was converted to a passing Windows build.
- 60 engineering manual acceptance scenarios remain pending. No 95% completion or production readiness claims.

## Safety and source preservation

The upgrade ZIP is guarded for the exact preceding BackendEngineering source, creates verified backups, rejects unknown local modifications and can roll back. Prefer the full corrected GitHub source ZIP for clean installation. **Do not overwrite the user's known 130/130 passing CTest47-fix Windows baseline.**

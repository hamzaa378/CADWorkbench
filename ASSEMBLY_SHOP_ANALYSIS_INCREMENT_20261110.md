# CADWorkbench v1.70 — Assembly connectivity and folded-shop analysis increment

**Status:** preliminary development source. Not the final accepted engineering release. The last Windows-tested source baseline is still the 2026-11-09 Fix 2 build. This increment has not been compiled or run under Windows Qt / OpenCascade.

## Implemented workflows

### Saved rigid-mate network: critical component occurrence analysis

The existing `Assembly > Audit saved rigid mate structure` command previously found disconnected nodes, single critical *connections* (bridges), and cycles. It now additionally identifies **single component occurrences** (other than the fixed occurrence) whose removal would disconnect otherwise reachable components from the fixed frame. This is useful when reviewing assembly connectivity in fixtures, removable subassemblies and multi-connection frame structures.

- Every reported occurrence identifies which *other* occurrences would lose their path to the fixed frame.
- Already-disconnected components are excluded from impact calculations, so the report doesn't overstate new loss of connection.
- The analysis supports graph cycles and parallel mate links; a ring is not falsely reported as fragile, but the junction between a ring and a dangling fixture is.
- A new **Save audit report...** button writes the complete text report using `QSaveFile`, so aborted writes do not leave incomplete output.
- This is a structural graph check only; it **does not certify geometric mate constraints, kinematic degrees of freedom, load capacity or collision**.

### Folded sheet metal: shop-selected bend-center spacing

The existing **Folded-sheet shop check** now accepts a **minimum adjacent bend-line center-to-center spacing (mm)** and a **K-factor for this analysis**. The check calculates bend-line stations from the saved folded-body definition and evaluates every consecutive station pair. On failure, the dialog names the failing pair(s), required clearance and computed minimum. When no limit is entered, the old radius/thickness and flange checks are preserved.

- The spacing computation does not mutate the folded model or its saved K-factor.
- A single-bend body cannot receive an adjacent-pair spacing requirement.
- Invalid spacing values, reversed/overlapping bend intervals, and non-finite values are rejected without overwriting the report structure.
- User-supplied spacing is a planning requirement, **not** a formal press-brake tooling standard, sequence collision simulation, or springback prediction.

## Regressions

- 580 Python/source/independent-kernel tests passed on Linux (3 additional source integration tests).
- 16 standalone g++ C++20 portable suites passed, including 34 assembly structure and 38 folded bend-sequence assertions.
- Exactly 130 registered native Windows CTests remain unchanged; no CTest was skipped or renamed.
- No new native tests were registered. The portable tests validate the non-Qt algorithms; native Qt GUI actions still require Windows review.

## Windows native validation (not yet run for this increment)

Extract the ZIP into a new, empty directory. In PowerShell, change into its project root and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Target: 130/130 native CTests and 580/580 Python/source tests. Exit code 3 is expected **only after** all automated tests pass and the 60-scenario manual engineering acceptance remains outstanding. Any compiler, test, or integrity failure is a real blocker.

## Release gate and workstream status

Previous estimated implementation coverage: parametric 85%, assembly 90%, drawings/sheet metal 81%. These are historical estimates, not verified engineering completion rates and are **not revised** here. Manual end-to-end acceptance remains 0/20 recorded in each workstream (unrun, not necessarily failed). The final release remains withheld until the required native build, all critical GUI scenarios, and 60-case acceptance thresholds are verified.

This increment does not solve persistent face IDs across arbitrary splits/merges, generalized geometric mating, associative GD&T or complete formed-sheet manufacturing validation. Those remain in the engineering backlog.

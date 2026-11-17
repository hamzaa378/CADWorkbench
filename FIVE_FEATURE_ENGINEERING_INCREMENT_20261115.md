# CADWorkbench v1.70 — Five engineering developments (15 November 2026)

**PRELIMINARY DEVELOPMENT SOURCE — NOT AN INSTALLER, RELEASE, OR 85%-CERTIFIED FEATURE SET.**

This increment is based **exactly** on `CADWorkbench_v170_FourFeatures_85Followup_PRELIMINARY_20261110_GitHub_Source.zip`. The last Windows-tested Fix 2 source remains a separate passing baseline.

## Features implemented in source

| Milestone track | Source module | Application integration | Status/limit |
|---|---|---|---|
| **F05 cylindrical mechanical fits** | `src/core/CylindricalFitReview.h` | Assembly six-DOF mate dialog: **Review selected cylindrical fit (read only)** | Explicit bore/shaft identity, signed dimensional deviations, worst-case clearance/interference/transition. No geometry is moved or clearance certified |
| **F06 hierarchy** | `src/core/AssemblyHierarchyAudit.h` | Assembly menu: **Audit explicit occurrence hierarchy CSV (read-only)** | Validates uniquely identified loaded component occurrences, parents, cycles, roots, leaves, and depth. The CSV is external user-authored hierarchy, NOT inferred from existing mate links and NOT persisted as a new assembly tree |
| **F07 motion review** | `src/core/MotionDriveLimitReview.h` | Assembly menu: **Review selected joint drive limits (read-only)** | Reports ACTUAL saved piecewise-linear peak commanded-coordinate speed; separately checks hypothetical smoothstep speed/acceleration envelopes against operator limits. Smoothstep is not applied; actual linear accelerations may jump at keyframes |
| **F09 datum review** | `src/core/GdtDatumReview.h` | File → Audit all linked drawing dimensions, appended manual GD&T warning section | Datum sequence validation and design-review advice for form/position/orientation/profile frames. Does NOT modify existing frames or certify any ASME/ISO standard |
| **F10 end-relief preflight** | `src/core/BendReliefPlanning.h` | Existing folded-sheet **shop limits** dialog, optional proposed width/depth and project rules | Reviews each saved bend against project-supplied minimum width/thickness and depth/radius allowances; never modifies the flat blank or builds relief cuts |

These five items are **functional incremental capabilities**, not full completion of F05/F06/F07/F09/F10's ten-feature milestone definitions. In particular hierarchical assemblies, actual bounded motion trajectories, GD&T feature anchoring, and manufactured bend relief geometry remain unfinished.

## Regression evidence

- `python -m unittest discover -s tests -p '*.py' -q` — **624/624 PASS** locally. Six new Python source integration contracts added.
- `python tools/run_portable_regressions.py` — **19/19 executable suites PASS** locally, including `five_feature_engineering_portable_tests.cpp` with 51 focused assertions.
- `python tools/verify_130_tests.py` — **130 unique CTest registrations preserved**. This is NOT evidence of running them.
- Qt/OpenCascade Windows MSVC compilation for this increment — **NOT RUN**. No Windows 130/130 claim is made.
- Manual engineering acceptance — **P=0/20, A=0/20, D=0/20 NOT_RUN**, not FAIL.
- Feature-specific 85% targets (>=17/20 plus all critical checks) — **NOT CERTIFIED**.

### Core validation behavior

All five functions reject malformed inputs without committing partial results; the tests also exercise output preservation, boundary cases, and simulated noncompliant manufacturing limits. The added audit operations make no `SceneDocument` mutation. These are engineering planning checks with explicit known limitations, not certificates.

## Manual Windows build / audit

Extract the full source ZIP into a **new directory**. Open PowerShell in the folder with `CMakeLists.txt`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 `
  -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit: $LASTEXITCODE"
```

Target for the increment: **130/130 native Windows CTests plus 624/624 Python tests**. If all automated checks pass but manual acceptance is incomplete, an audit exit code of 3 may be expected.

### Source-only upgrade (alternative to clean preliminary source extraction)

Use the upgrade ZIP only against an untouched extracted tree from the immediately preceding **85Followup** full-source ZIP. Extract the kit separately, then:

```powershell
py -3 .\apply_upgrade.py --target 'C:\path\to\CADWorkbench_v170_FourFeatures_85Followup_PRELIMINARY_20261110' --check
py -3 .\apply_upgrade.py --target 'C:\path\to\CADWorkbench_v170_FourFeatures_85Followup_PRELIMINARY_20261110' --apply
```

The upgrade makes a timestamped backup outside the source tree and supports `--rollback BACKUP_DIR`. Do not upgrade the Windows-tested Fix 2 source with this delta. Do not run this upgrade against a separately modified Git checkout without a clean preflight.

## Remaining work toward milestone and final release

1. Make the hierarchy a persistent editable occurrence tree with stable parenting, import/export, and integration with real constraints.
2. Add kinematic trajectory choice/commit (smoothstep or other bounded profiles), then verify actual swept geometry and collision limits.
3. Extend GD&T from manual source-body frames to supported feature/datums, measured tolerances, standards profiles, and approved drawing revision workflows.
4. Generate actual per-bend relief cuts, preserve parametric regeneration, and verify folded/refolded solids and tooling feasibility.
5. Close topological splits/merges and geometric mate persistence (F01/F02/F05/F08 carried forward).
6. Finish the remaining 10-feature milestone and run the Windows, 130 native CTests, 60 GUI scenarios and final engineering release gate.

**Do not publish as a production GitHub release or a known-working Windows installer yet.** The full source ZIP is suitable for a development branch and includes CMake/vcpkg, tests, docs, and source hashes.

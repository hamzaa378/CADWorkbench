# CADWorkbench v1.70 — Backend Sprint 9: Inspection and Rotating-Motion Safety

**Date: 2026-11-16. Status: PRELIMINARY SOURCE. Not a Windows-verified release.**

## Baseline and change-control

Forked from the complete Sprint 8 combined ZIP. User is separately building Sprint 8 on Windows. Sprint 9 must be tested in a *different extracted directory*, and the working Windows baseline must not be overwritten. Frontend remains unchanged and no document/scene serialization version changes are introduced.

## Feature 1 — Hole pattern dimensional inspection (`src/core/HolePositionInspection.h`)

A pure C++20, stateless manufacturing-worksheet API accepts up to 1,000 individual hole measurements (nominal XY, measured datum XY, optional projected-height XY, measured actual diameter, specified min/max size, specified diametral position tolerance, RFS or MMC, optional largest mating-pin diameter). It records a per-hole size result, position error diameter, MMC bonus, worst tested projected-axis deviation, actual clearance against the supplied pin and combined decision. Pattern-level totals and worst-position utilization are available.

Importantly, projected-axis endpoint checks are appropriate for a **straight measured centerline in the supplied datum frame**, since the Euclidean deviation norm is convex along that line. This does not model bore form error, ovality, cylindricity, real datum establishment, thermal effects or manufacturing measurement uncertainty. MMC means **internal hole minimum diameter**; a pin fit comparison is only a conservative geometric centerline worksheet, not a complete ASME Y14.5 or ISO GPS determination. All needed measurements and tolerance definitions must be supplied by the user. The service stages its entire result and fails without overwriting the prior report on malformed IDs, NaN, invalid dimensions, or inconsistent projected-axis data.

Native test wiring: five added behavioral checks under existing `workshop_engineering_v164_tests.cpp` tolerance-suite execution (CTest `cadcore_v164_tolerance_stacks`). No new CTest registrations.

## Feature 2 — Rotating rigid-body motion speed envelope (`src/core/RigidMotionSpeedBound.h`)

New `calculateRigidPairSpeedBound` accepts whole-body local axis-aligned enclosures relative to each body's rotation origin, plus independently established upper bounds on **origin translation speed (mm/s)** and **angular velocity (rad/s)** throughout the motion interval. It computes a conservative swept-point speed bound `v_surface <= v_origin + omega*r_max` per body; pairwise relative speed is bounded by their sum. This closes a gap between the earlier translation-only AABB sweep and the existing `AdaptiveMotionScan` conditional Lipschitz proof mechanism.

New `scanRigidPairWithSpeedBounds` configures the existing read-only adaptive scan with the derived bound; it never infers angular/linear limits from discrete samples or asserts a real rotating-solid collision-free state without an independently justified speed bound and accurate clearance observations. An independent callback must provide sampled **solid distance/overlap**. Angular joint solver and B-rep evaluator are *not* automatically wired to this worksheet. A bound is conditional on source geometry remaining rigid, bounding boxes enclosing the whole initial body, and speed limits holding globally.

Native test wiring: new checks under existing `articulated_assembly_v170_tests.cpp` (CTest `cadcore_v170_articulated_closed_loop`); `AdaptiveMotionScan.cpp` was explicitly added to that target in CMake to avoid an unresolved external symbol on Windows.

## Testing and build safety

* 900/900 full Python/source tests PASS (up from 887). Earlier Sprint 4 contract updated from exactly 35 portable suites to **at least 35**, maintaining baseline coverage as suites increase.
* 37/37 full portable C++ regression executables PASS on GCC and 37/37 on Clang. Two added portable tests performed **60,025** positional inspection checks and **30,016** motion speed-bound checks per compiler.
* Standalone local GCC build of the affected native-style tests: articulated **36 checks passed**, workshop tolerance **317 checks passed**. This does *not* replace MSVC, Qt or OpenCascade testing.
* Static source audit confirms 130 CTest registrations remain. `MotionDriveLimitReview.h` received a nonfunctional statement-format correction after a -Wmisleading-indentation warning during targeted GCC compilation.
* `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1` expects 900 Python tests for the new candidate. Sprint 8 files and its earlier expected count are preserved in its original archive.

## Windows procedure — only after deciding to build Sprint 9

Place the **Sprint 9 ZIP** in Downloads and open a *fresh* PowerShell window. This does not touch the Sprint 8 extraction.

```powershell
Set-Location "$env:USERPROFILE\Downloads"
$zip = 'CADWorkbench_v170_BackendSprint9_InspectionMotion_COMBINED_PRELIMINARY_20261116.zip'
if(!(Test-Path $zip)){throw 'Sprint 9 ZIP missing'}
$dest = "CADWorkbench_Sprint9_Test_$(Get-Date -Format yyyyMMdd_HHmmss)"
Expand-Archive -LiteralPath $zip -DestinationPath $dest
Set-Location (Join-Path $dest 'CADWorkbench_v170_BackendSprint9_InspectionMotion_PRELIMINARY_20261116')
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT9_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg'
# Targeted exit 4 indicates this early check PASSED but full release gate was NOT run.
# For the complete MSVC application build + all 130 native tests:
# powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT9_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -FullAudit
```

The old 130-test registration count is intentional. Native execution and all 60 manual acceptance scenarios remain pending. Do not claim safety or standards conformance based only on these arithmetic worksheets.

## Continuing development

1. Read the Sprint 8 Windows log before promoting a baseline; fix native integration failures rather than renaming/ignoring tests.
2. Add a data adapter from actual document-drawn hole patterns, with consistent datum coordinate transforms, but **do not fabricate measured actual positions** from nominal CAD geometry.
3. Use joint-velocity limits + verified B-rep bounding geometry to populate a motion speed envelope; test rotating fixture collision/clearance against real OCCT solids and adaptive observations.
4. Continue drawing/source revision recovery, especially undo/redo/save/reopen under actual Windows CTests.
5. Break up oversized SceneDocument.cpp as a separate audited refactor after a native green build.

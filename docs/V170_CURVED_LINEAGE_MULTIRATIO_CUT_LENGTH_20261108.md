# CADWorkbench v1.70 — curved-lineage, articulated-ratio graph and cut-length development

**2026-11-08. PRELIMINARY COMPLETE SOURCE.** This source continues the previous `MajorIntegrated_ParametricCoupling_ContourNesting` archive without changing existing project schema v24 or deleting older tests.

## Changes to actual engineering behavior

### Parametric model: persistent physical witness on curved-face Boolean descendants

The curved-face `Open-face shell` Boolean rebase path now retains its **original physical curved witness**, including the optional second surface witness for freeform faces, after a successful current-kernel `Modified`/`Generated` lineage check and a unique geometric resolve. Previously the rebase copied a newly sampled interior point from each trimmed descendant. On a chain of cuts this could move a reference away from its initial design location. The reference still fails closed when it is removed, ambiguous, unsupported by the current kernel's lineage, or outside the supported analytic/freeform tolerances. A native OCCT case now performs two subsequent cylindrical crops and verifies the witness never drifts. This is not arbitrary topology naming through any split, merge or B-rep modification.

### Articulated assembly: acyclic multi-link drive graph authoring

The existing batch drive editor no longer limits a coordinated change to a single leader-follower ratio. It permits additional editable relationship rows for a directed **acyclic** drive graph, including chains and branches. The same `drive_coupling::solve` validates incoming follower uniqueness, travel limits, unit compatibility, cyclic relationships, finite ratios, and atomic targets. The resulting target map then enters the previously present full articulated preview, optional sampled-clearance preflight and transactional checked commit. The links are intentionally **edit-time only**; saved persistent gear/rack mates are still outstanding. Existing saved joints, coordinate limits and project formats are preserved.

### Sheet metal: neutral cut-length manufacturing evidence

Added `StockContourMetrics.h` to compute exposed material perimeter and net area from the actual orthogonal developed material segments, subtracting interior seams introduced by rectangular material decomposition. The multi-part associative stock job now emits `cut-length-summary.csv` for every part, with neutral cut length, net material area and removed relief area. Its `batch-manifest.json` records the aggregate cut length and the CSV SHA-256 digest. The new file is staged with the SVG/DXF/bend schedules and is only published after every part succeeds. This is **not** kerf, lead-in/out, piercing, toolpath order, grain, springback or certified manufacturing planning.

## Checks completed in this environment

- 510/510 Python/source tests pass, including four new contract checks.
- 21/21 GCC portable C++ targets pass (normal build).
- 21/21 Clang portable C++ targets pass (ASan/UBSan enabled).
- Standalone cut-length tests cover plain rectangles, edge reliefs, internal holes, corner reliefs, overlapping reliefs, output rollback and 500 deterministic random cutout cases.
- Native OCCT regression tests added for two successive curved crops and the manufacturing batch manifest. They are not compiled or executed here.
- Full Windows MSVC/Qt/OpenCascade Release build, 130 native CTests, and the 60 formal acceptance scenarios have **not** been rerun on this source.

## Deferred baseline issues

The most recent Windows evidence before this pass reported 128/130 native CTests passing, with `cadcore_v170_assembly_mate_document` (clearance report expectation) and `cadcore_folded_sheet_metal` (segmentation fault) failing. These tests remain enabled and deliberately unmodified per the user's instruction. The earlier test result does **not** establish 128 passing on this new source revision.

## Windows retest

From a freshly extracted folder, run `powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_NEW_WORKFLOWS.ps1 -VcpkgRoot C:\vcpkg` to exercise focused development flows, or run `RUN_V170_RELEASE_GATE.ps1` for the full native gate. Do not approve final release until actual Windows results, formal acceptance and deferred regression corrections are complete.

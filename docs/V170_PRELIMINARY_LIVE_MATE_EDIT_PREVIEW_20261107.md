# CADWorkbench v1.70 — Preliminary live rigid-mate editing and preview

**Status:** unreleased source snapshot, **not** a Windows-native-verified release. This increment builds on the prior preliminary interval-audit snapshot. The release gate remains two workstreams at ≥85% and the third at ≥65% of 20 native/UI-verified acceptance scenarios per workstream. The 60 scenarios have not been formally scored.

## Assembly improvement (A05, A12, A17 — partial)

- `SceneDocument::liveRigidMateWorldDefinitions()` projects saved component-local frame observations to the current world frame for a stable, read-only editor.
- `SceneDocument::replaceLiveRigidMateConnection()` stages a replacement of the observations for **one existing pair** while preserving the pair identity and graph index; uses the existing full rigid mate solver and atomic OCCT B-rep commit path. Invalid/overconstrained edits are rejected without changing committed shapes, definitions, or pose.
- `SceneDocument::previewLiveRigidMateConnectionEdit()` is strictly read-only. It validates the **whole 2–32-body mate graph**, predicts which occurrence origins move (distance + rotation), and reports the worst closed-loop solver residual. A failed preview leaves the output report untouched.
- The main-window `Manage live rigid mate connections` action allows editing world-space anchor/axis/normal coordinates in place. A confirmation dialog shows the read-only preview before committing the full network. Unchanged editor input is ignored without creating a revision. The input now preserves twelve displayed decimal digits; a direct core-level unchanged edit also avoids checkpoint and B-rep token changes.
- Existing remove-link, undo/redo, save/reopen behavior is covered by the expanded native test fixture (but it has **not been compiled or run** with the Windows Qt/OCCT application here).

## Boundaries

- This is not general **mate reference rebinding** to different component identities. An attempted endpoint change is explicitly rejected.
- No persistent face/edge topology re-selection, interactive 3D drag gizmo, hierarchical joint-mate coupling, or adaptive collision certification is claimed.
- The preview is a **kinematic constraint check**, not a CAD kernel geometry validation, manufacturing approval, or collision/clearance certification. It does not replace the transactional OCCT checks during commit.
- User editing is for existing point, oriented-axis, and coincident-plane observations on independent free component occurrences. The full solver still rejects underconstrained and inconsistent systems.

## Evidence and regression coverage

- `python -m unittest discover -s tests -p '*.py' -q`: 381 passing tests (mixture of source-contract checks and independent numerical/geometry reference tests; **not** native GUI acceptance).
- GCC 14 / Clang portable C++ suites: 12 targets passed on each compiler with ASan/UBSan enabled. These checks exercise the underlying pose-network and other portable algorithms, **not** the new Qt editor/OCCT integration.
- Native Windows test `cadcore_v170_assembly_mate_document` includes new cases for read-only report, unchanged/no-op edits, dependent movement, corrupted loop rejection, undo/redo and save/reopen. This CTest and the remaining **129 native CTests require execution on Windows**.
- The previous witness-fixture correction to `cadcore_v170_interleaved_history` remains present; that test's Windows retest is a mandatory release gate.

## Current planning estimate (not a formal acceptance score)

| Workstream | Indicative implemented coverage | Release target | Unverified gap |
|---|---:|---:|---|
| Parametric modeling | ~60% | 85% | generalized feature timeline, persistent topology through arbitrary splits/merges, GUI editing breadth |
| Assembly engineering | ~60% | 85% | full mate rebind, interactive dragging/drive, arbitrary hierarchical motion verification |
| Drawings and sheet metal | ~40% | 65% | associative dimensions, multi-flange corner/relief completeness, refold round-trip, manufacturing exports |

These are qualitative engineering planning estimates, **not earned release-gate percentage points**. A native/UI scenario checklist is required before asserting real coverage.

## Windows acceptance procedure

Keep the previous passing app separate. Extract the preliminary source to a new folder, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

All 130 tests must pass, especially `cadcore_v170_interleaved_history` and `cadcore_v170_assembly_mate_document`. Manually review the `Manage live rigid mate connections` dialog with a valid chain and a contradictory closed loop, check that unchanged edits leave the file clean, and test save/reopen and undo/redo. **Do not promote this snapshot as a release unless the 85/85/65 evidence gate is met.**

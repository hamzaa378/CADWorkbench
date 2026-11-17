# CADWorkbench v1.70 — Combined three-workstream overhaul (preliminary source)

Date: 2026-11-08. This is one complete source package based on the prior
`AllThree_HistoryMotionBend_PRELIMINARY_Source_20261108` archive. It is not a
Windows-verified binary release or a claim of 85% / 85% / 65% acceptance.

## 1. Build blocker from the supplied Windows log

The Windows log reached MSVC compilation and stopped in `src/core/SceneDocument.cpp`
with `error C3861: 'setError': identifier not found` in the free function
`validArticulatedRebind`. This was an incorrect reference to the private
`SceneDocument::setError` from an anonymous-namespace free helper.

Correction: the helper now uses `if(error)*error=...` directly. This fixes the
specific visible compiler access defect. Actual recompilation on Visual Studio
18/2026 is required to establish that no further native compiler errors remain.

## 2. Parametric history: full dependent-body preflight

New read-only API: `SceneDocument::previewInterleavedHistoryWithDependents`.
It runs the *existing production* `setInterleavedFeatureHistory` and full
linked-body dependency planner inside a disposable scene, checks changed
B-reps and positive finite volumes, and returns a report without committing
live geometry or undo checkpoints. The history editor invokes this before
staging reorder/removal, and again before the final Save. This catches cases
where a locally valid history would break dependent linked bodies.

Scope: an integrated, bounded dependency preflight for supported interleaved
histories, not universal stable topology naming through arbitrary face splits.

## 3. Assembly: clearance policy checked at commit boundary

New API: `SceneDocument::commitLiveArticulatedCoordinatesChecked`. It validates
requested coordinates and nominal interference/clearance, optionally previews
every motion sample using the same graph and OCCT regeneration path, then
performs one atomic drive commit only if policy checks pass. The GUI batch-drive
editor calls this API after the user's review, so bypassing its preview dialog
cannot skip the safety policy. The existing lower-level unguarded API remains
available for workflows that intentionally do not enforce collision policies.

This remains a sampled geometric check, not certified continuous-motion safety.

## 4. Drawings and sheet metal: one-source development export set

New read-only API: `SceneDocument::buildLinkedFlatDevelopmentSet`. It checks the
saved folded-source/flat-blank provenance, produces SVG, DXF and CSV together
from committed K-factors, bend stations and relief settings, cross-checks the
SVG/DXF physical development dimensions, and updates all three caller buffers
only after successful validation. The existing export dialog now delegates to
this API and writes the selected artifact using `QSaveFile`. No manual re-entry
of bend factors is needed in this workflow.

This does not simulate forming springback or certify shop manufacturing.

## 5. Tests and evidence

- Python/source tests, including independent OCC reference tests: **477/477 PASS**.
- GCC portable C++ targets: **16/16 PASS** (unsanitized).
- Clang portable C++ targets: **16/16 PASS**, with address/undefined-behavior sanitizers.
- Additional native regression assertions added to the existing interleaved
  history, articulated assembly scene, and full CAD core test binaries. These
  have **not run** on Windows here.
- The complete Windows Visual Studio / Qt / OpenCascade application build:
  **NOT RUN for this source package**.
- Native CTest inventory: **130 expected**, none newly verified for this package.
- Formal acceptance scenarios: **0/60 recorded as verified** in the prior
  gate (due to missing native and scenario evidence), which is not a failure
  rate for application features.
- Previous development estimates, retained provisionally: Parametric **76%**;
  Assembly **83%**; Drawings/sheet metal **68%**. These are not acceptance scores.

## 6. Windows verification procedure

Extract to an empty directory. On Windows PowerShell with VS 2026 and
`C:\vcpkg`, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

If native compilation fails, capture the **first MSVC `error Cxxxx` line** and
the next error context. If compilation passes, the script expects all 130
Release CTests and a populated, evidence-backed `build/V170_ACCEPTANCE_RESULTS.csv`.
Release approval requires >=17 of 20 passing acceptance cases in two workstreams,
>=13 of 20 in the third, all critical scenarios, and all 130 CTests.

The previously reported `cadcore_v170_interleaved_history` face-machining
fixture remains included and must be rerun. No prior passing installation
should be replaced before native acceptance.

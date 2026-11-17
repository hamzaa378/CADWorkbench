# CADWorkbench v1.70 — completion-focused push (preliminary source)

## Intent

This candidate starts with `CADWorkbench_v170_Checkpoint3of3_ArticulatedFrameMotion_PRELIMINARY_20261108.zip` and keeps that build available as rollback. The 3-checkpoint target was not reached; this version does **not** claim final release completion.

## New actual functionality

- `SceneDocument::previewLiveArticulatedJointRebind` stages a **saved joint index** with different parent/child occurrences from the existing mechanism; a complete articulated solve and B-rep regeneration must pass before producing the read-only report. The original document, joint definitions, undo state and caller report remain unchanged when preview fails. Static fit is inspected for up to 12 occurrences, with explicit limitations beyond that.
- `SceneDocument::rebindLiveArticulatedJoint` validates the exact same member set, rejects self-links, foreign members, disconnected or inconsistent networks, and commits a successful link + its regenerated poses in one checkpoint. A true no-op does not create a revision. Joint type, driven coordinate/limits, axis and zero frame are not silently reset.
- `Assembly → Rebind saved articulated joint endpoints` offers existing occurrence selectors, preview and explicit commit confirmation.
- The existing native articulated scene CTest now includes positive rebind, disconnected-graph rejection, self-link/foreign-member rejection, report nonmutation, undo/redo, save/reopen and subsequent refresh.

**Scope limit:** This is rebind **within the set of saved network members**. It does not create new components, automatically attach to faces/edges, retype joints, or solve arbitrary continuous motion.

## Release-evidence tooling

- `docs/V170_ACCEPTANCE_RESULTS.csv` is a 60-case **template** initially marked `NOT_RUN`.
- `tools/score_v170_gate.py` counts only accepted cases with a real evidence file path, enforces the critical-case roster, checks the 130-test JUnit file for missing/failed/skipped cases, and refuses release unless the 85/85/65 rule is satisfied. It does not substitute for actual UI testing or inspect the meaning of submitted evidence.
- `RUN_V170_RELEASE_GATE.ps1` runs the existing Windows source hash check, MSVC Release build and native CTests, creates a writable scorecard at `build/V170_ACCEPTANCE_RESULTS.csv`, and writes the combined gate summary to `build/v170_acceptance_gate.json`. Editing scores under `build/` does not invalidate the source hash manifest. The runner does not declare success merely from source-compilation or documentation.

## Verification actually performed for this candidate

- 461/461 Python/source-level and independent geometry tests pass, including new rebind contract tests and conservative gate-scorer unit tests.
- The GCC sanitized portable script completed 14/15 existing targets with passes; it timed out while compiling the 15th, which has not been rerun to completion for **this** candidate. The 15th target had passed for checkpoint 3; do not count it as a current-candidate fresh result.
- Native Qt/OpenCascade/MSVC build, GUI interactions, the new native rebind C++ cases, the original interleaved face-machining regression, and all 130 CTests are **not verified** for this candidate.
- 130 `add_test` registrations remain in `CMakeLists.txt`; registration does not mean execution.

## Required completion gate, not achieved

| Workstream | Previous provisional scope estimate | Formal accepted scenarios | Final threshold |
|---|---:|---:|---:|
| Parametric modeling | ~72% | Not scored | 17/20 (85%) |
| Assembly engineering | ~80% | Not scored | 17/20 (85%) |
| Drawings / sheet metal | ~64% | Not scored | 13/20 (65%) |

The new reference-rebinding feature is not counted as an accepted scenario until native and interactive evidence exists. The planning estimates are not release proof. The formal release cannot proceed until the scenarios are scored, all critical cases pass, the original face-machining regression passes, and Windows native CTests pass 130/130. Missing scenarios especially include more general topology identity, large mixed-feature histories, broad hierarchical mate reattachment, and detailed drawing/sheet-metal associativity.

## Run on Windows

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

If this command fails, inspect `build/cadworkbench_release_ctest.xml` and `build/v170_acceptance_gate.json`. After native tests pass, record actual GUI/workflow evidence in `build/V170_ACCEPTANCE_RESULTS.csv`, rerun and review. Preserve the previous Windows-passing baseline until this candidate clears every gate.

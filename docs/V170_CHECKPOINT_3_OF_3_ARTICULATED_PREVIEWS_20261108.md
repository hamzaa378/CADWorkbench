# CADWorkbench v1.70 — checkpoint 3 of 3 (PRELIMINARY)

**Not a final release.** This is a further development checkpoint based on `CADWorkbench_v170_Checkpoint2of3_TopologyJointSheet_PRELIMINARY_20261108.zip`. Keep the last known Windows-tested build unchanged.

## 1. Sampled multi-drive articulated-motion preview

Added `SceneDocument::previewLiveArticulatedTrajectory()` and an opt-in **Preview full sampled motion path** control inside **Assembly → Edit articulated drives together...**. The user sets 2–12 equal intervals between current and target coordinates. For every discrete pose, CADWorkbench reuses the existing complete articulated solver, stages transformed OCCT solids without committing them, and examines exact nominal pairwise fit when the mechanism has no more than 12 participating bodies. A failure at any sample preserves the original document and the previous caller report. The final commit remains a separate, confirmed and undoable operation. Existing joint travel limits and authoring bounds apply.

**Limitations:** linear coordinate interpolation is not a trajectory optimizer and samples are not a proof of swept clearance; collisions between samples remain possible. The static pairwise fit check is skipped above 12 occurrences; the report says so explicitly. This tool does not certify mechanism dynamics, velocity profiles, singularity freedom or safety-critical motion.

## 2. Editable saved joint zero-frame orientation

Existing **Assembly → Edit articulated joint axis, limits and reference offset...** now includes yaw-Z, pitch-Y and roll-X for the persistent zero-reference frame, in degrees. The editor uses a tested matrix conversion (`ArticulatedFrameEuler.h`) with `Rz*Ry*Rx`, rejects skew/reflection/nonfinite inputs, and runs the full staged articulated solve + B-rep preview before a single undoable commit. Existing saved frame rotations are displayed using an equivalent deterministic Euler convention. The persisted joint frame remains a row-major 3x3 matrix, so the scene schema is unchanged.

**Limitations:** as with all Euler coordinate editors, near pitch ±90° the representation is not unique. The conversion picks an equivalent zero-roll convention. This does not add joint endpoint reparenting, new joint types, auto-constraint selection, dynamics or universal motion collision guarantees.

## Verification actually run

- Python/source-level suite: **446/446 PASS**. Six new checks confirm method signatures, staged preview and GUI integration paths. These are source-contract tests, not MSVC integration.
- New C++ frame-math assertions: **199 PASS under GCC and under Clang, each with ASan/UBSan**.
- GCC sanitized portable suite: first **14 targets** passed, followed by the existing articulated-network target compiled and run successfully on its own (**27 assertions PASS**). This gives individual passing results for all **15** portable targets; the one-shot GCC script exceeded its timeout before the last compilation.
- Clang full portable suite: **15/15 PASS**, with ASan/UBSan.
- `CMakeLists.txt` still registers **130 native Windows CTests**. Native tests extended in `tests/articulated_assembly_scene_v170_tests.cpp` for trajectory nonmutation, rejected inputs, editable frame orientation, and persistence.
- **Not executed here:** Visual Studio MSVC Release compile, Qt GUI interaction, OCCT native C++ integration and 130 Windows CTests. In particular the original `cadcore_v170_interleaved_history` face-machining failure needs a passing Windows re-run.

## Completion estimates, NOT acceptance scores

| Workstream | Earlier checkpoint | Current provisional | Required |
|---|---:|---:|---:|
| Parametric modeling | 72% | 72% | 85% |
| Assembly engineering | 78% | 80% | 85% |
| Drawings / sheet metal | 64% | 64% | 65% |

The requested final gate of two workstreams at **85%** and the third at **65%**, plus all mandatory native and end-to-end tests, remains **unmet**. This third checkpoint is deliberately not promoted to a final v1.70 ZIP. Further implementation and Windows test results are required; the three-ZIP planning goal was not enough to close every engineering subsystem.

## Windows acceptance

In a new folder, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

At minimum inspect `cadcore_v170_articulated_scene_transaction`, `cadcore_v170_face_lineage` and `cadcore_v170_interleaved_history`, then require **all 130 native tests passing**. Manually check the articulated editor's yaw/pitch/roll roundtrip, 2/12-interval previews, invalid target rollback, and comparison of preview to an accepted saved pose. Stop and diagnose any failure before increasing implementation estimates.

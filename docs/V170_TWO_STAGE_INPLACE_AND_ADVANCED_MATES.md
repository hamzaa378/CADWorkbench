# CADWorkbench v1.70 — ordered external-sketch modifiers and mate authoring

## Scope

This is a **development increment**, not the completed 50-feature v1.70 release. It extends the latest 116-native-test candidate, without claiming that candidate passed Windows MSVC verification.

### 1. Two ordered in-place sketch operations

An eligible ordinary solid with one existing in-place sketch feature may receive **one additional feature** (Fuse boss or Cut pocket) driven by a **different** external constrained polygon sketch. Operations run in order on the same target body ID. Both source bodies are explicit edges in the cross-body dependency graph, so changes to either sketch regenerate the target and its descendants. Other model bodies are preserved.

The **Edit → in-place associative sketch feature** dialog now provides: edit first, append second, edit second, remove second, remove entire stack. The first modifier remains independently editable. Geometry is rebuilt transactionally from the original base B-rep and both current source sketches, using effective-operation and solid-connectivity guards. Failed operations make no document or undo history changes. Stage deletion/reordering beyond two and feature-tree cohabitation remain unsupported.

Saved v24 `in_place_sketch_feature` JSON records have an optional `follow_up` object with source ID/token, Fuse/Cut operation, depth in mm and normal offset in mm. Legacy one-stage records still load unchanged. Load rejects incomplete, duplicated, missing, stale or invalid follow-up references and verifies the recomputed volume and bounding box against the persisted B-rep before committing. Older CADWorkbench binaries may discard the optional field when re-saving; use **backups** and do not round-trip new documents through older builds.

### 2. Axis/plane assembly mate authoring

The existing **Assembly → Solve assembly frame mates...** dialog now exposes three fully constrained templates:

1. Three corresponding noncollinear world-frame point pairs.
2. One coincident point and two nonparallel **oriented** coaxial axis directions.
3. One coincident point, one oriented coaxial axis, and a nonparallel coincident **oriented** plane.

In the latter two modes, row 1 is an anchor point on each occurrence; rows 2 and 3 are *unit* direction/normal vectors. These apply at the common anchor. Coordinates are current world-frame millimetres. The solver rejects underconstrained or conflicting constraints and does not change the scene on failure. The solved pose and optional rigid parent-child joint are saved; the raw individual mate equations are **not** persistently edited or driven.

### Verification and acceptance

- Source-integrity and legacy preflight suites, plus dedicated `v170_two_stage_and_mate_ui_preflight.py`.
- Portable `LinkedBodyGraph` dependency/impact planner tests under GCC and Clang, including both modifier sources and cycle rejection.
- Portable rigid assembly mate tests under GCC, Clang, and ASan/UBSan, including axis/plane mixed solutions and one-axis rank deficiency.
- Independent OCP/OpenCascade Boolean volume oracle for two consecutive modifiers and source edits.
- Three additional **native** in-place tests: `two-stage`, `two-stage-persistence`, `two-stage-corruption`. Assembly native document regression also covers oriented axes and pose save/reopen.

Windows acceptance must compile and execute all **119 unique native CTests**, and the two new UI modes need manual smoke tests. This environment cannot prove the native Qt/OpenCascade Windows build or dialog behavior.

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 119
```

### Still required for the full v1.70 roadmap

Unlimited editable ordered feature stacks, robust topology identity across arbitrary face/edge splits/merges and rotations, persistent fully constraint-driven assemblies and closed loops, richer sketch constraints, CAM toolpath generation plus replay and post-processing, associative GD&T/drawings, sheet-metal workflows and the remaining 50-item roadmap acceptance gates. None of these are being claimed complete by this increment.

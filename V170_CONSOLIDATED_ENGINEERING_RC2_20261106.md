# CADWorkbench v1.70 — Consolidated engineering RC2

**Status: development source candidate, not a completed v1.70 release.** This
is a continuation of RC1, not a replacement for the last Windows-verified
executable. It focuses on engineering correctness and saved design intent.

## Changes from RC1

### Priority 1 — multi-source feature rebuild planning

`cad::dependency::planReuseWithSources` is a portable C++20 API for
independent external geometry inputs named `@source:<name>`. Each input has a
nonempty, deterministic geometry revision token. A changed sketch/source
invalidates its own downstream branch, recomputes all joins that consume that
branch, and leaves unrelated verified caches reusable. Implicit first-step
features still use `@base` and implicit successors use the previous step.

The planner rejects unordered/duplicate catalogs, unknown external inputs,
non-finite/malformed feature parameters, and inconsistent cache availability.
It does **not** rebuild a B-rep itself, nor is it wired to every interactive
feature history. Callers must supply real geometry revision tokens and commit
OpenCascade results transactionally. The existing legacy planner remains.

### Priority 1 — consistent sketch overconstraint diagnostics

`SketchConstraintDiagnostics.h` identifies *whole* constraint blocks whose
local Jacobian rows add no independent information. The Qt sketch solver
reports those zero-based indices and the canvas presents their 1-based numbers
as an advisory warning after a successful solve. Duplicate or redundant
constraints are **not** automatically deleted; contradictory constraints still
fail through existing residual checks. The rank report is local to the solved
configuration and is **not** a global geometric constraint proof.

### Priority 2 — persisted, explicitly refreshable rigid frame mates

The scene now supports one document-scoped local-frame rigid mate network:
`setLiveRigidMateNetwork`, `refreshLiveRigidMateNetwork`, and
`removeLiveRigidMateNetwork`. The Assembly menu can save local-frame mates for
one selected parent/child pair and refresh or remove the saved network.
The document API can author larger bounded networks (2–32 occurrences).

Input world-frame points and unit axes are captured in the original component
local frames. The existing atomic network solver computes updated placements;
all changes are committed together or rejected. Network definitions participate
in undo/redo, deletion guards, clear/new document, and a strictly validated
optional `live_rigid_mates` record in the v24 native scene format. Invalid
network data must not replace the open document. Re-solving an already
satisfied network does not invent a new geometry revision.

**Important boundaries:** refreshing is explicit, not an automatic response to
all document edits. Anchors are numerical occurrence-local points/directions,
not proven persistent face/edge references. Live articulated joint networks,
multiple simultaneous named networks, and face-driven mate selection are not
implemented. Older v24 binaries may silently discard the optional new record
on re-save. Back up projects and use the same or newer build for editing.

## Verification and release gate

- Independent Python source/reference suite (including OCCT frame examples):
  see `VERIFICATION_THIS_INCREMENT.md` for the final run.
- GCC and Clang portable C++20 with ASan/UBSan: feature/source planner,
  sketch redundancy, Boolean ancestry, linked-body graph, rigid and articulated
  mates; see verification notes for exact counts.
- Additional **native regression source**: local-frame capture, save/reopen,
  repeated refresh, no-op geometry tokens, invalid saved arrays, undo/redo,
  protected deletion, and redundant sketch dimensions. These tests are **not
  executed** in this environment.
- CMake continues to target Win32/MSVC, Qt6, OpenCascade and 130 registered
  native CTests. Linux's Windows-only CMake guard is expected and does not
  establish compilation of `SceneDocument.cpp`, UI, or SketchSolver.cpp.

On Windows (from extracted root):

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Do not promote this source to v1.70 final until the native build, all registered
CTests, save/reopen/migration, and GUI/engineering acceptance pass. The
remaining 50-target roadmap blockers from RC1 still apply: universal topology
identity, fully generalized feature histories, persisted live articulated
assemblies, full GD&T, sheet-metal production validation, CAM, and automated
Windows end-to-end tests.

**Historical increment:** This document describes the earlier linear-only mixed-history candidate. See `V170_ASSOCIATIVE_MIXED_DAG_AND_WITNESSED_EDGES_20261106.md` for the newer branched mixed-DAG mode.

# CADWorkbench v1.70 — continued modeling architecture (130-CTest candidate)

## Basis

Built on the previous 127-CTest source. This candidate is NOT a verified native
Windows release until a Windows Release build and all CTests pass. The former
ZIP remains the rollback baseline.

## 1. Interleaved general feature history

`SceneDocument::setInterleavedFeatureHistory` adds an alternative in-place
history on eligible solid bodies. This *linear* history may alternate any
supported **single-input** ShapeFeatureTree operation with external constrained
sketch additive and subtractive features. Source edits rebuild each stage and
its linked descendants using the shared dependency graph. The original B-rep
is retained for repeatable edits, rollback, and undo. Unlike the former
fixed-order two-layer history, a general modifier may run before, between and
after sketch-driven stages.

Native `.cadscene` saves contain optional `interleaved_history` records with
source IDs, revision tokens, parameters and frozen base geometry. Loading
rejects bad sources, invalid history records, cycles and mismatching saved
solid geometry. Delete protection remains in force. Old clients may drop the
new optional record when they resave; keep a backup.

**Restrictions:** a separate history mode for ordinary solid bodies without
existing feature definitions; 1-40 strictly sequential steps and at least one external-sketch step. Feature graph
branches and arbitrary internal DAG input references are not interleaved in
this mode. No new graphical drag/drop editor yet: document API and native test
fixtures are available for integration. Mixed body cross-references still
require valid source constrained-polygon sketches.

## 2. Curved-face lineage

New conical and toroidal trimmed-surface witness anchors supplement existing
planar, cylindrical and spherical support. Extended descriptors include cone
semiangle or torus minor radius. Legacy cylinder/sphere anchors remain 14
fields; new descriptors are 15. Supported split/merge/translation cases use
unique trimmed-interior witnesses. Ambiguous and nonmatching surfaces fail
closed, rather than guessing.

**Restrictions:** not a universal topological naming system. Arbitrary B-spline
surface evolution, destroyed witness regions, topology ambiguity, and general
rotation-aware tracking still need stable operation history or native CAD
kernel naming abstractions.

## 3. Articulated closed-loop assemblies

`cad::articulated::solveNetwork` supports fixed, revolute and prismatic
connections, driven/undriven joint variables, bounded travel, nonlinear joint
closure iterations, Jacobian-rank checks, and atomic failure behavior. The
`SceneDocument::solveArticulatedAssemblyNetwork` API applies component poses
as one undoable transaction, validates B-reps and roundtrip persistent
Euler placements, and returns solved joint coordinates.

**Restrictions:** 2-24 independent free occurrences, 1-64 connections, up to
12 undriven scalar coordinates; no global dynamics or flexible links. Requires
loops to determine all undriven coordinates, rejects unconstrained mechanisms
and singular joint configurations. Joint definitions are passed to the API and
are not yet stored as persistently live assembly-mate records, but the solved
component placements persist. UI authoring of these loops remains future work.

## Verification

New CTest targets:
- `cadcore_v170_articulated_closed_loop`
- `cadcore_v170_articulated_scene_transaction`
- `cadcore_v170_interleaved_history`

Existing curved-face CTest expanded for cone and torus. Other 127 CTests remain
registered, for a new count of **130**. Portable C++ and source checks are not a
replacement for the native Windows acceptance command:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

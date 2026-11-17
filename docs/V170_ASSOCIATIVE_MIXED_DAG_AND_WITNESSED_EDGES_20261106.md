# CADWorkbench v1.70 — associative mixed DAG and geometric topology references

## Release gate

This is a source-only development increment based on the tolerance-aware v1.70
candidate. **It has not been compiled or run with Windows MSVC, Qt6 and
OpenCascade 8.0.1.** The last user-confirmed native Windows baseline must be
preserved until the Windows acceptance suite completes.

## 1. Mixed associative feature history with explicit branches

The existing `SceneDocument::setInterleavedFeatureHistory` accepts at most 40
operations on an ordinary solid that has no competing feature definition. This
increment adds `InterleavedHistoryStep::inputs`, a list of stable earlier step
IDs, to both ordinary shape modifiers and external constrained-sketch bosses or
pockets. Rules:

- No inputs means the previous stage, preserving older project behavior.
- A single input identifies an earlier stage or `@base` for a branch root.
- A `Shape` node with type `Fuse branches`, `Subtract branches` or
  `Intersect branches` **must have exactly two distinct, earlier inputs**.
  It cannot have numeric parameters or be suppressed. The material effect of
  subtract operations is checked.
- All other shape and sketch modifiers may have zero or one input. External
  sketch sources remain independent body IDs; a sketch stage uses the output
  of its selected feature parent as its target solid.
- Dependencies must appear before the consumer. No forward references,
  duplicate IDs, duplicate parents, reserved `@base` node IDs, self references,
  or missing parents. The last stage is the displayed body result.
- Any source-edit or edit to the graph rebuilds the full mixed history in
  topological order and then downstream bodies using the existing linked-body
  scheduler. Output is committed as a single undoable operation only after
  the whole result is valid. This version does not introduce new incremental
  per-step caches for the mixed mode.
- New explicit-link histories serialize as
  `interleaved_history.type="dag-interleaved-history-v2"`, with `inputs`
  arrays on linked stages. Unbranched histories continue serializing as
  `linear-interleaved-history-v1`; both load with strict validation. The outer
  scene schema remains v24. Corrupted files are rejected without mutating the
  open document.

Example of the new API usage (shown as an excerpt within a calling function):

```cpp
using cad::InterleavedHistoryStep;
InterleavedHistoryStep boss;
boss.id="boss";
boss.kind="SketchFuse";
boss.sketchSourceId=sourceSketchBodyId;
boss.depthMm=5;
boss.offsetMm=8;
boss.inputs={"@base"};

InterleavedHistoryStep left;
left.id="left";
left.kind="Shape";
left.feature.id=left.id;
left.feature.type="Translate";
left.feature.parameters={{"x",-2},{"y",0},{"z",0}};
left.inputs={"boss"};

InterleavedHistoryStep right=left;
right.id="right";
right.feature.id=right.id;
right.feature.parameters["x"]=2;

InterleavedHistoryStep join;
join.id="joint";
join.kind="Shape";
join.feature.id=join.id;
join.feature.type="Fuse branches";
join.inputs={"left","right"};

QString error;
if (!document.setInterleavedFeatureHistory(targetBodyId,
        {boss,left,right,join},&error)) {
    // Report error and keep the previous model unchanged.
}
```

### Restrictions

The API still requires at least one external constrained-sketch stage and an
ordinary solid target with no competing feature definition. Supported shape
operations are those in `ShapeFeatureTree`; arbitrary inter-body feature types,
fully free-form cross-document references, a full graphical node editor and
per-node mixed DAG incremental caching are not implemented. A compact
**Edit → Mixed associative feature history** dialog now supports adding and
editing live sketches, translations, Z-rotations, uniform scales, and two-input
Boolean branch operations, as well as reordering/removing steps. The full
ShapeFeatureTree operator catalog remains accessible through the document API,
not the compact dialog. No guarantee is made that every
valid conceptual B-rep operation will succeed in the CAD kernel.

## 2. Persisted edge references through supported topology changes

`captureStableEdgeAnchor` now emits v2 **witness** data for straight and
circular edges: a point in the trimmed curve interior, a tangent, curve type,
curvature, and the existing body-relative reference center. Retained older
centroid and length descriptors remain present. New anchors are stored in the
same numeric feature parameter map and consumed by selected-edge fillet and
chamfer operations.

`resolveStableEdgeAnchor` checks for a unique matching trimmed interior
successor on the same curve type and compatible support geometry. This permits
supported edge splits/merges, even when the original edge length/index changes,
and rigid translations. It probes the saved world-space witness first to avoid
wrongly following center-of-mass changes caused by material edits, then a
body-relative translated witness. A witness on a split boundary, deleted
edge, or two coincident candidate edges **fails closed** and requires a new
selection; it must never choose an arbitrary traversal index.

Legacy four-field and seven-field centroid/length anchors remain readable.
Unsupported edge curve types still use the older hints; this is not a global
identity algorithm for B-spline curves, changing radii, major shape rotations,
nonrigid deformations, or wholly removed surface regions.

### Face references

Planar and cylindrical/spherical/conical/toroidal witness matching remains.
Curved faces now try the unchanged witness before a body-relative compensated
one: an edit that changes the solid centroid without moving the face is less
likely to break a still-valid reference. Unique-successor and ambiguity
checks remain mandatory. No universal B-spline face naming has been delivered.

## Regression and verification

The existing **130 native Windows CTest targets remain registered**. The
`interleaved_history_v170_tests.cpp` suite adds fork/join history, edits to a
source that feeds two branches, forward/cyclic or duplicate-parent rejection,
transactional rollback, save/reopen, and corrupted file rejection.
`face_lineage_v170_tests.cpp` adds edge witness creation, split successor
matching, exact-witness-boundary rejection, large translation, legacy anchors,
full-circle -> partial arc continuation, and deleted-arc rejection.

The separate Python `v170_dag_edge_kernel_reference.py` probes the underlying
OpenCascade geometry projection and split behavior. This is **not** a run of
the C++ integration implementation. The independent C++ dependency graph
planner is compiled with GCC and Clang under sanitizers where available.

### Windows acceptance

From a clean PowerShell in the extracted source directory:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Then use **Edit → Mixed associative feature history (branches + sketches)**
to create a branched mixed feature history, edit the source sketch,
check both branches, save/reopen, and verify a selected-edge fillet/chamfer
remains attached after inserting an upstream splitter or operation. Confirm
ambiguous selections report an error instead of switching to an unrelated
edge. Promote the build only if native tests, UI checks, and persistence tests
all pass.

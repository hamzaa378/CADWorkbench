# CADWorkbench v1.70 — Real-world edge finishing development increment

## Baseline and classification

- Source baseline: **PersistentGeometry_WinCTest47_Fix_PRELIMINARY_20261116** (the corrected Windows source the user tested).
- That baseline passed **130/130 native Windows CTests** and **725/725 Python/reference tests** in the user's Windows audit. The 60 manual acceptance scenarios remain unrun.
- **This new increment is PRELIMINARY**. Its new Qt/OpenCascade C++ code and newly extended native assertions must pass MSVC and CTest on Windows before promotion.
- No existing source, project, or installed binary is modified by extracting the full-source archive. The separate guarded upgrade checks the exact baseline and creates a rollback backup.

## Functional development

### 1. Edited, witnessed multi-edge fillet

`Edit > General feature tree for any body > Add feature > Fillet selected edges` accepts **2 to 32 distinct selected edges** from the same currently visible solid. Each selected source edge is identified by an independent *geometric witness* rather than its unstable traversal index. A single feature history node stores the group and permits editing each edge's radius.

- Input: 2–32 selected edges on one current body, positive finite edge radii.
- Output: one editable operation whose input and output are complete B-reps, not a temporary preview-only shape.
- Save, reopen, undo, redo, suppression, and bounded geometric re-resolution follow the existing general feature-tree architecture.
- Ambiguous, stale, duplicate, unsupported, or trimmed-away source witnesses are rejected; no automatic nearest-edge substitution.
- Current implementation is **not** universal naming after arbitrary Boolean edits or topological splits, and it does **not** provide a full curvature-continuous variable-radius fillet along a *single* edge. Radii are constant along each selected edge, and may differ across edges.

### 2. Edited, witnessed multi-edge chamfer

`Edit > General feature tree for any body > Add feature > Chamfer selected edges` uses the same 2–32-edge witness mechanism. Distances can differ between selected edges. The OpenCascade operation remains the authority for actual geometric validity. This is a constant-distance chamfer per edge, not a distance-angle or asymmetric per-edge chamfer command.

### 3. Atomic multi-part finishing job

`Edit > Batch finishing of selected bodies (atomic fillet/chamfer)...` operates on **2–16 distinct selected independent solids**.

- Select all-edge fillet or all-edge equal-distance chamfer and specify a common engineering dimension.
- Each target gets its own persisted stable-ID feature node; it remains editable in the general feature tree.
- Rebuilds all proposed target bodies and their linked downstream results in a separate *staging document*. If any solid or descendant is invalid, **nothing** is committed to the actual scene.
- A passing group commits once, giving a single Undo/Redo step. A repeated proposal with no actual changes is a no-op.
- Component occurrences and linked-derived bodies retain their existing source-edit restrictions.
- Intended for finishing multiple independent manufacturing parts; it does not automatically propagate part-definition edits across all assembly occurrences.

## Architecture and validation

- Core changes: `SceneDocument::setShapeFeatureTreesBatch`, group-aware feature validation and OCCT fillet/chamfer execution in `ShapeFeatureTree`, lineage classification in `BooleanReferenceLineage`.
- UI changes: new grouped feature-tree modes and atomic batch command.
- Regression: extended existing `shape_feature_tree_tests.cpp` (still one existing CTest target) for geometry, asymmetric per-edge sizes, malformed/duplicate references, edit/rebuild, save/reopen, undo/redo, batch atomicity and invalid finish sizes.
- Independent OCP Python geometry references and new source integration contracts. No native tests removed or skipped.
- The new group operation deliberately fails closed when an upstream edit consumes or ambiguously replaces its witnesses. A successful operation is not evidence of structural fitness for safety-critical use.

## Windows verification instructions

Open PowerShell in a fresh extracted **full-source** folder and run:

```powershell
py -3 .\tools\preflight_major_build.py --root .
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot C:\vcpkg -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

The required Windows-native result is **130/130 CTests** and the updated Python/reference count. Run manual checks: select two non-adjacent edges, give them different sizes, preview and apply, edit, save/reopen, undo/redo; then batch-finish two boxes, deliberately include one impossible body, verify **zero** partial commits, and export a STEP before/after comparison.

## Known risks and remaining engineering work

1. New C++ integration has not yet been compiled with the user's **MSVC 19.51 / Qt 6.11 / OpenCascade 8.0.1** toolchain.
2. Fillet/chamfer feasibility is kernel- and shape-dependent; near-degenerate slivers and close radii can be rejected.
3. Source witnesses can fail or become ambiguous after severe Boolean splits/merges, large deformations, or deleted edges. Preserve explicit rebind as a future task.
4. Actual multi-body manufacturing operations do **not** generate tool paths, fixtures, tool catalogs, or certified GD&T.
5. Manual engineering acceptance remains 0/20 per workstream until checked on Windows GUI; the final release gate remains unchanged.

## Suggested next larger upgrades

- General persistent topology naming for strongly modified B-reps, with designer-approved rebinds.
- Real subassembly ownership and transformations, geometry-linked mates with motion and clearance checks.
- Drawing revision issue workflow and linked feature inspection details for manufacturing.
- End-to-end GUI evidence and native regression promotion before a downloadable verified clean-install release.

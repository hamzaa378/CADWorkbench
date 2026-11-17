# CADWorkbench v1.70 — mixed-history, curved-face and closed-loop increment

## Change basis

This candidate is a modification of `CADWorkbench_v1_70_Ordered_Eight_Stage_InPlace_Development_20261106.zip` (the 123-CTest source). The prior ZIP remains untouched. No native Windows result for this or the previous candidate has been supplied during this increment.

## 1. Mixed feature history: sketch stages followed by general features

A body with up to eight ordered externally linked constrained-polygon bosses/pockets can now **also** have a shape-independent feature tree. Each such shape-tree node uses the completed sketch stack as its base. Existing general feature operations include transforms, pattern operations, cuts, face operations, loft, sweep, and revolve. This is the first supported mixed geometry pipeline **on one body ID**.

- When a source sketch changes, the dependency-ordered regeneration reevaluates the sketch chain, then rebuilds all general modifiers against the new geometry. A failed later modifier rejects the entire candidate and retains the committed scene.
- Feature-cache data is discarded/recomputed when the upstream sketch result changes, rather than reused with the wrong base.
- Save/reopen verifies both layers: the rebuilt external-sketch result against the shape tree's stored base, followed by the shape-tree result against the saved final body.
- Removing the external sketch stack rebuilds the downstream general modifiers over the original base instead of silently dropping the general tree. Undo/redo is covered.

**Limits:** the supported order is external sketch stages **then** the general feature tree. Arbitrary interleaving between these types, unlimited stage counts, and direct external sketch references as individual shape-feature nodes are still not implemented. Existing feature-tree node limits and surface/solid restrictions remain.

## 2. Curved-face witness lineage

`captureCurvedFaceAnchor` and `resolveCurvedFaceAnchor` support *trimmed cylindrical and spherical surfaces* using a radius/support descriptor, an interior point on the actual curved face, and a body-relative center. These anchors do not rely on face traversal indices or face area and can identify an unambiguous successor through certain splits, merges and translations. Missing/duplicated matches fail closed rather than silently selecting a different surface.

**Limits:** this API is a foundation for further curved-face feature references; existing planar face-attached modeling commands are not automatically converted to cylindrical attachments. B-splines, tori, non-rigid deformations, rotation-equivariant matching, removed witness regions, and ambiguous successor faces remain unsupported. It is not a fully general persistent naming service.

## 3. Closed-loop rigid assembly network

`cad::mates::solveRigidNetwork` accepts 2–32 nodes and 1–128 edges. Each edge uses the existing fully constrained 6-DOF point/axis/plane solver to establish a relative frame. A spanning traversal proposes poses; **all** edges (including non-tree closure constraints) are checked independently to 0.0001 mm. Disconnected, inconsistent, or underconstrained edge systems reject without overwriting the result.

`SceneDocument::solveAssemblyMateNetwork` applies the solved corrections to free component occurrences in a single staged document transaction. It checks B-rep validity, world-placement bounds, persistent Euler reconstruction, then commits one undo/redo entry. Positions survive save/reopen through the existing component placement format.

**Limits:** this is a complete-rigid-connection network solver, including consistent rigid closed loops. It is **not** a general articulated multi-body solver: partially constrained revolute/prismatic networks, flexible bodies, dynamics, redundant constraints under numerical noise, and simultaneous nonlinear global joint optimization still need more work. Raw constraint definitions are not persisted as live mate records, only the solved placements.

## Regressions

- Portable GCC / Clang / ASan+UBSan: `tests/assembly_constraint_network_v170_tests.cpp`.
- Windows native: `tests/assembly_constraint_network_scene_v170_tests.cpp` (atomic placement, closed loop, persistence and rollback).
- Windows native: `tests/curved_face_lineage_v170_tests.cpp` (cylinder split, translation, sphere anchor, rejection).
- Windows native: `tests/mixed_feature_history_v170_tests.cpp` (sketch regeneration through a general node, persistence, undo/remove).
- All original 123 tests retained, new total **127**.

## Native Windows acceptance

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 127
```

The Release compile and all 127 CTests must pass before promoting this candidate to a verified release. Source preflight and independent OpenCascade/Python-oracle checks cannot substitute for the MSVC/Qt integration build.

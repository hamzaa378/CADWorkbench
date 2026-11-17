# CADWorkbench v1.66 — dependency-aware sparse feature regeneration

## Baseline and release scope

The user confirmed **v1.65 Windows Release: 88/88 CTests passed**. The v1.66 candidate builds on that exact source snapshot; existing geometry operations, serialization and body/assembly APIs remain intact. This is an engineering-modeling **foundation** upgrade, not completion of the v1.70 roadmap.

### Implemented features

1. **Dependency-aware reuse beyond prefixes.** Old geometry outputs are mapped by stable feature node ID, not only their position. On an edit, unchanged independent branch nodes may retain their previously evaluated B-reps even if earlier nodes change. Dependents of a modified node are recomputed.
2. **Safe branch reordering.** An explicitly sourced feature can move earlier/later as long as its own definition and effective upstream graph are unchanged and all references remain to earlier nodes. Reuse is preserved despite the different position in the node array.
3. **Implicit-sequence invalidation.** A feature with no declared inputs depends on the preceding feature; if its predecessor changes, cache reuse is denied. Boolean nodes depend on *both* explicit branches.
4. **Fail-closed dependency planning.** The portable planner rejects invalid/duplicate identities, missing/later/self-references, duplicated inputs, non-finite parameters, inconsistent cache lengths, and null outputs.
5. **Transactional CAD integration.** SceneDocument only authorizes reuse when the feature tree's geometry revision is current and the old output cache has the correct length. Each reused B-rep is validated before use; the operation commits only after geometry and linked-body propagation succeed. Failure leaves the old model, undo history and cached outputs untouched.
6. **Feature-tree UI feedback.** The existing general feature-tree editor displays planned reused versus regenerated node counts before Save. Dry-run geometry validation still performs a **full deterministic rebuild** without committing changes, so the two paths can be compared.
7. **Portable planner tests.** The pure C++20 dependency planner compiles and runs independently of Qt/OCCT, covering parallel branches, edits, suppression, insertions, deletions, implicit dependencies, safe reorderings, malformed references, and large independent branch graphs.
8. **Native geometry regression.** A new CTest exercises actual `SceneDocument` branch fusion and verifies independent output reuse, fresh rebuild volume equivalence, order changes, invalid-input rollback, undo/redo, and persistence after save/reopen.

### Where to use it

Create or open a solid. Choose **Edit → General feature tree for any body**. Add multiple features whose inputs refer explicitly to the frozen base or earlier IDs. Edit one branch and inspect the rebuild summary: changes to that branch and descendants are reevaluated, but unaffected sibling branches can be reused. Select **Validate staged geometry (dry run)** to evaluate the full graph independently before saving.

### Technical limits

- The reuse plan is *within one body's generic shape-feature stack*. Cross-body linked Booleans are still recomputed by their existing transactional propagation mechanism; this is not a global dirty-subgraph engine.
- Reuse relies on stable feature IDs and identity of type, numeric parameters, suppression state, effective inputs and the unchanged base geometry. It does **not** provide topological face/edge identity when upstream geometry splits or merges.
- Geometry cached by `TopoDS_Shape` remains **transient**: no kernel handles are serialized. Saved files keep the existing scene schema **v24** and rebuild caches on reopen.
- Selection-dependent features may still require manual face/edge reselection after destructive topology edits. Any ambiguity should fail instead of modifying another face.
- Reordering a feature with sequential inputs can invalidate downstream caches even when the visual result looks similar; this is conservative and intentional.
- Performance improvement depends on the fraction of independent branches. A strictly sequential feature history still rebuilds its descendants after a modification.
- Native MSVC, Qt6 and OCCT compatibility of this new v1.66 candidate must be confirmed by the user's Windows Release build.

## Windows acceptance

From freshly extracted source in PowerShell:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 90
```

Targeted tests:

```powershell
ctest --test-dir .\build -C Release --output-on-failure -R 'cadcore_v166_sparse_dependency_planner|cadcore_v166_sparse_feature_regeneration|cadcore_v165_incremental_feature_graph'
```

Expected **90/90 native tests**. Preserve the last Windows-verified v1.65 installation until the new candidate passes.

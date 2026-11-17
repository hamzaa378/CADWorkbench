# v1.65 — incremental feature graph and modeling operations

## Changes since the user-verified v1.64 (87/87 tests)

### Feature graph execution

- Feature stacks retain transient, per-node OCCT output shapes (`Part::shapeFeatureCachedOutputs`).
- When changing an existing stack, `SceneDocument::setShapeFeatureTree` compares stable node IDs, operation names, inputs, suppression flags, and numeric parameters. The longest identical leading sequence is reused; the modified node and all later nodes are reevaluated.
- This also works when a later node reads an explicitly named earlier branch, because reused outputs are restored into the executor's stable-ID map.
- Rejected geometry changes leave the current part, cache, linked Boolean dependents, and undo state intact. The previous source-based rebuild path is still used when no trustworthy cache exists.
- Save files continue to use the **v24 scene schema**. The cache is in-memory only and is regenerated on scene reopen and when an editable sketch upstream changes.
- **Scope limit:** This is prefix-only selective rebuilding within *one part's* feature tree. It is not a minimal dirty-subgraph solver for arbitrarily ordered DAG nodes or a global multi-body dependency engine. The dry-run button still does a full deterministic build.

### New editable feature-tree operations

In the existing feature-tree dialog, choose one of:

1. **Rotate arbitrary axis:** angle (degrees), axis direction X/Y/Z, pivot X/Y/Z (millimetres).
2. **Mirror arbitrary plane:** normal X/Y/Z, a point on the plane X/Y/Z (millimetres).
3. **Linear pattern vector:** copy count 2–24 and vector offset per copy X/Y/Z (millimetres). Produces a multi-solid compound, **not a fused result**.
4. **Circular pattern arbitrary axis:** copy count, total sweep (degrees), normalized axis direction and pivot in millimetres. Full 360° divides sweep by count to avoid duplicate final instances; partial sweeps include endpoints.
5. **Intersect branches:** pick two earlier feature-node IDs or the frozen base. Uses OCCT `BRepAlgoAPI_Common` and rejects an empty or invalid overlap.

All features use the existing stable node ID, numeric parameter, suppression, branch input, undo/redo and persistence machinery. Zero direction vectors are rejected before kernel execution. Bad geometry never commits.

## Native acceptance

Run from a **freshly extracted** source tree in Windows PowerShell:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 88
```

Targeted newly added native CTest:

```powershell
ctest --test-dir .\build -C Release --output-on-failure -R cadcore_v165_incremental_feature_graph
```

### Manual UI acceptance

Create a box, open its shape-independent feature tree, add an arbitrary-axis rotation and custom-plane mirror, and confirm their dimensions can be edited. Test a vector pattern and a circular pattern around a non-Z axis. Create two explicitly branched translation nodes, combine them with Intersect branches, and edit the second translation: the overlap must update. Undo, redo, save, reopen, then edit again.

## Unfinished v1.70 targets

Persistent topological naming (split/merge tracking), arbitrary sketch-profile revolve/sweep/loft as graph nodes, cross-part minimal dependency reconstruction, full assembly solver, drawing associativity, and CAM toolpath generation remain outside this release. No newly generated G-code or unattended fabrication operations have been added.

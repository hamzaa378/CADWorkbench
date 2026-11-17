# CADWorkbench v1.60 — selected-edge history and linked edit consistency

## Baseline and verification

- Based directly on the full v1.59.1 source snapshot supplied in this conversation.
- The user reports that **all 77/77 Windows native tests passed on v1.59.1**. This is the accepted previous baseline.
- This v1.60 source adds **one** native CTest, `cadcore_selected_edge_v160`, bringing the expected total to **78**.
- Existing Python source-contract suites and the new v1.60 source preflight can be run without Windows; these are **not** a substitute for the native Windows Release build and CTest.

## What changed

### 1. Editable selected-edge fillet and chamfer nodes

The general feature-tree editor now offers `Fillet selected edge` and `Chamfer selected edge`. These act on **one topologically distinct edge**, not on all edges. They retain their operation ID, dimensions, suppression state and dependency inputs like other feature-tree nodes. Both support save/reopen and are included in undo/redo and downstream feature replay.

When exactly one edge of the target body is selected before opening the feature-tree dialog, the editor translates the viewport's face-exploration edge index to the topology-unique index used by `captureStableEdgeAnchor()`. If the selected-edge feature is appended to the end of the current stack with that edge index, it captures a geometric anchor (centroid and length). The editor shows `[edge anchor]` in the feature tree without filling the feature row with internal anchor numbers.

**Limit:** This is geometric matching, not complete persistent topological naming. If an upstream edit moves or changes the edge too far or creates indistinguishable candidates, the feature fails closed and requires the user to reselect the edge. Numeric-only `edge_index` features remain susceptible to topology reordering. Current capture occurs for **append-at-end** operations, where the viewport refers to the input geometry being modified.

### 2. Distinct-edge enumeration

Both the generic feature-tree's all-edge operations and direct `SceneDocument::filletAllEdges` / `chamferAllEdges` now call `TopExp::MapShapes(..., TopAbs_EDGE, ...)`, so each topologically distinct edge is added to the OCCT builder exactly once. Traversing face/edge occurrences could otherwise add the same underlying edge multiple times.

### 3. Transactional linked-body updates for direct fillet/chamfer

Direct all-edge fillet and chamfer previously modified the source without invoking linked Boolean dependent rebuilds. Both now stage the edit in a copy, run `propagateLinkedBodies`, and commit a single undo checkpoint **only** after every dependent rebuild succeeds. A failure leaves the original bodies and dependency tokens unchanged. Both commands reject editing a derived Boolean body directly; modify source bodies instead.

The single-edge preview/commit paths (`prepareFillet`/`commitFillet` and `prepareChamfer`/`commitChamfer`) have the same transactional downstream propagation and derived-body rejection. A stale preview token still blocks commitment.

**Limit:** This update wires four additional direct-edit paths into the existing propagation pipeline, not every legacy modeling tool. Linked derived bodies remain read-only and restricted to Boolean dependencies.

## Verification

On Windows, extract the ZIP and run:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
PowerShell.exe -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
```

Focused tests:

```powershell
ctest --test-dir .\build -C Release --output-on-failure -R "cadcore_selected_edge_v160|cadcore_associative_features_v159|cadcore_universal_feature_tree"
python .\tests\v160_source_preflight.py
```

### Manual UI checks

1. Create a box, switch to edge selection, and select one visible edge.
2. Open **Edit → General feature tree for any body**, then append **Fillet selected edge**. The selected edge should be the default, with an edge-anchor label in the tree.
3. Save, reopen, edit the fillet radius, and undo/redo. Repeat with selected-edge chamfer.
4. With two overlapping source solids, create a linked Boolean body. Fillet or chamfer the *source* through the direct all-edge tools and confirm that the linked result stays current. Verify that direct edits of the linked result are rejected.
5. Deliberately make an edge reference ambiguous or invalid. The operation should fail without replacing the saved geometry.

## High-impact follow-ups

- A durable OCCT history-based topology naming system with edge/face splits, merges and persistent naming through arbitrary upstream edits.
- Arbitrary sketch profiles and edges feeding revolve, sweep and loft feature nodes instead of current numerical shape templates.
- Consolidated cross-body dependency graph for Boolean, sketch-to-face, and derived-reference workflows.
- Selective dirty-node recompute/cache and a traceable rebuild diagnostic inspector.
- Full interactive constraint solve-status, dimensions and sketch-level construction geometry.

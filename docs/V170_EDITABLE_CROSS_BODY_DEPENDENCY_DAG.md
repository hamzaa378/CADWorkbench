# v1.70 development increment: editable cross-body dependencies

This increment builds on the latest **v1.70 body-relative geometry anchor candidate**. The most recently **user-confirmed Windows baseline** is the 94/94 advanced-profile increment; the two 95-test face/anchor increments were not confirmed as passing when this work began. The archive retains the earlier features and requires a **98/98 Windows acceptance run**.

## Implemented

1. **Edit an existing linked Boolean result** without deleting/recreating it. Change `Fuse`, `Cut`, or `Common` and choose a different primary body and tool body. The result retains its identity, name, material metadata, and downstream references.
2. **Source-by-ID dependency ordering**: linked results may refer to later independent bodies in the document. A pure C++ topological planner validates the graph before any B-rep rebuilding.
3. **Transitive regeneration**: changes in a source propagate to all linked descendants, including documents whose insertion order differs from dependency order.
4. **Transactional updates and rollback**: malformed references, dependency cycles, and invalid or empty Boolean results abort the entire staged edit. The current source and downstream models remain unchanged.
5. **Save/reopen support** for legal out-of-order graphs, using the existing `linked_boolean` record (no file schema migration required). Saved cycles are rejected before replacing the open document.
6. **Rebuild-impact preview**: select a body and choose **Modify → Preview associative rebuild impact** to see which results depend on that body.
7. **Dependency inspector**: **Modify → Inspect associative body dependencies** shows inputs, operation types, and current/stale link tokens.

## Using the editor

1. Create a few solid bodies and **Modify → Associative union/subtract/intersection**.
2. Select an existing *linked result* (exactly one body) in the model browser.
3. Open **Modify → Edit associative Boolean result...**.
4. Select an operation and two different solid sources; for subtraction, `A - B` uses the `Primary body (A)` and `Tool body (B)` selectors.
5. Confirm. CADWorkbench validates the dependency DAG, regenerates affected linked bodies, and creates a single undoable checkpoint. A failed operation leaves the scene unchanged.

## Implemented test coverage

* 41 deterministic portable graph and impact checks under GCC and Clang, including out-of-order graphs, branching, cycles, missing/duplicate inputs, and 250-node graphs.
* New native CAD regression `cadcore_v170_linked_body_edit` checks retargeting and subsequent indirect propagation.
* New native CAD regression `cadcore_v170_linked_body_persistence` tests save/reopen and corrupted cyclic records.
* `cadcore_v170_linked_body_graph_planner` is a third new registered CTest, for **98 native CTests** total.
* Source-contract suites and independent OpenCascade reference geometry validation run separately on Linux. These are **not substitutes for compiling the full Windows application**.

## Limitations

This is an **editable associative Boolean-body DAG**, not yet a universal associative CAD graph. Features on one body cannot automatically use external body sketches or named faces as input, and assembly joint constraints remain separate. Imported STEP/IGES bodies are usable Boolean sources if they are valid solids, but they do not acquire inferred editable feature histories. The geometric anchors from the earlier increment still do not resolve topological splits or merges. No automatic automatic assembly mate solver, collision-free CAM toolpaths, or stress FEA is claimed.

## Windows acceptance

From the extracted source root:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 98
```

The previous verified source should be retained until all 98 tests pass and the graphical edit/undo workflows are manually inspected.

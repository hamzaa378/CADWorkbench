# v1.70 development increment: atomic editing of linked Boolean bodies

## Baseline

Derived from `CADWorkbench_v1_70_Editable_Cross_Body_DAG_Source_20261105.zip`, which registered **98 Windows native tests**. User has not yet confirmed those tests in this conversation. No source baseline files were dropped. The version number remains 1.70 because this is an increment on the v1.70 roadmap, not a completed release.

## Implemented

* **Edit multiple associative results (atomic)** under Modify opens a table of saved linked Boolean result bodies. The operation and both sources are editable per row; source and result body identities are preserved.
* **Graph-first validation** verifies every proposed source reference and checks cycles on the *final* combined graph. Changes that temporarily create a cycle when applied singly can be staged together if the final graph is acyclic.
* **Transactional geometry rebuild** evaluates all affected results in topological dependency order against a staged copy, with one undo checkpoint only after every Boolean succeeds. A failing Boolean, cycle, duplicate target, invalid input, or missing source leaves the committed document unchanged.
* **Read-only impact preview** reports changed results and their transitive dependents in dependency order without computing geometry or modifying the scene.
* **No-op suppression** avoids geometry builds and undo-history entries when all operations and references remain the same.
* **Save/reopen** uses the existing v24 scene representation. No schema migration or extra dependencies are introduced.
* The original single-result editor delegates to the same transactional engine.

## Limitations

This remains a **Boolean-only** cross-body DAG. It does not yet provide associative cross-body sketches, user-defined assembly joint solving, face/edge lineage across splits/merges, or universal multi-body feature nodes. Impact previews detect graph reachability, not whether a Boolean will succeed: the final commit still performs full kernel validation. A source body's geometry is never modified by a linked-result batch edit.

## Test inventory and acceptance

* `cadcore_v170_atomic_batch_edit`: coordinated retargeting, transitive rebuild, undo/redo, no-op, invalid/cyclic/missing or geometry-empty rollback.
* `cadcore_v170_atomic_batch_preview`: read-only graph planning and ordered impacts.
* `cadcore_v170_atomic_batch_persistence`: save/reopen and dependency impact after load.
* The portable `linked_body_graph_v170_tests` includes batch impact and failure-isolation assertions.
* Additional `tests/v170_atomic_batch_preflight.py` checks build registration and UI/source integration.

**Expected Windows native test count: 101.** This source was not natively compiled with MSVC/Qt/OpenCascade in the development environment. Perform acceptance on a clean extraction:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 101
```

Keep the last passing Windows baseline until this command reports all 101 tests passing.

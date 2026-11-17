# CADWorkbench v1.70: Shared-dependency and engineering-review increment (preliminary)

This is an **unreleased development candidate**, built from `CADWorkbench_v170_ThreeTrackEngineering_PRELIMINARY_20261110`. It is not the earlier Fix 2 Windows-tested baseline. The three implemented changes are:

## 1. Model regeneration and architecture

The dependency graph previously existed as three near-identical node-building loops in `SceneDocument.cpp`: the primary regeneration planner, selected-body impact, and atomic batch-Boolean preview. A single internal `linkedBodyGraphNodes` routine is now reused by all three. It covers Boolean, linked sketch, in-place sketch stages (including additional stages), interleaved history, and dependent folded flat blanks. New feature families should be added to this single mapper.

New portable `linkedgraph::explainAffectedMany` returns **source-to-descendant causal paths** in stable topological rebuild order, preferring shortest chains and deterministic ties. The existing UI's *Associative rebuild impact* preview now shows the reason for rebuilding each body, not just its ID. Graph preflight and cause reporting are read-only; normal modification remains transactional with rollback.

## 2. Assembly reliability review

`AssemblyResiliencePlanner.h` contracts nonbridge connections into 2-edge-connected blocks and pairs leaf blocks in the resulting bridge tree. It proposes graph edges that would remove single-link isolation within the already-connected fixed-frame component. Each proposed link is simulated and re-audited, and only a result with **zero remaining fixed-frame bridge links** is returned. The existing rigid-mate audit shows the proposed occurrence pairs and clearly labels them as **nonautomatic engineering candidates**. The planner does not create mates, evaluate fit, stiffness or load paths, or solve kinematics. Disconnected occurrences are separately reported and are not automatically attached.

## 3. Drawing measurement review

The linked-drawing dimension audit previously reported only current/stale/missing/broken. It now obtains the same **0.001-mm-rounded nominal** that the SVG exporter renders, by reading its `selected-edge-dimension` metadata. It exposes the current nominal and a **user-entered tolerance interval**, for both CURRENT and REGENERATE_REVIEW annotations; missing or invalid geometry never gets a numerical value. The report UI and CSV include those values. The drawing is not regenerated or marked reviewed by an audit; explicit preview and engineer acceptance remain required.

## Verification and limitations

- Python/source/reference suite: **592/592 passed** on the local development source (and rerun against the extracted archive).
- Portable C++: **17/17 passed** with `python tools/run_portable_regressions.py --compiler g++`, including the graph-cause suite.
- Native CTest names: **130** preserved, with added assertions in existing native targets. Windows MSVC/Qt/OpenCascade compilation and execution of this candidate remain **pending**.
- Existing accepted engineering GUI cases: **0/20 per workstream** (NOT_RUN, not FAIL). Never mark these passed from source checks.
- No scene schema or persistent format change. No automated mate insertion or drawing revision acknowledgement.

### Windows audit (in extracted source root)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot C:\vcpkg -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

An audit exit code of `3` is expected only if the **new** Windows native and Python checks pass and manual acceptance is still pending. Exit `1` indicates automated verification failure; it must not be reclassified.

### Workstream progress

These updates meaningfully advance P (shared regeneration causality), A (graph resilience planning), and D (measured linked dimensions), but they are not substitutes for the acceptance scenarios. Carry forward the historical provisional estimates (~85% / ~90% / ~81%) as **unrescored** until a requirement-by-requirement implementation evaluation is performed. Verified GUI acceptance remains 0/20 each. Keep the full release gate unchanged: two workstreams at least 17/20, third at least 13/20, all critical scenarios passed, and Windows native regressions green.

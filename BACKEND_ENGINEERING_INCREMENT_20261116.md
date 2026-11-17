# CADWorkbench v1.70 — Backend Engineering Major Increment (PRELIMINARY)

## Scope and baseline

Base snapshot: `CADWorkbench_v170_DesignExperience2_Backend_PRELIMINARY_20261116_GitHub_Source.zip`.
New snapshot: `CADWorkbench_v170_BackendEngineering_MAJOR_PRELIMINARY_20261116_GitHub_Source.zip`.
**Qt frontend is frozen:** zero files under `src/ui/` are modified. Existing scene-file format and `CMakeLists.txt` registration count (130 native CTests) remain unchanged. This is source only; Windows MSVC/Qt/OpenCascade compile, native regression and GUI acceptance MUST still run. Preserve the previously verified native CTest47 baseline separately (130/130 and 725/725 were verified on Windows for that earlier source).

## Four actual backend developments

1. **Local assembly constraint mobility**: `src/core/AssemblyConstraintMobility.h`. A read-only, bounded, world-coordinate rank analyzer for solved/aligned networks of point, coaxial-axis and planar mates, with signed plane offsets. It reports independent rank, remaining infinitesimal DOF, disconnected occurrences, wholly redundant connections, and potentially mobile nodes. It returns failure on inconsistent geometry and never changes a result on error. **Not** a dynamics, finite-motion, or mechanism singularity certificate. No automatic new mates are created.
2. **Acceleration-bounded quintic motion**: `src/core/{JointMotionInterpolation,MotionDriveLimitReview}.h`. Adds `Progression::QuinticSmoothstep` without altering existing `Linear` or cubic `Smoothstep` behaviour. `planQuinticMotion` derives a shared duration under each drive's speed and acceleration limits (quintic peak factors 15/8 and 10/sqrt(3)), then samples synchronised coordinates. End velocity and acceleration are zero, but torques, path collision, and feasible articulated poses are **not** certified. Existing scene movement methods retain their prior interpolation selection unless the new core API is explicitly called.
3. **Incremental dependency rebuild waves**: `src/core/LinkedBodyGraph.{h,cpp}` exposes `planAffectedWaves`: edited root(s) plus all dependents, grouped by required dependency depth with document-order stability per wave. A descendant explicitly edited along with its ancestor is still scheduled after the ancestor. It reports scheduling **only**; there is no new parallel execution or implicit cache invalidation.
4. **Faster large-graph scheduling**: `linkedgraph::plan` now uses a min-priority queue (rather than rescanning every document body for each step). Worst-case selection complexity falls from quadratic in bodies to O((V+E) log V), preserving the exact index-priority topological ordering. No bypass of cycle, duplicate-source, or missing-body rejection.

## Validation and evidence

- Python/source/reference tests: **771/771 passed** in the working Linux source tree.
- Portable compiled C++ regression programs: **24/24 passed** in the working Linux source tree. Dedicated new suites cover 48 mobility checks, 215 quintic-motion checks and 23 rebuild-wave checks, plus compatibility coverage in existing native targets.
- 130 native CTest names remain registered; native Windows execution of this increment is **PENDING**.
- The independent 60 engineering acceptance scenarios (20 per workstream) remain NOT_RUN. The 95% targets (19/20 per workstream) are targets, not achieved implementation scores.

### Regression commands from source root

```powershell
py -3 .\tools\preflight_major_build.py --root .
python -m unittest discover -s tests -p '*.py' -q
python tools/run_portable_regressions.py --compiler g++   # on Linux/MinGW where g++ is available
python tools/verify_130_tests.py
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot C:\vcpkg -SetupPythonEnv -Parallel 4
```

For Windows, use the last PowerShell command as the full MSVC build plus native/Python audit. It may return exit 3 when automated verification passes but manual acceptance is still pending. Do not interpret source-level tests as having run the new Qt/OpenCascade Windows build.

## New core APIs and behavior boundaries

- `cad::mates::analyzeConstraintMobility(nodeCount,fixedNode,connections,&report,&error)` expects observations that already satisfy the nominal mate geometry; errors for unaligned/contradictory observations. Its `remainingDegreesOfFreedom` is an instantaneous Jacobian dimension; redundant rows can represent harmless mathematical redundancy, not necessarily bad design.
- `cad::joint_motion::planQuinticMotion(drives,limits,sampleCount,&plan,&error)` plans only one shared schedule for all drives, with up to 1024 samples, a maximum one-hour duration and a required valid travel envelope. It never modifies model state.
- `cad::linkedgraph::planAffectedWaves(nodes,changedIds,&waves,&error)` handles independent branches and staged source chains and preserves caller output on invalid requests.

## Upcoming backend work

Wire the mobility review into the geometry-linked assembly backend after pose solving; implement safe incremental geometry regeneration with measured cache hits; validate planned motion against actual articulated poses and continuous-clearance assumptions; and add reliable topology-reference migration after Boolean splits, merges and fillets. All of those require independent native/engineering acceptance beyond this increment.

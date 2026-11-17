# Four feature milestone — CADWorkbench v1.70 (2026-11-10)

**Status: started, not 85% accepted or production ready.**  Four of the ten planned features are active in this increment: F01, F02, F05 and F08. The agreed target is **at least 85% implementation-readiness for EACH feature**; it is a forward target, not a result achieved by source-file edits. F03/F04/F06/F07/F09/F10 are not counted. The 60 separately tracked GUI acceptance scenarios remain NOT_RUN.

## What is implemented in this increment

| ID | Feature | Production code and UI integration | New regression evidence | Remaining critical gaps for >=85% |
|---|---|---|---|---|
| F01 | Persistent planar face references (splits/merges, rotations) | `BooleanReferenceLineage.cpp`: rigid transport now preserves the original interior primary witness and a surviving optional secondary witness; checks the saved machining center remains strictly IN the same trimmed successor, rejecting boundary/removed supports | New native `selected_edge_v160_tests.cpp` assertions for one/two rigid rotations; previous OCCT Boolean/split/merge tests retained; Python integration contract passes | Full Windows CTest, saved multi-step feature edit with face split+merge, complex freeform faces, GUI evidence; no universal topological naming claimed |
| F02 | Persistent edge references (Booleans/rotations) | `BooleanReferenceLineage.cpp`: rotated/translated line, circle, and spline edge rebase carries the original physical edge witness and tangent instead of a recaptured midpoint; existing kernel-provenance checks remain mandatory | New native rotated-spline witness assertion; prior split/trim/boundary and lineage tests retained; source contract passes | Windows CTest and saved chamfer/fillet after complex Boolean splits, generated edges, ambiguous curves, multi-stage GUI run |
| F05 | Planar / concentric / signed-distance mates | `AssemblyMateSolver.h/.cpp`, `AssemblyConstraintNetwork.cpp`, `SceneDocument.cpp`, `MainWindow.cpp`: signed plane offsets in millimeters, positive/negative solve, closure diagnostics, editing, optional saved field compatible with old scenes, rejection/rollback | Portable compiled tests of solver+network pass; new native solver and saved-scene regression assertions; Python source contract passes | Windows compilation and save/reopen CTest, Qt GUI test of plane offset editor, real selected planar/cylindrical geometry binding and DOF handling |
| F08 | Associative drawing updates | `MainWindow.cpp`: command stages **all** saved drawing sheets against current model B-reps, BOMs and saved annotation IDs; checks missing/broken links; renders SVG into memory first; writes unissued preview SVGs and an index CSV to a new directory with rollback; NEVER acknowledges/overwrites approved revisions | Python integrated source contract passes; existing native drawing SVG/dimension and Python reference checks retained | Actual Windows drawing render/GUI run, selected real dimension update evidence, reviewed issue/approval workflow, PDF output and large-sheet stress tests |

## Explicit 85% feature-level acceptance targets

Each feature must pass 17 of 20 feature-specific checks (5% each), **including every critical check**. This is separate from the three workstream scores (20 manual cases each) and does NOT waive the final v1.70 release gate. The feature score below is **not yet measured or certified** because the new Windows native tests and GUI acceptance remain pending.

- **F01:** saved reference capture, interior primary witness, secondary witness, Boolean provenance, retained split fragment, merged face, transformed face, rotated feature center, two-stage rotation, preserved original witness, ambiguity rejection, removed region rejection, undo rollback, edit persistence, save/reopen, native CTest, complex split, multi-face merge, graphical review, no wrong-face attachment.
- **F02:** line/arc/spline witness capture, tangent orientation, source body tracking, Boolean Modified/Generated provenance, surviving split edge, removed witness rejection, split-boundary rejection, rigid translation, arbitrary-axis rotation, two rigid stages, curve curvature discrimination, ambiguous multiple edges, fillet/chamfer rebinding, stale reference diagnostics, undo rollback, save/reopen, native CTest, compound body, GUI verification, no wrong-edge attachment.
- **F05:** 3-point solve, planar mates, coaxial mates, signed positive offset, signed negative offset, 6-DOF rank check, contradicting mates, near-unit directions, network loops, per-connection offset closure, save/reopen offsets, legacy scene compatibility, malformed input rejection, undo rollback, persistent editor, GUI offset entry, edge/face geometry attachment, driven assembly movement, collision inspection, native CTest.
- **F08:** saved sheet identities, source body set, current geometry fingerprint, metadata fingerprint, stale detection, missing source rejection, measurement link preflight, exported current dimensions, current orthographic projection, current BOM, batch staged render, no partial output on preflight error, new output directory, file write rollback, CSV source index, preserves revision token, individual issue/review, SVG renderer validation, native CTest, GUI rendering acceptance.

**Do not count any of the above 20-item checklists as 17/20 passed until the corresponding evidence is captured.** A source-level contract does not substitute for a Windows-native or manual workflow assertion. At the current checkpoint, these four features are *in development* and below the required evidence threshold for an 85% designation.

## Exact automated verification of this candidate (Linux development environment)

- `python -m unittest discover -s tests -p '*.py' -q`: **597 passed** (includes existing reference and simulated audit-failure tests; some require Python OCP dependencies).
- All 18 C++ standard-library regression executables: **passed**, including the new plane-offset solver/network executable.
- `CMakeLists.txt` registers exactly **130** native Windows CTests (they have **not been executed for this new snapshot**).
- The earlier Windows-tested Fix 2 baseline is preserved separately; its 130/130 success must NOT be inherited by this snapshot.

## Windows commands (run in the NEW extracted source directory)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot C:\vcpkg -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Exit code `3` only means automated success and manual acceptance still outstanding. Exit code `1` is an automated failure. Preserve logs, don't skip the failing test or alter the manifest. Test the new UI command under **File → Regenerate ALL saved drawing previews (not issued)...** with several saved sheets and deliberately missing links. Test the assembly edit under the existing mate UI. Test topology on a saved part, after a real edit, save/reopen and undo/redo.

## Release and distribution

**Current deliverables:** preliminary full source ZIP + guarded upgrade ZIP from the Architecture/Resilience/Drawings snapshot. **Not produced:** Windows binaries or final clean-install package. The latter is reserved until all ten features and the standing workstream/manual gate pass, followed by a fresh Windows install smoke test. Do not publicize the interim archive as production-ready.

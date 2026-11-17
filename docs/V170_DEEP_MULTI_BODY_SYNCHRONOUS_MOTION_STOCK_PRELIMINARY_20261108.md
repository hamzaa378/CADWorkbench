# CADWorkbench v1.70 — integrated deeper functionality (PRELIMINARY, 2026-11-08)

This source checkpoint extends **all three engineering workstreams** without relaxing
CADWorkbench's native release gate. It is not a formal acceptance result.

## Parametric: atomic optional-feature histories across multiple source bodies

The Edit menu includes **Coordinated feature suppression/restore across bodies**.
Select 2–12 parametric source bodies and turn supported optional holes, patterns,
chamfers and fillets on/off independently. All affected source B-reps are rebuilt
in a detached candidate. `propagateLinkedBodies` then rebuilds the dependent
DAG. Only after every geometry check succeeds does the operation make a **single
undo checkpoint**. Invalid operations leave every live source and dependent
untouched. There is an explicit read-only preview listing valid changed solids
and their volumes, and a confirmation before commit. A Windows-only native
regression is integrated into the existing `cadcore_parametric_blind_hole` test:
it covers two simultaneous source edits, rejected invalid edits, undo/redo,
and save/reopen. This remains limited to the optional features represented by
the current parametric rectangle feature definition; it is **not** universal
persistent naming through every topology-changing operator.

## Assembly: simultaneous sampled-body proximity

The articulated workspace CSV previously compared the *union of sampled
bounding boxes*. Two moving bodies can occupy the same space at different times
while never getting near one another in the same pose. The new independent
`SimultaneousProximity` tracker records a minimum AABB gap and overlapping-pose
count for each pair at **matching sample indices**. It validates the complete
body set and geometry before committing each sample; malformed frames cannot
poison earlier results. These fields are appended to the exported workspace CSV.
All pose B-reps are regenerated through the existing articulated solver.

**Limits:** Bounding boxes are broad-phase approximations, not exact solid
interference; between-sample collisions remain possible; only moving child
components in the articulated graph are compared by this report. The exact
nominal-fit audit in the other motion path remains separate.

## Sheet metal: free-rectangle reuse for stock-sheet planning

The manufacturing-job stock planner now splits and prunes available rectangular
space after each placement. Unlike the old row/shelf-only algorithm, later
blanks can reuse free areas above or alongside earlier blanks. It is deterministic,
preserves input part order, enforces interpart and edge gaps, respects optional
90-degree rotation and validates the entire plan before publishing any package.
The public SVG/CSV/provenance formats are unchanged. Regression includes a
one-sheet arrangement that required two sheets under the former shelf-only
planner and 350 deterministic randomized geometric integrity cases.

**Limits:** Conservative rectangular envelope layout only; not grain-aware,
nonrectangular contour nesting, CNC toolpaths, bending order, or certified scrap
optimization. The algorithm is heuristic and does not guarantee globally
optimal stock usage. Its proposed layout must be reviewed by manufacturing.

## Verification and release status

- Python and independent source/reference checks: 500/500 passing.
- Portable C++ (GCC, non-sanitized): 18/18 targets passing.
- Portable C++ (Clang, ASan+UBSan): 18/18 targets passing.
- New native feature-history test has **not** been executed here (Windows MSVC,
  Qt6, and OpenCascade required).
- Last supplied Windows baseline: 128/130 native tests passing.
  **Deferred unchanged:** `cadcore_v170_assembly_mate_document` (clearance
  report assertion) and `cadcore_folded_sheet_metal` (segfault).
- Final release and formal 60-scenario acceptance: **NOT approved**.

On Windows, extract into a **new directory** and run the focused development
workflow first:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_NEW_WORKFLOWS.ps1 -VcpkgRoot C:\vcpkg
```

Then, once the deferred failures are scheduled for correction, run the full
unchanged release gate:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

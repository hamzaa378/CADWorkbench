# CADWorkbench v1.70 — larger three-workstream preliminary engineering increment

**Status:** DEVELOPMENT SNAPSHOT. NOT a fully verified Windows release. Date: 2026-11-07.

Based on `CADWorkbench_v170_Preliminary_LinkedDimensionEditor_Source_20261107.zip`. The previous preliminary archive remains a separate rollback baseline. All prior source is retained, including the face-machining witness fixture correction. No new ZIP may be represented as satisfying the final 85% + 85% + 65% acceptance gate.

## New workflows, actually connected in the Qt application

### A. Parametric history: staged B-rep verification before commit

In **Edit → Mixed associative feature history (branches + sketches)...**, choose **Preflight staged history (read-only)**. It rebuilds the staged fork/join history in memory using the existing production evaluator and reports each returned stage's B-rep health and volume. When the full rebuild fails, it attempts prefixes beginning with the first sketch stage, identifying the earliest testable failing prefix. This command does not call `setInterleavedFeatureHistory()`, update model geometry, or create an undo record. The existing **Save** command remains responsible for committing a successful complete history.

Limits: First-prefix localization can only begin at the first sketch stage because the mixed-history evaluator rejects sketch-free histories. This is not a universal proof of persistent topology naming; complex split/merge references and reference rebinding remain unfinished. Some valid non-solid subtypes may have zero volume even though the evaluator accepts their B-rep.

### B. Assembly: structural audit of the saved rigid-mate graph

In **Assembly → Audit saved rigid mate structure and cycle links...**, inspect the current persistent rigid-mate graph without mutating its observations or placements. The report shows fixed occurrence, participating components, connected components, unreachable nodes, independent graph cycle count, and bridge links (whose removal separates a structural branch). Multigraph parallel links are handled correctly; missing endpoints, self-links and empty observation sets fail closed. The portable algorithm (`AssemblyNetworkStructureAudit.h`) limits itself to 256 nodes and 4096 connections.

Limits: A connected graph is **not** proof of geometric mate consistency, degrees-of-freedom closure, collision freedom, or independent constraints. The separate geometric solver remains authoritative for the supported rigid 6-DOF observations; articulated and hierarchical solver maturation is still required.

### C. Drawing/manufacturing & sheet metal: saved bend-sequence review

In **Inspect → Audit selected folded body bend sequence...**, use the saved folded-sheet definition, enter one K-factor per bend, then generate a read-only developed neutral-length and bend-station report with cumulative signed fold angles, direction reversals, allowance, minimum straight flange, and inside-radius/thickness ratios. Every neutral-length total and bend station is independently cross-checked against the existing `calculateFoldedSheetFlatPattern()` kernel, rejecting disagreement. CSV export uses `QSaveFile` for atomic writes. Existing DXF and shop-limit workflows remain unchanged.

Limits: This is a diagnostic *sequence* report, **not** an optimized press-brake order, springback model, tool reach/clearance or collision certificate. Only current saved folded-body definitions may be audited. GD&T and automatically regenerating associative drawing dimensions remain major ongoing work.

## Source files changed or added in this increment

- `src/core/AssemblyNetworkStructureAudit.h` (new, portable header)
- `src/core/SheetMetalBendSequenceAudit.h` (new, portable header)
- `src/ui/MainWindow.cpp`, `src/ui/MainWindow.h` (three end-to-end GUI command integrations)
- `tests/assembly_network_structure_audit_tests.cpp` (new)
- `tests/sheet_metal_bend_sequence_audit_tests.cpp` (new)
- `tests/v170_major_three_workstream_upgrade_contract.py` (new)
- `tests/assembly_constraint_network_v170_tests.cpp` (existing native rigid network CTest extended)
- `tests/cadcore_tests.cpp` (existing native folded-sheet CTest extended)
- `scripts/verify_portable_consolidation.sh` (14 portable C++ test targets, previously 12)
- `CONTINUE_HERE_LATEST_PRELIMINARY.md` and the three SHA-256 manifest aliases (updated)

## Verification evidence for this increment

- `python -m unittest discover -s tests -p '*.py' -q`: **391/391** passed (source contracts and independent kernel references).
- `CAD_SKIP_PYTHON=1 bash scripts/verify_portable_consolidation.sh g++`: **14/14** targets pass, with ASan and UBSan.
- Same script with `clang++`: **14/14** targets pass with ASan and UBSan.
- Structural mate audit: **20** standalone assertions passed; bend-sequence audit: **18** standalone assertions passed.
- Original 130 CTest registrations preserved, with new native regression assertions integrated in two existing CTest executables.
- **NOT VERIFIED:** the Qt6/OpenCascade application compiling with Visual Studio/MSVC, running the modified GUI, and all 130 Windows native CTests. A passing portable/test-contract suite cannot validate the Qt integration.

## Windows verification (required before promoting this source)

Extract into a new folder, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

Check **all 130/130 native CTests**, especially `cadcore_v170_interleaved_history` (the older face-machining issue), `cadcore_v170_closed_loop_rigid_network`, `cadcore_folded_sheet_metal` and `cadcore_orthographic_drawing`.

Manual UI smoke checklist:
1. Create a mixed history with an external constrained sketch, preview it, then intentionally make a disconnected boss. The preflight must refuse the geometry without changing a saved model. Edit the step back and commit, undo, save, reopen.
2. Create a saved rigid mate network. Inspect graph bridges/cycles and confirm the audit does not alter placement, source tokens or revision count. Check geometric solver separately.
3. Create an editable folded sheet with two alternating bends. Open the bend-sequence report, export CSV, independently compare stations with the DXF/bend table. Edit folded-body dimensions, repeat. Ensure stale body definitions are rejected.

## Estimated release-gate coverage (NOT formal pass/fail percentages)

| Workstream | Prior working estimate | After increment, rough working estimate | Target |
|---|---:|---:|---:|
| Parametric modeling | ~60% | ~61–62% | 85% |
| Assembly engineering | ~60% | ~62–63% | 85% |
| Drawings and sheet metal | ~42% | ~44–45% | 65% |

These are judgmental planning estimates, **not** percentages derived from the mandatory 60-scenario acceptance matrix. The 85/85/65 target is still unmet. Remaining major items: reliable topology identity for complex B-rep edits; unrestricted feature regeneration and interactive editing; mature hierarchical/closed-loop assembly manipulation; fully associative drawings and GD&T; multi-flange and folded-to-flat redesign workflows.

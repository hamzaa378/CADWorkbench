# CADWorkbench v1.70 — complete Windows QA and functionality-gap handoff

This candidate is a **test/audit checkpoint**, not a new feature release and **not** a Windows-verified build. Preserve the previously verified Windows installation for comparison. Do not mask any failing CTests, especially `cadcore_folded_sheet_metal`.

## One-command comprehensive audit

Open PowerShell in the newly extracted source directory and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot C:\vcpkg
```

The script validates the source SHA-256 manifest, builds the full Release app/tests, runs **all 130 registered CTests**, also runs the bundled Python/source regression suite, and generates an objective report. Any failing test produces a nonzero exit code but still generates the report where possible.

### Outputs

- `build/v170_comprehensive_audit/python_source_tests.log`: bundled Python/source test run on this Windows checkout.
- `build/v170_comprehensive_audit/ctest_output.log`: full native CTest output and any `[folded commit]` markers.
- `build/cadworkbench_release_ctest.xml`: full JUnit report generated on this run; stale copies are deleted first.
- `build/v170_comprehensive_audit/v170_native_130_matrix.csv`: one row per native CTest, exact PASS/FAIL/NOT_RUN/SKIPPED state.
- `build/v170_comprehensive_audit/v170_acceptance_60_matrix.csv`: each user workflow, priority, evidence status, suggested native test leads and next action.
- `build/v170_comprehensive_audit/v170_remaining_work.md`: comprehensive test and acceptance gap overview, including unresolved critical scenarios.
- `build/v170_comprehensive_audit/v170_full_audit.json`: machine-readable version of the two matrices.
- `build/V170_ACCEPTANCE_RESULTS.csv`: the *editable* 60-case evidence tracker. It is only initialized from `docs/` if missing and is never overwritten by a rerun.

If you only want to refresh the report from an existing JUnit and a manually updated scorecard, run:

```powershell
python tools\v170_full_test_audit.py --root . --junit build\cadworkbench_release_ctest.xml --acceptance build\V170_ACCEPTANCE_RESULTS.csv --output-dir build\v170_comprehensive_audit
```

The audit returns nonzero until the formal gate is satisfied; this is expected for an incomplete review.

## Folded-sheet crash: exact current state

The **9 November 2026** focused Windows log shows that the last successful point is `[folded edit] commit edited solid: before`. The crash has not yet been fixed or proven to originate in the OpenCascade solid builder, linked-body propagation, or undo history. The new diagnostic instrumentation, enabled only when `CAD_V170_FOLDED_TRACE=1`, reports the precise stage. A crash during `checkpoint()` additionally prints each deep-copy boundary.

If the crash persists, find the last `[folded commit]` or `[folded checkpoint]` entry in `ctest_output.log` and use the Windows debugger / crash dump on `cadcore_tests.exe --folded-sheet-metal` before modifying the geometry algorithm. Do not replace a real crash with a skipped test.

The previously recorded **full** Windows run of the older frozen source passed **126/130**, with four failures. A *subsequent focused* four-case run passed the articulated transaction, assembly mate document, and parametric foundation tests but still crashed on the folded-sheet case. There is not yet a fresh 130-case result for this source checkpoint.

## How to record acceptance evidence

The 60 scenarios are separate from the native suite. To mark a case `PASS` in `build/V170_ACCEPTANCE_RESULTS.csv`, perform the workflow in the actual application, capture evidence under `build/acceptance_evidence/<ID>/`, and enter the path of an actual file in `evidence_path` (for example `build/acceptance_evidence/P09/reproduction.md`). Record input values, expected vs observed results, save/reopen, undo/redo behavior, images and application version where relevant. An automated native PASS is a lead to inspect, **not** a substitute for an end-to-end workflow. The recorded evidence still needs engineering review.

The scorecard has 20 Parametric (P), 20 Assembly (A), and 20 Drawings/Sheet-metal (D) cases. Release rules require **at least 17/20** in two groups and **at least 13/20** in the third, including all critical scenarios, plus **130/130 native CTests** and manual GUI review. This source still has no independently confirmed 60-scenario acceptance result.

## High-priority end-to-end demonstrations

- **P09/P10/P12/P18:** Create an upstream sketch, add mixed solid/Boolean/edge operations, split and merge supported faces, change the upstream dimension, and verify *correct physical face/edge reference*, not just unchanged counts. Undo, save/reopen and retest.
- **A07/A08/A11/A12/A17:** Create a closed-loop multibody mechanism, edit several connected mates/coordinates, preview and commit movement, measure clearance at multiple poses, undo/redo, save/reopen and repeat the same motion.
- **D04/D08/D09/D17:** Build a multi-flange folded body, create a linked flat and drawing annotations, edit bend angles/widths and upstream dimensions, verify linked DXF/SVG/drawing geometry, flag stale ambiguous references, save and reopen.

## Not-yet-proven capabilities to investigate

These are audit priorities, **not claims that the source lacks them**: unrestricted topology naming for arbitrary face split/merge cases; general closed-loop articulated constraint solving with persistent gear-link definitions; continuous rather than sampled collision guarantees; full manufacturing-valid fold-flat round-trip; associative drawing dimensions/GD&T surviving arbitrary feature changes; detailed contour nesting beyond neutral orthogonal reliefs.

## Test completeness definitions

Native PASS means the registered executable returned success under Windows Release. Acceptance PASS means the named UI/engineering workflow was exercised and evidence recorded. Completion percentages previously reported (P ~85%, A ~90%, D ~81%) are **provisional source-functionality estimates**, not measured acceptance. Do not raise them merely because CTests passed. The audit reports actual scenario pass counts, unrun cases and failed cases separately.

---

## All 60 acceptance cases to execute and review

### Parametric modeling

- [ ] **P01** (mandatory critical): Fully constrained closed profile with dimensions and constraints. Evidence path: ____
- [ ] **P02** (mandatory critical): Solve geometric constraints and report under/overconstraints. Evidence path: ____
- [ ] **P03**: Interactive sketch UI create/edit/delete without direct JSON editing. Evidence path: ____
- [ ] **P04**: Sketch attached to arbitrary stable reference planes/faces. Evidence path: ____
- [ ] **P05**: Extrude/revolve using editable sketch profiles. Evidence path: ____
- [ ] **P06**: Robust pocket, hole, fillet, chamfer history. Evidence path: ____
- [ ] **P07**: Loft across multiple profiles with guide controls. Evidence path: ____
- [ ] **P08**: Sweep across nontrivial guide and stable normal handling. Evidence path: ____
- [ ] **P09** (mandatory critical): Edit upstream dimensions and regenerate downstream features. Evidence path: ____
- [ ] **P10** (mandatory critical): Interleave sketch/body/Boolean/features in one timeline. Evidence path: ____
- [ ] **P11**: Cross-body dependent geometry after edits. Evidence path: ____
- [ ] **P12** (mandatory critical): Stable edge lineage after split, merge and selective fillet. Evidence path: ____
- [ ] **P13** (mandatory critical): Stable face lineage after split, merge and Boolean edits. Evidence path: ____
- [ ] **P14** (mandatory critical): Dependency cycle rejection and transactional rollback. Evidence path: ____
- [ ] **P15**: Suppress, reorder, insert and delete features without corruption. Evidence path: ____
- [ ] **P16**: Feature-level property editing with undo/redo. Evidence path: ____
- [ ] **P17**: Persist complex feature DAG save/reopen and recover. Evidence path: ____
- [ ] **P18** (mandatory critical): Error states for invalid geometry/ambiguous references. Evidence path: ____
- [ ] **P19**: Scale and performance on representative large history. Evidence path: ____
- [ ] **P20**: STEP/IGES import/edit/export preserves usable geometry. Evidence path: ____

### Assembly engineering

- [ ] **A01**: Multi-level assembly hierarchy create and edit. Evidence path: ____
- [ ] **A02**: Move/rename nested subassemblies atomically with undo/redo. Evidence path: ____
- [ ] **A03** (mandatory critical): Persist occurrence identities and hierarchy through save/reopen. Evidence path: ____
- [ ] **A04**: Mated subassembly instances and local frames. Evidence path: ____
- [ ] **A05**: Persistent coincident/parallel/offset rigid mates. Evidence path: ____
- [ ] **A06**: Revolute and prismatic joint definitions. Evidence path: ____
- [ ] **A07** (mandatory critical): Closed-loop rigid mate solver. Evidence path: ____
- [ ] **A08** (mandatory critical): Closed-loop articulated motion solve. Evidence path: ____
- [ ] **A09**: Move one component and solve dependencies interactively. Evidence path: ____
- [ ] **A10**: Drag/drive controlled motion with limits. Evidence path: ____
- [ ] **A11** (mandatory critical): Detect overconstraint/underconstraint and singularity. Evidence path: ____
- [ ] **A12** (mandatory critical): Edit/rebind mate references after model changes. Evidence path: ____
- [ ] **A13**: Interference and exact solid overlap verification. Evidence path: ____
- [ ] **A14**: Minimum-clearance verification between chosen solids. Evidence path: ____
- [ ] **A15**: Hierarchical subassembly fit/clearance audit. Evidence path: ____
- [ ] **A16**: Motion sweep collision/clearance verification. Evidence path: ____
- [ ] **A17** (mandatory critical): Mate/pose edits atomic with undo/redo. Evidence path: ____
- [ ] **A18**: Named exploded view and assembly sequence. Evidence path: ____
- [ ] **A19**: Hierarchical BOM quantity, revisions and configuration. Evidence path: ____
- [ ] **A20**: STEP assembly import/export preserves hierarchy and placements. Evidence path: ____

### Drawings and sheet metal

- [ ] **D01**: Generated orthographic views from solid geometry. Evidence path: ____
- [ ] **D02**: Editable section views with reliable source links. Evidence path: ____
- [ ] **D03**: Editable detail views with scale and clipping. Evidence path: ____
- [ ] **D04** (mandatory critical): Linked drawing dimensions survive supported parametric changes. Evidence path: ____
- [ ] **D05**: Diameter/radius/hole callouts linked to source features. Evidence path: ____
- [ ] **D06**: Datum symbols and validated GD&T feature-control frames. Evidence path: ____
- [ ] **D07**: Standards-aware dimension units, tolerances, formats. Evidence path: ____
- [ ] **D08** (mandatory critical): Source regeneration updates drawing geometry automatically. Evidence path: ____
- [ ] **D09** (mandatory critical): Stale/ambiguous references are flagged, never silently wrong. Evidence path: ____
- [ ] **D10**: Drawing title blocks, sheet formats and revisions. Evidence path: ____
- [ ] **D11**: Technical drawing SVG/PDF/DXF exports verified against model. Evidence path: ____
- [ ] **D12**: Multi-sheet drawing persistence and edit/reopen. Evidence path: ____
- [ ] **D13**: Sheet-metal blank creation with thickness rules. Evidence path: ____
- [ ] **D14**: Edge flanges with bend angles/radii. Evidence path: ____
- [ ] **D15**: Multiple flange/relief/corner workflows. Evidence path: ____
- [ ] **D16**: K-factor or bend allowance table and process rules. Evidence path: ____
- [ ] **D17** (mandatory critical): Associative folded-to-flat update after edit. Evidence path: ____
- [ ] **D18**: Valid flattened profile and fold-line generation. Evidence path: ____
- [ ] **D19**: Re-fold round-trip geometry/measurement validation. Evidence path: ____
- [ ] **D20**: Manufacturing handoff with bend callouts and flat DXF. Evidence path: ____

After scoring, rerun `RUN_V170_COMPREHENSIVE_AUDIT.ps1` (with `-SkipBuild` only when the build is unchanged) or the standalone audit tool. Keep an independent, reviewed acceptance evidence record before final approval.

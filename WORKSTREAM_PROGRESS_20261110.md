# Engineering progress and acceptance ledger — 2026-11-10

The prior implementation estimates (P **~85%**, A **~90%**, D **~81%**) were informal and repeatedly carried forward. They are **not** scored engineering-completion percentages, and this update does not retrospectively assert that those numbers were correct. Inventing +1% for each new feature would be false precision. Until a reproducible rubric and end-to-end evidence are in place, treat those numbers as historical planning placeholders, not verified scores.

## Track progress using three independent measures

| Workstream | Source capability advanced this increment | Local automated checks | Windows 130-test audit of this snapshot | GUI acceptance |
|---|---|---|---|---|
| P — parametric modeling | P13 face lineage: competing duplicated-face witnesses now fail safely | 587 shared Python tests + topology source contracts + native test added | Not run | 0/20 NOT_RUN |
| A — assembly engineering | A07/A15 supporting diagnostics: independent edge-disjoint paths from fixed occurrence | 40 assembly assertions, among 16 portable suites | Not run | 0/20 NOT_RUN |
| D — drawings/sheet metal | D09 stale/ambiguous linked-dimension preflight across a complete saved sheet | Source contracts + native drawing regression added | Not run | 0/20 NOT_RUN |

**These are three completed source increments, not three accepted scenarios.** The 0/20 entries mean *not yet manually reviewed*, not failed. Completion for each workstream requires all 20 scenarios demonstrated or explicitly dispositioned and retested; the minimum release gate is ≥17/20 in two streams, ≥13/20 in the third, all critical scenarios passed, plus native build/regression passing.

## Why implementation progress does not equal acceptance

- A new operation can extend existing functionality without fully satisfying an acceptance scenario (e.g., P13 still excludes universal naming through arbitrary rotations and topology changes).
- Assembly redundancy counts links, not geometry or motion. A07's full 6-DOF closure is a separate problem.
- The new dimension audit exposes stale references; it does not itself regenerate a full associative drawing (D08).
- Source and portable passing checks don't imply a Windows Qt build or a GUI demonstration.

## Next engineering blockers to 100% capability

P: general face/edge identity across multiple split/merge and rotations, guided lofts/sweeps, large-history stress, and full edit/reorder regression.

A: hierarchical mate rebind, solved articulated motion/clearance through collision, persistence, and full assembly STEP hierarchy round trips.

D: genuinely automatic drawing regeneration and reattachment after source edits, manufacturing GD&T validation, multi-flange relief, and folded/flat round-trip checks.

Use `docs/V170_ACCEPTANCE_RESULTS.csv` and the established QA reviewer to record real PASS/FAIL/NOT_RUN evidence when manual review resumes; never mark an unrun scenario passed.

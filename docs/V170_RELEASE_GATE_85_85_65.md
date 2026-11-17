# CADWorkbench v1.70 — user-approved 85/85/65 release gate

## Release rule (2026-11-07)

Do not promote or distribute a **release** source ZIP unless all of the following hold. The user separately authorized one clearly labeled preliminary source snapshot after the current pass; that snapshot remains unverified for release:

1. At least **two** of the three workstreams below pass **85% or more** of their independently verifiable acceptance scenarios, **and** the remaining workstream passes **65% or more**. All three must be considered.
2. The `cadcore_v170_interleaved_history` regression passes after the face-machining witness-fixture correction. No invalid-geometry or reference-validation bypass is acceptable.
3. The full Windows MSVC Release build succeeds and all registered native CTests (currently 130) pass, with no skipped critical cases. Repeat critical regressions with fresh scene save/reopen.
4. Existing passing features must remain intact: cross-body DAG, witnessed-topology lineage, sketch dependencies, rigid and articulated mates, drawings, sheet-metal, undo/redo and exports.
5. An acceptance point counts only when it works in a real workflow and has reproducible evidence (native test + exercised UI/persistence for UI-exposed workflows). Code existence, menu availability, source-only lint, and standalone math tests do not constitute acceptance.

## How percentages are measured

Each workstream has **20 equal-weight end-to-end scenarios**, scored pass/fail. Every accepted scenario is worth 5 percentage points. Partial credit can be tracked separately but must not be used toward the release gate. 17/20=85%, 13/20=65%. A regression in any mandatory critical behavior blocks release regardless of scenario counts. The choices of 20 scenarios and equal weights are a transparent project planning convention; they are not an independent Fusion benchmark.

### P — Parametric modeling (20 scenarios)

- P01 Fully constrained closed profile with dimensions and constraints [critical]
- P02 Solve geometric constraints and report under/overconstraints [critical]
- P03 Interactive sketch UI create/edit/delete without direct JSON editing
- P04 Sketch attached to arbitrary stable reference planes/faces
- P05 Extrude/revolve using editable sketch profiles
- P06 Robust pocket, hole, fillet, chamfer history
- P07 Loft across multiple profiles with guide controls
- P08 Sweep across nontrivial guide and stable normal handling
- P09 Edit upstream dimensions and regenerate downstream features [critical]
- P10 Interleave sketch/body/Boolean/features in one timeline [critical]
- P11 Cross-body dependent geometry after edits
- P12 Stable edge lineage after split, merge and selective fillet [critical]
- P13 Stable face lineage after split, merge and Boolean edits [critical]
- P14 Dependency cycle rejection and transactional rollback [critical]
- P15 Suppress, reorder, insert and delete features without corruption
- P16 Feature-level property editing with undo/redo
- P17 Persist complex feature DAG save/reopen and recover
- P18 Error states for invalid geometry/ambiguous references [critical]
- P19 Scale and performance on representative large history
- P20 STEP/IGES import/edit/export preserves usable geometry

### A — Assembly engineering (20 scenarios)

- A01 Multi-level assembly hierarchy create and edit
- A02 Move/rename nested subassemblies atomically with undo/redo
- A03 Persist occurrence identities and hierarchy through save/reopen [critical]
- A04 Mated subassembly instances and local frames
- A05 Persistent coincident/parallel/offset rigid mates
- A06 Revolute and prismatic joint definitions
- A07 Closed-loop rigid mate solver [critical]
- A08 Closed-loop articulated motion solve [critical]
- A09 Move one component and solve dependencies interactively
- A10 Drag/drive controlled motion with limits
- A11 Detect overconstraint/underconstraint and singularity [critical]
- A12 Edit/rebind mate references after model changes [critical]
- A13 Interference and exact solid overlap verification
- A14 Minimum-clearance verification between chosen solids
- A15 Hierarchical subassembly fit/clearance audit
- A16 Motion sweep collision/clearance verification
- A17 Mate/pose edits atomic with undo/redo [critical]
- A18 Named exploded view and assembly sequence
- A19 Hierarchical BOM quantity, revisions and configuration
- A20 STEP assembly import/export preserves hierarchy and placements

### D — Drawings and sheet metal (20 scenarios)

- D01 Generated orthographic views from solid geometry
- D02 Editable section views with reliable source links
- D03 Editable detail views with scale and clipping
- D04 Linked drawing dimensions survive supported parametric changes [critical]
- D05 Diameter/radius/hole callouts linked to source features
- D06 Datum symbols and validated GD&T feature-control frames
- D07 Standards-aware dimension units, tolerances, formats
- D08 Source regeneration updates drawing geometry automatically [critical]
- D09 Stale/ambiguous references are flagged, never silently wrong [critical]
- D10 Drawing title blocks, sheet formats and revisions
- D11 Technical drawing SVG/PDF/DXF exports verified against model
- D12 Multi-sheet drawing persistence and edit/reopen
- D13 Sheet-metal blank creation with thickness rules
- D14 Edge flanges with bend angles/radii
- D15 Multiple flange/relief/corner workflows
- D16 K-factor or bend allowance table and process rules
- D17 Associative folded-to-flat update after edit [critical]
- D18 Valid flattened profile and fold-line generation
- D19 Re-fold round-trip geometry/measurement validation
- D20 Manufacturing handoff with bend callouts and flat DXF

## Evidence and current status

The corrected face-machining test fixture remains in the working source. Unreleased increments include assembly-subtree moves, group fit/clearance audits, transactional sketch-constraint editing (including pair relations and fixed anchors), folded-contour validity checks, editable manual GD&T frames, and the new **conditional conservative motion-clearance bound** (see `docs/V170_MOTION_CLEARANCE_CONDITIONAL_BOUND_20261107.md`). Source checks, portable numerical regressions and independent geometry checks are not substitutes for full native acceptance. The current expanded working tree has not undergone a native Windows MSVC Release build.

**Completion score: not assigned.** The individual acceptance scenarios have not all been exercised and recorded. In particular, existing source notes describe universal topology lineage, unrestricted histories, full mate editing, standards-compliant GD&T and production-grade sheet metal as incomplete. Therefore **no ZIP release is authorized by this gate**.


## Additional preliminary work: persistent rigid-mate editing preview (2026-11-07)

Read `docs/V170_PRELIMINARY_LIVE_MATE_EDIT_PREVIEW_20261107.md`. The existing saved rigid-mate link manager now supports world-frame observation editing, full-graph kinematic preview and confirmation, no-op revision avoidance, and dedicated native regression assertions. This partially advances A05/A12/A17 but does not implement cross-component rebind or general topology-aware mate recovery. Portable/source checks pass; Windows Qt/OpenCascade tests and the 60-scenario evidence score are still pending.

# CADWorkbench v1.70 — mixed associative DAG and witnessed-edge source update

**Windows-native verification pending.** This new increment retains all 130 registered CTests and adds regression cases in existing suites. GCC/Clang portable graph-planner checks, Python preflight and independent OpenCascade geometry reference tests do **not** establish that the full Qt/OpenCascade application builds or operates correctly on Windows.

- **Generalized mixed history:** Explicit feature dependency inputs, two-input Boolean joins, coordinated external sketch rebuilding, strictly validated source references, transactional rollback, and versioned DAG save/reopen. A compact Edit-menu authoring dialog now exposes practical sketch/transform/Boolean DAG steps.
- **Persistent topology references:** Witnessed straight/circular edges survive supported splits, arc trimming and translations; ambiguous or deleted successors fail closed. Curved-face anchors now test both unchanged and translation-compensated witness positions to avoid unnecessary invalidation from changes to the solid center of mass.
- **Scope:** 1–40 supported mixed steps on eligible ordinary solids, with at least one external sketch. No unrestricted universal topological naming for B-splines/nonrigid edits; no complete visual DAG editor or generalized inter-part feature authoring.

See `docs/V170_ASSOCIATIVE_MIXED_DAG_AND_WITNESSED_EDGES_20261106.md` for exact API contract, examples, compatibility, checks and the mandatory Windows verification command.

---

**Latest 130-native-test development candidate (Windows acceptance pending):** A new 1–40-step interleaved history allows supported ordinary single-input shape operations to alternate with external constrained-sketch bosses/pockets, with graph-ordered regeneration, rollback and scene persistence. Conical and toroidal surface witnesses extend prior planar/cylinder/sphere anchors. A bounded articulated multi-body revolute/prismatic loop solver adds numeric closure, joint limits and atomic placements. These remain **partial** architectural solutions: arbitrary DAG interleaving with a GUI editor, universal topological lineage, and live persisted articulated mate networks are not delivered. See `V170_COMPLETION_ENGINEERING_INCREMENT.md`.

**Latest 127-native-test development candidate (Windows acceptance pending):** A same-body mixed pipeline combines up to eight externally linked constrained-sketch modifiers followed by the existing general shape-feature tree. New trimmed cylindrical/spherical witness anchors cover selected unambiguous splits/merges and translations. A transactional 2–32-component rigid assembly network solver validates closed-loop constraints and saves corrected placements. These are **partial**, not universal, implementations of roadmap items 1, 8, 11, 23 and 24. See `V170_THREE_GAPS_ENGINEERING_INCREMENT.md` for their exact supported scope and remaining gaps.

**Latest v1.70 development increment (123 native tests registered; Windows not yet verified):**

The external constrained-sketch in-place boss/pocket system now supports **up to eight ordered modifiers** per eligible target. Every distinct source enters the cross-body dependency DAG, with transactional regeneration, per-stage editing, removal from the end, strict save/reopen validation, and corrupted-scene rollback. The previous two-stage source-loading field-count defect is corrected. The full 50-feature v1.70 roadmap is **not complete**. See `V170_ORDERED_INPLACE_SKETCH_STACK.md` for scope and limitations.

**Latest development increment — two-stage in-place external sketches and mixed mate GUI (119 registered native CTests; Windows pending).** A second ordered Fuse/Cut feature from a different external sketch can be appended, edited or removed on an eligible existing target body, with both sources in the DAG and validated save/reopen. The assembly mate dialog now authors three-point, point-plus-two-axis, or point-plus-axis-plus-plane constraints. This advances partial roadmap targets #8, #11 and #23, but does **not** finish them. See `V170_TWO_STAGE_INPLACE_AND_ADVANCED_MATES.md`. No new claim is made about the full 50-feature release.

**Current 116-test development addendum (native Windows verification pending):** Supported planar face identity can now follow a **unique witnessed trimmed-face successor** across some splits/merges; source-linked boss/pocket can modify an eligible existing target body in place (one modifier per body); and a bounded rigid six-DOF assembly mate solver supports point, coaxial and planar inputs (three-point UI, no closed-loop solve). See `V170_THREE_FOUNDATIONS_INCREMENT.md`. Roadmap items 1, 7, 11, 23 and 24 remain **partial**: these increments are not universal solutions.

**Current development addendum:** A source-linked constrained sketch can now also create an **associative additive boss** as a hidden extrusion and a visible connected-solid Fuse. It rejects detached and fully buried bosses both at creation and on later regenerations (111 registered native CTests; Windows run pending). See `V170_ASSOCIATIVE_SKETCH_BOSS.md`.

**Development addendum:** This increment includes live source retargeting and associative sketch pockets using a hidden cutter + linked Boolean Cut result (109 registered native CTests, Windows run pending). It does **not** implement a universal in-place cross-body pocket modifier or persistent topology naming. See `V170_ASSOCIATIVE_SKETCH_POCKET_AND_RETARGET.md`.

# CADWorkbench v1.70 — feature-by-feature development status

**Last user-confirmed native Windows baseline: v1.70 advanced-profile candidate, 94/94 passing.** This **130-test candidate** extends the unverified 127-test source; intervening candidates have not been reported as Windows-verified. Preserve the last passing Windows installation and validate all 130 native tests before promoting this source. This ledger is intentionally conservative: a capability is not marked complete just because it has a menu entry or one prototype operation.

| # | Target | Status | Current evidence and remaining gap |
|---:|---|---|---|
| 1 | Stable topological naming | Partial | Planar, cylindrical, spherical, conical, and toroidal witnesses support limited unambiguous geometry succession and translations; arbitrary B-spline evolution and ambiguity remain |
| 2 | General parametric feature graph | Partial | Within-body branching and linked Boolean DAG remain. A new 1–40-step linear mixed history can interleave single-input shape operations and external sketch bosses/pockets; arbitrary branched interleaving and graphical authoring remain |
| 3 | Selective graph rebuild | Partial | Dependency-aware within-body cache reuse, plus transitive linked-Boolean regeneration in dependency order; no global feature/assembly invalidation scheduler; unary sketch-derived bodies now share the Boolean dependency order |
| 4 | Arbitrary sketch profile revolve | Partial | Polygon radial/axial profile and arbitrary axis in this candidate; no persistent sketch link |
| 5 | Arbitrary sketch-path sweep | Partial | Numeric polyline and interpolated spline guide sweep in this candidate; no independent linked guide sketch |
| 6 | Arbitrary section loft | Partial | 2–8 numeric XY polygon sections at Z stations in this candidate; no arbitrary-plane sketch sources |
| 7 | Planar-face sketch attachment | Partial | Face-attached boss/pocket now support large upstream translations with new body-relative anchors and local 2D coordinates; source-linked standalone sketch extrusions now exist; limited witnessed-planar-face split/merge matching; no universal topology lineage |
| 8 | Fully constrained sketch solving | Partial | Interactive selected constraints and solver, without complete DOF/conflict workflow |
| 9 | Geometric constraint expansion | Partial | Basic geometric constraints; full arc/spline tangent/continuity work outstanding |
| 10 | Named-unit expressions | Partial | RC4 adds saved unit-aware expression graphs, recalculation, cycle/unit validation and transactional save/reopen; still not a universal expression framework for every feature type |
| 11 | Multi-body associative features | Partial | Editable linked Boolean DAG, atomic coordinated multi-result retargeting, cycle rejection, rollback, preview and persistence; external constrained-sketch extrusion and atomic sketch-driven additive/subtractive result bodies are supported; most non-Boolean cross-body operators remain absent |
| 12 | Part configuration variants | Partial | RC5 saves named unit-checked configurations; consolidated work adds read-only full-geometry rebuild of each saved variant and per-variant assembly BOM/volume/mass comparison. Cross-body simultaneous configuration tables and manufacturing sign-off remain |
| 13 | Geometry validity diagnostics | Partial | B-rep validity checks and inspection, not systematic repair suggestions |
| 14 | Detailed mass tensor analysis | Implemented for supported scope | Center of gravity, inertia tensor, principal axes and assembly rollup in verified v1.63 |
| 15 | User-defined materials | Partial | Materials and engineering inputs, not complete provenance/version-aware catalog |
| 16 | Static bracket precheck | Partial | Idealized cantilever bracket calculation, not FEA |
| 17 | Beam section library | Partial | Verified numerical section-property families, not standard-grade catalog |
| 18 | Shaft design study | Partial | Preliminary shaft bending/torsion calculator; keyway/strength workflows incomplete |
| 19 | Fastener sizing workflow | Partial | Basic bolt-group checks, not grade/preload specification system |
| 20 | Fit and tolerance specification | Partial | Tolerance-stack calculations; formal ISO fit designation remains |
| 21 | Feature-level engineering annotations | Partial | RC5 stable feature-ID design/inspection/process notes with revision stamps, audit and CSV; universal geometry-linked requirements/approval routing remain |
| 22 | Assembly coordinate systems | Partial | Assembly coordinate/joint guards; general authoring remains |
| 23 | Planar/cylindrical mate authoring | Partial | RC3 offers saved multi-connection rigid frame observations and repeat refresh; fully face-picked persistent references and general authoring remain |
| 24 | Closed-loop assembly solve | Partial | Rigid loops plus a bounded nonlinear revolute/prismatic articulated loop solver, joint limits, singularity/underconstraint checks and atomic placements. No persisted live joint-definition network or full authoring UI |
| 25 | Hierarchical subassemblies | Partial | Persistent occurrences and grouping exist; deep hierarchy not complete |
| 26 | Assembly drag/drive | Partial | Some basic assembly motion; interactive full joint drive not complete |
| 27 | Continuous interference refinement | Partial | Adaptive pair scan now subdivides risky/long motion intervals, includes saved keyframes and reports exhausted refinement budget. It remains sampled, not a certified continuous-path solver |
| 28 | Minimum-clearance envelopes | Partial | Static tolerance-aware fit audit plus bounded adaptive motion-pair sampling; no mathematically certified swept-volume clearance envelope |
| 29 | Exploded-view sequences | Outstanding | No full named exploded-sequence authoring |
| 30 | Assembly BOM revisions | Partial | BOM and metadata plus rebuilt saved-variant BOM/volume/mass comparison and source/BOM fingerprinted CSV; a cross-body part-number configuration/revision approval matrix remains incomplete |
| 31 | Sheet-metal flange authoring | Partial | Basic sheet-metal tools; complete edge-flange authoring outstanding |
| 32 | Folded-flat associativity | Partial | Folded/flat operations exist; universal associative regeneration incomplete |
| 33 | Sheet-metal process rules | Partial | Bend estimates, not process rules library |
| 34 | Welding workflows | Partial | Weld calculations only; annotations/drawing symbols incomplete |
| 35 | Hole tables and callouts | Partial | RC3 exact-geometry Z-axis axial-hole inspector and revisioned CSV; RC4 drilling-review export; general angled holes, drawing callouts and universal machining intent outstanding |
| 36 | CAM tooling library | Partial | RC4 adds user-entered diameter/reach catalogs for a limited, non-executable drilling feasibility review; no persisted production cutter library |
| 37 | 2.5D drilling toolpaths | Outstanding | No verified 2.5D drill CAM toolpaths |
| 38 | 2.5D pocket/contour CAM | Outstanding | No verified 2.5D pocket/contour CAM |
| 39 | Toolpath verification | Outstanding | No safe stock-removal collision replay |
| 40 | Machine-specific post-processing | Outstanding | No machine-ready postprocessor |
| 41 | Associative drawing projection | Partial | Orthographic views exist; associative projection incomplete |
| 42 | Persistent drawing dimensions | Partial | Some persistent drawing data; robust topology-linked dimensions incomplete |
| 43 | Section/detail view authoring | Partial | Basic drawing views; arbitrary section/detail linking outstanding |
| 44 | GD&T feature control frames | Partial | RC4 adds validated manually authored source-revision-linked text frames in SVG and native scene; no standards-compliant symbols, datum feature semantics or geometric tolerance solver |
| 45 | Manufacturing drawing templates | Partial | Basic sheet formats; workflow templates/revision zones incomplete |
| 46 | Searchable feature/task navigator | Partial | Searchable feature browser exists; broader task navigator incomplete |
| 47 | Selection and snap improvements | Partial | Face/edge selection and snap support; preselection/pick-through incomplete |
| 48 | Interactive property editing | Partial | Feature editor and rollback, not complete type-safe property sheet |
| 49 | Cross-workflow design report | Partial | RC5 document audit blocks stale or orphaned feature notes; design review/BOM/study exports exist, but formal approval integration remains |
| 50 | Performance and release CI | Partial | Windows Release verifier and CTests/JUnit; full automated UI/CI still pending |

A full v1.70 release requires all 50 targets to meet their original acceptance criteria, native Windows build, test suite, GUI smoke, persistence, and safety-related CAD workflows. **This development candidate does not meet that definition yet.**

## Latest separate v1.70 increment: live external sketch extrusion (106-test candidate)

This increment adds one-input external sketch links, edits, transitive Boolean regeneration and saved-scene validation. See `V170_EXTERNAL_SKETCH_DEPENDENCY.md`.

## Previous increment: assembly fit audit (104-test candidate)

This increment improves assembly design review, not global sketch/feature dependency solving. See `V170_ASSEMBLY_FIT_AUDIT.md` for its exact scope and verification limits.


### 2026-11-06 continuation: three reference improvements (source candidate)

Additional bounded progress: v3 witnessed B-spline/Bezier edges (projection + tangent + curvature), exact translation/rotation stages in tracked mixed histories, and cylinder/sphere/cone/torus witness-based shell-opening selection and Boolean/rigid rebasing. Unknown or ambiguous successors fail without committing. **Do not interpret this as universal topological naming, universal spline-face history, or a Windows-tested release.** See `V170_SPLINE_RIGID_CURVED_SHELL_INCREMENT_20261106.md`.

### 2026-11-06 consolidation RC2 (development source only)

Selective external-source DAG cache-planning (new portable API), advisory
sketch redundancy diagnostics, and one document-scoped local-frame rigid mate
network (save/reopen, explicit refresh, undo/redo) advance several *partial*
roadmap goals. **They do not complete** full cross-body integration, universal
geometric references, interactive live mate-network authoring, GD&T, sheet-metal
production, or CAM. See `../V170_CONSOLIDATED_ENGINEERING_RC2_20261106.md`.
All native Windows build/CTest and GUI verification remains pending.

### 2026-11-06 consolidation RC3 (single giant source update; Windows not verified)

Includes multi-link authoring/removal for the saved rigid mate graph, a persisted
refreshable articulated-network model with driven coordinate editing and
transactional undo/redo, and a conservative revision-stamped axial-hole CSV
inspection. These reduce the previously partial gaps in roadmap targets 23–26,
35 and 42, but **do not close** the full target definitions or certify this
release. Full native Windows build/CTests and GUI acceptance remain pending.
See `../V170_CONSOLIDATED_ENGINEERING_RC3_GIANT_20261106.md` for precise scope.

### 2026-11-06 RC4 large consolidation (unverified development source)

- Persistent unit-aware dimension formulas, regenerated when sources change, cycle rejection, complete save/reopen and atomic rollback; limited to supported parametric dimensions.
- Machine-neutral drilling feasibility worksheet from fresh measured Z-axis holes, tool diameter and reach screening, explicit fixture warnings, revision stamping, spreadsheet-safe CSV, and atomic export. **This is not CAM, G-code, process approval, machine safety, or collision checking.**
- Manually authored geometric-tolerance annotations with unique IDs, 3 optional datum letters, drawing position and source-body token; exported SVG refuses stale source tokens. **Not ASME/ISO GD&T compliance or automatic interpretation.**
- Portable source tests and regression additions are not MSVC/Qt GUI verification. These remain release blockers.

For an honest completion figure, the 50-row status ledger counts targets as *completed only if their full acceptance scope is implemented*. Some partially developed targets now have improved coverage but are not accepted as complete. Do not equate a feature's presence, number of lines changed, or percentage of requirements with a working 85%-complete release.

### 2026-11-06 consolidation RC5 (source-level development, no native sign-off)

Project-native named configurations (target 12), revision-stamped stable-ID
feature-level engineering notes (target 21), and blocking stale/orphaned-note
project audit (target 49) advance those requirements from earlier partial/
outstanding positions to **bounded partial implementations**. New native
regressions are written but not executed; portable and Python/source checks
are described in `../V170_CONSOLIDATED_ENGINEERING_RC5_20261106.md`.
**Do not claim 85% accepted roadmap coverage or Windows release readiness.**

### 2026-11-06 drawing and manufacturing correctness increment (source candidate)

A drawing-only, bounded circular radius-change resolver can update a linked
full-circle diameter or hole location through a supported diameter edit, while
ordinary feature/machining references remain strict. The Guided engineering
studies catalog includes a three-contributor linear tolerance-stack study with
a deterministic worst-case clearance margin. Portable/native-source and
independent OpenCascade regressions were expanded. **Native MSVC/Qt/OpenCascade
compilation and CTests remain unexecuted; roadmap acceptance is not increased
merely by adding these bounded functions.** See
`V170_CIRCULAR_DRAWINGS_AND_TOLERANCE_STACK_20261106.md`.

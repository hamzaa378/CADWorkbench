# CADWorkbench v1.70: 50 substantial engineering/CAD capabilities — delivery roadmap

See [current implementation status](V170_IMPLEMENTATION_STATUS.md) before interpreting any target as completed.

These are **50 target improvements**, not 50 completed changes. Each requires working application behavior, persistence when applicable, failure-path coverage and a native Windows Release build/CTest gate. Work in smaller, reviewable increments to protect the latest passing baseline.

Legend: `Planned` means **not delivered by this roadmap**. Some categories already have partial precursor tools in v1.60; the planned items aim at deeper, production-grade behavior. The v1.61 calculation workbench is an additional delivered subsystem, not a claim that these items are finished.

| # | Major target | Acceptance criteria | State |
|---:|---|---|---|
| 1 | Stable topological naming | References survive deterministic face splits/merges; clear ambiguity errors | Planned |
| 2 | General parametric feature graph | Feature nodes accept different body/sketch source types, cycle detection | Planned |
| 3 | Selective graph rebuild | Only affected descendants recompute; cache invalidated correctly | Planned |
| 4 | Arbitrary sketch profile revolve | Closed user sketch revolves around selected axis and remains editable | Planned |
| 5 | Arbitrary sketch-path sweep | Profile and spline/arc guide remain linked after editing | Planned |
| 6 | Arbitrary section loft | Multiple user-specified sketches drive loft geometry and persistence | Planned |
| 7 | Planar-face sketch attachment | Moving/rotating parent face updates child sketch placement | Planned |
| 8 | Fully constrained sketch solving | DOF display, conflict isolation, deterministic underconstrained behavior | Planned |
| 9 | Geometric constraint expansion | Tangent and continuity constraints on circles, arcs and splines | Planned |
| 10 | Named-unit expressions | Formulas, dimensional consistency, cyclic dependency diagnostics | Planned |
| 11 | Multi-body associative features | Cross-body pattern/pocket/fillet dependencies rebuild transactionally | Planned |
| 12 | Part configuration variants | Persistent dimension/configuration tables and variant comparison | Planned |
| 13 | Geometry validity diagnostics | Solids/manifolds/self-intersections and repair suggestions | Planned |
| 14 | Detailed mass tensor analysis | Principal inertia, local axes, center of gravity and units | Planned |
| 15 | User-defined materials | Traceable density, Young's modulus, strength, version and provenance | Planned |
| 16 | Static bracket precheck | Explicit boundary/load cases, compare to formula assumptions | Planned |
| 17 | Beam section library | Standard section profiles and section-property verification | Planned |
| 18 | Shaft design study | Torque, combined bending, keyway effect and shaft sizing reports | Planned |
| 19 | Fastener sizing workflow | Bolt grade, thread, preload range, shear/tension interaction | Planned |
| 20 | Fit and tolerance specification | Standardized hole/shaft designations and stack-up reports | Planned |
| 21 | Feature-level engineering annotations | Stress/fit/material notes linked to body/feature IDs | Planned |
| 22 | Assembly coordinate systems | Local coordinate frames editable and reusable across joints | Planned |
| 23 | Planar/cylindrical mate authoring | Interactive selection of matching faces and constraint setup | Planned |
| 24 | Closed-loop assembly solve | Detect incompatible loops and report residual/DOFs | Planned |
| 25 | Hierarchical subassemblies | Nested components with local transformations and reuse | Planned |
| 26 | Assembly drag/drive | Interactive joint motion with hard limits and edit persistence | Planned |
| 27 | Continuous interference refinement | Adaptive collision sampling beyond only discrete keyframes | Planned |
| 28 | Minimum-clearance envelopes | Assembly motion clearance by moving pair and result visualization | Planned |
| 29 | Exploded-view sequences | Named explode/restore states and assembly drawing integration | Planned |
| 30 | Assembly BOM revisions | Configuration-specific BOM rollups and provenance of changes | Planned |
| 31 | Sheet-metal flange authoring | Sketch-edge flanges, bend direction, relief and corner treatments | Planned |
| 32 | Folded-flat associativity | Modify folded state and update flat pattern automatically | Planned |
| 33 | Sheet-metal process rules | Tooling limits, bend-radius tables and K-factor library per material | Planned |
| 34 | Welding workflows | Weld definition, throat annotation, joint length and drawing symbol | Planned |
| 35 | Hole tables and callouts | Recognize and annotate drilling/thread dimensions across views | Planned |
| 36 | CAM tooling library | Cutter definitions, feeds/speeds, stock, fixture and safe heights | Planned |
| 37 | 2.5D drilling toolpaths | Drill/spot/counterbore cycles with preview and simulation checks | Planned |
| 38 | 2.5D pocket/contour CAM | Tool clearance, stock remainder and collision checks | Planned |
| 39 | Toolpath verification | Stock removal and fixture collision replay | Planned |
| 40 | Machine-specific post-processing | Explicit postprocessor selection, G-code validation and dry-run | Planned |
| 41 | Associative drawing projection | Auto-refresh orthographic/auxiliary views after model edits | Planned |
| 42 | Persistent drawing dimensions | Face/edge references that survive supported model edits | Planned |
| 43 | Section/detail view authoring | Arbitrary cut lines and linked details with visible-hidden control | Planned |
| 44 | GD&T feature control frames | Datum labels, geometric tolerances and standards-driven validations | Planned |
| 45 | Manufacturing drawing templates | Project sheet formats, revision zones and approval workflows | Planned |
| 46 | Searchable feature/task navigator | Context-aware commands, error indicators and feature descriptions | Planned |
| 47 | Selection and snap improvements | Body/face/edge/vertex filtering, preselection and pick-through | Planned |
| 48 | Interactive property editing | Type-safe property widgets and preview/rollback on invalid edits | Planned |
| 49 | Cross-workflow design report | CAD mass/BOM + engineering estimate + drawing references and limits | Planned |
| 50 | Performance and release CI | Full MSVC build, native tests, UI smoke, packaging and reproducibility | Planned |

## Release gates

Every incremental candidate should pass: (1) CMake configure; (2) MSVC Release compile; (3) all native CTests and new focused regressions; (4) CMake/Qt UI smoke when touched; (5) project load/undo/save; (6) ZIP integrity and manifest; and (7) a change-list comparison with the previous verified source tree. Static checks alone are never a sufficient release gate.

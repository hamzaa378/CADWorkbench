# CADWorkbench v1.70 vs Autodesk Fusion — desktop-only capability gap

Comparison date: 2026-11-07. Source: this candidate's C++ code and its documented scope, plus official Autodesk Fusion product features and workspaces:
- https://www.autodesk.com/in/products/fusion-360/features
- https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-WORKSPACES.htm

**Definition:** Compare the desktop/local engineering tasks available in Autodesk Fusion, **excluding cloud collaboration, cloud project storage, cloud lifecycle management, and cloud-hosted computation**. This does **not** exclude manufacturing CAM, electronics/PCB, local rendering, assembly motion, local simulation, or offline modeling capabilities. Some Fusion advanced features depend on product tier/extension; this is a broad Fusion capability benchmark, not a license-specific reproduction list.

## Why a percent can only be approximate

The number of menu entries or template parts is not a meaningful fraction of a professional CAD product. A fully dependable parametric sketch, associative drawing or postprocessed toolpath requires geometry, failure handling, persistence, test fixtures, user interactions and reliable performance. This is an informed **planning rubric** rather than an independently measured benchmark of completed engineering workflows. A score requires side-by-side task acceptance for production-level confidence.

| Offline capability | Weight | Indicative CADWorkbench credit | Main remaining gap |
|---|---:|---:|---|
| Parametric solid history, direct/modeling tools | 25 | 11 | Broad connected feature-timeline editing, stable topology across complex operations, robust error healing |
| Sketch editing, geometric/dimensional constraints | 12 | 5 | Full on-canvas constraint authoring, diagnostics/DOF, tangency, arcs/splines, stable face-linked sketches |
| Assemblies, joints, motion | 12 | 5 | Persisted live mate graphs, interactive constrained dragging, broader loop solving, nested components |
| Manufacturing drawings, GD&T | 10 | 4 | Editable persistent dimensions, manufacturing-grade drawing generation, standards and downstream updates |
| Surface, mesh and sheet metal | 10 | 4 | Mature freeform/T-spline equivalent, generalized mesh editing, associative flanges/bends/flat patterns |
| CNC CAM and verification | 12 | 1 | Actual 2.5D/3D toolpaths, tool/stock setup, safety simulation, posts; existing machining calculators are not CAM |
| Engineering simulation / FEA | 8 | 0.5 | Meshing, loads/constraints, structural/static/thermal solvers, validated result viewers |
| PCB schematics, routing and board design | 5 | 0 | No comparable ECAD workflow |
| Rendering and design animation | 3 | 0.5 | Local render/appearance workspace and assembly animation tooling |
| Interoperability, automation and workflow polish | 3 | 1.5 | Industry-format fidelity, API/macros, discoverability, installation and ergonomic UI |
| **Total** | **100** | **32.5** | **Indicative only; not a test-derived achievement percentage** |

Broadly, this suggests **roughly one-third of the full offline Fusion task capability/maturity**, with major uncertainty. Restricting the comparison to **mechanical geometry, sketches, basic assemblies and drawings** produces a higher qualitative estimate (around **40–50%**, not independently benchmarked). The current source's 130 native tests are important engineering regression gates, but not evidence of Fusion feature parity.

## A credible 80% destination

An 80% **task-coverage** objective needs a dedicated representative suite: sketch/edit a fully constrained plate, change the defining sketch after downstream fillets/holes, make multi-body top-down assemblies, drag a closed-loop mechanism, author associative toleranced drawings, flatten a flange-based sheet-metal design, import/edit/export a complex neutral CAD file, create verified 2.5D/3-axis CNC output, and run trustworthy linear-static/thermal simulation. A broad Fusion benchmark also includes PCB schematic/layout and visualization.

**Priority work packages** (dependency order):
1. Correctness and reference identity: stable face/edge/vertex ancestry, canonical feature DAG + transactional rebuild/undo/save, geometry diagnostics and selection API.
2. Modeling/UI depth: interactive constraint solver and freedom diagnostics, sketched arcs/splines, profile/guide driven sweeps and lofts, advanced editable features.
3. Assemblies: persistent joints/mates, hierarchical subassemblies, drag/drive, interference and collision/clearance studies.
4. Drawing and sheet metal: true associativity, drafting dimensions, GD&T, flange/bend/relief rules, reliable flat patterns.
5. Manufacturing: tools/stock/fixtures, 2.5D and 3D toolpath generation, verification and safe postprocessing; do not equate an estimate calculator to CAM.
6. Offline CAE + remaining categories: meshing, material/load/constraint models, validated solvers and reports; local rendering/animation; decide explicitly whether integrated PCB is part of the target.
7. Continuous acceptance: actual complex user projects, UI stress tests, native rebuild tests, neutral file roundtrips, performance and installer reliability.

**Estimated remaining effort:** for broad 80% offline Fusion workflow coverage, a *rough planning* allowance is **100–220 engineer-months** of coordinated modeling, geometry, UI, manufacturing, simulation and QA work, with substantial uncertainty and possible third-party technology/licensing dependency. This can be multiple years even with a 5–8-person experienced engineering team. A narrower 80%-of-core-mechanical-CAD target (excluding CAM, CAE, PCB and photoreal rendering as well as cloud) might require roughly **40–90 engineer-months**. These are estimates, not guaranteed schedules or validated cost forecasts.

### Distinguish feature coverage from quality parity

80% task coverage does not establish 80% precision, robustness, production readiness or reliability. Acceptance should include tolerance to invalid geometry, destructive edits, high-body-count scenes, old project files, representative customer work, and verified data roundtrips. The source is **not yet 130/130 native-test verified**, so even its existing documented functions need the remaining Windows verification.

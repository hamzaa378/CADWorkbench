# CADWorkbench v1.62 — tested engineering design foundation increment

**Scope:** A stable development increment toward the 50-target v1.70 roadmap. This version does **not** implement all 50 roadmap targets and is not a v1.70 production release.

## Working code added

1. `ParametricStudy`: Qt-independent unit-aware arithmetic parser for formulas, typed physical dimensions, named-variable resolution, cycle detection, unknown-symbol rejection, division-by-zero checks, transaction-safe result evaluation, and numerical bounds. Input length and expression-complexity limits are enforced. Units include mm, cm, m, in, ft, N, kN, Pa, kPa, MPa, GPa, s, min, kg, g, Kelvin differences, degrees, and radians. Underlying basis: mm, N, s, K. This is a preliminary engineering math engine, not a complete SI unit system.
2. Parametric dimension editor: existing named parameters can be edited with formulas and referenced dimensions, previewed, then evaluated and applied using the existing transactional `setNamedParameters` operation. Existing constraint-linked dimensions remain read-only. **Formula text is not stored in the CAD project**; only its evaluated numeric geometry parameters are saved.
3. Design configuration study: named parametric variants can be previewed and validated, and one can be applied to the selected model. Explicit `.cadcfg` file import/export stores sidecar expressions. Configurations do not persist inside the `.cad` document, do not cross-reference other bodies, and cannot apply to arbitrary imported geometry.
4. Feature dependency planner: standalone iterative directed acyclic graph validator, cycle/missing-reference checks, and selection of affected descendants. Includes testing of 5,000-node chains. **The planner is not wired into the existing OCCT feature executor**, so this does not introduce selective B-rep regeneration or incremental caches into the application.
5. `GuidedEngineeringStudies`: ten independent multi-result workflows for rectangular cantilever brackets, simply supported beams, combined bending/torsion shafts, axial/shear bolt groups, paired fillet welds, double-shear pins, ideal Euler columns, diametral fits, single sheet-metal bends, and drilling feed-time/peck overhead. Each displays explicit assumptions and omissions. These are engineering estimates, **not FEA, CAM toolpaths, a joint solver, or certified design tools**.
6. New searchable workbench commands under **Inspect → Guided engineering design studies**. Uses selected-body bounding dimensions only for matching dimension fields. It cannot infer effective load paths or section modulus from complex CAD geometry.
7. A read-only **Inspect → Engineering design review and export** report combines the actual BOM, saved material assignments, feature/link staleness flags, drawing metadata hints and whole-project audit findings into a TXT report. It does not silently certify suitability for manufacture.
8. `MaterialRecord` validation and CSV export with provenance/grade/revision checks, duplicate-ID rejection and spreadsheet formula neutralization. The catalog engine is accessible to C++ callers; it is **not yet a persistent materials editor in the CAD GUI**.
9. `scripts/verify_release_windows.ps1`: fail-fast MSVC Release build, exact native CTest count discovery, full test run with JUnit result and executable SHA-256. The release gate cannot be certified from Linux.

## User paths to try on Windows

1. Extract the source ZIP; run `$env:VCPKG_ROOT='C:\vcpkg'` then `powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1`.
2. Create a parametric rectangle body, select it, and use **Edit → Edit named parameters**. Enter `width = 100 mm`, `height = width/2`, and validate. Apply, undo and redo. The form must reject `width = height` together with `height = width` and reject mixing `mm` with `MPa`.
3. Use **Edit → Design configurations and variants** to preview and apply variants, then export/reload the `.cadcfg` sidecar. Invalid geometry should leave the document unchanged.
4. Choose **Inspect → Guided engineering design studies**, change a cantilever load and observe its stress/deflection. Export the report.
5. Choose **Inspect → Engineering design review and export** on a scene with multiple bodies and material assignments. Check the report's audit, BOM, and linked-feature status.

## Verification contract

- Linux portable code: compile and execute each of `parametric_study_v170_tests.cpp` and `guided_engineering_studies_v170_tests.cpp` using GCC, Clang and sanitizers.
- Source wiring/preflights: `python tests/v162_engineering_expansion_preflight.py` plus existing suites.
- CMake source: parsed to the intended Windows-only guard on Linux. That **does not** verify Qt/OCCT native compilation.
- Windows acceptance: **81 registered native tests**, all passing, plus a UI smoke test covering new dialogs. **No Windows build was performed in this environment**.

## Not implemented

Full OCCT topological naming; arbitrary-sketch revolve/sweep/loft; fully constrained spline/arc sketch solver; actual cached selective geometry regeneration; multi-body universal dependency execution; closed-loop assembly solver; collision-checked CAM paths; machine postprocessors; associative section/detail drawings and GD&T validation. Those targets remain open and are tracked in `V170_IMPLEMENTATION_STATUS.md`.

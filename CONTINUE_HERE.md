# CADWorkbench source handoff

This is the current **v1.55.0 source working tree** on 3 November 2026 (India time). The source ZIP is a snapshot of file contents, including files that were staged, unstaged, or untracked in the local Git checkout. It deliberately omits `.git`, compiled binaries, dependency installs, and caches. The checkout base commit was `b050654e5ff948302ee10357d39e2c6554bb9c93`; the working-tree changes have **not** been made into a new commit.

## Start here in another chat

1. Extract the ZIP and point the chat at the extracted `CADWorkbench_v1_55_Source` folder, or work in the existing checkout if it is still available.
2. Read this file, `CMakeLists.txt`, `README.md`, and the source files for the area being changed. Treat older feature tables and QA notes as historical where they disagree with this handoff.
3. Preserve the current source. Do not reset to the base Git commit: that would discard the features summarized below.
4. On Windows, set `VCPKG_ROOT` to a bootstrapped vcpkg checkout and run `./scripts/build_windows.ps1` in PowerShell. It configures, builds, and runs CTest. A C++20 MSVC toolchain, Visual Studio CMake tools, Qt 6, and OCCT 8 through `vcpkg.json` are required.
5. If using the existing local build, its directory is `../build_v148_combined`. The ZIP does not include that directory, so a fresh extraction must build dependencies and compile again.

Suggested message for the next chat: **“Continue CADWorkbench from this source snapshot. Read CONTINUE_HERE.md, inspect the code for the area you choose, implement a substantial next CAD improvement, and verify it with a Windows Release build and affected regression tests. Preserve all current source changes.”**

## Verified state of this snapshot

- Windows x64 Release `ALL_BUILD`: **passed**.
- CTest: **72/72 passed**, most recently on 3 November 2026. The suite includes the long geometry/persistence case and focused sketch, surface, mesh, document-revision, assembly, drawing, parametric, BOM, and sheet-metal cases.
- The current interactive application UI was **not** live-tested in the last coding rounds. Menu commands compiled; GUI behavior needs separate verification before claiming it was observed live.
- Native source, CMake setup, vcpkg manifest, PowerShell scripts, C++ tests, supplemental Python source tests, and current documentation are in the ZIP. Runtime Qt/OCCT DLLs and an installer are **not** part of this source ZIP.

## Important implemented additions beyond older v1.55 notes

| Area | Current implementation | Main files |
|---|---|---|
| Sketches | General numeric 2D point/segment/circle solver, radius/equal-radius/concentric/point-on-circle/line-circle tangency, point symmetry about a line, external/internal circle tangency, finite point-on-segment, constraint persistence, and editable constrained polygon extrusions with circular cutouts. Solver rejects inconsistent and degenerate cases. | `src/core/SketchSolver.*`, `src/core/SceneDocument.*`, `tests/sketch_solver_tests.cpp` |
| Parametric history | Editable rectangle extrusion, through/blind hole, X/Y linear hole patterns including blind holes, named dimensions and links, all-edge chamfer/fillet, optional-step suppression, and transactional batch suppression with one undo step. It remains a bounded chain. | `src/core/SceneDocument.*`, `src/ui/MainWindow.cpp`, `tests/cadcore_tests.cpp` |
| Assemblies | Reusable occurrences support XYZ placement and roll/pitch/yaw orientation, saved rigid/revolute/prismatic joints, motion tracks, BOM-linked definition synchronization, and sampled motion interference scanning. | `src/core/SceneDocument.*`, `src/ui/MainWindow.cpp`, `tests/cadcore_tests.cpp` |
| Drawings | Persistent source-linked sheets, selected-edge dimensions guarded by geometry revision, SVG/PDF export, and editable DXF export with separate geometry/hidden/center/dimension/detail layers and clipped detail linework. Exports remain snapshots. | `src/core/DrawingSheet.*`, `src/core/DrawingDxf.cpp`, `src/ui/MainWindow.cpp` |
| Sheet metal | Editable faceted folded bodies, per-bend K factors, optional rectangular bend-end reliefs in folded flat-pattern DXF, and bend-table CSV with signed directions, radii, allowances, deductions, and stations. | `src/core/SheetMetal.*`, `src/ui/MainWindow.cpp`, `tests/cadcore_tests.cpp` |
| Other areas | STEP assembly exchange, BOM/parts lists and configurable drawing checklists, persistent surface bodies, mesh repair/intake, and named offline document revisions are present and regression-covered. | `src/core/SceneDocument.*`, `src/core/SurfaceModeling.*`, `src/core/MeshRepair.*`, `src/core/DocumentRevisionHistory.*` |

## Boundaries and next work

- The sketch solver covers points, segments, and circles; it does not yet provide drawn arcs, splines, or a full sketch-canvas editor. The parametric timeline is not a general dependency graph and cannot arbitrarily reorder or insert operations.
- Assembly solving is bounded; closed-loop constraint solving, mating workflows, and comprehensive collision/contact studies remain open. The interference scan samples specified motion positions.
- Saved drawing records are linked to model identities and revisions, but exported SVG/PDF/DXF files do not update themselves. More edge/face dimension types, arbitrary sections, and automatic sheet refresh remain open.
- Folded bodies are faceted; flat-pattern export is a developed snapshot. Exact analytic bends, flat-to-folded associativity, springback, tooling compensation, and formal manufacturing validation remain open.
- The ISO/ASME project checks are configurable completeness checks, not a claim of formal standards compliance. CAM, FEA, and cloud workflows are not included.
- `docs/README_HISTORY.md`, `docs/QA_MATRIX.md`, `docs/local_verification.json`, and some older feature tables contain stage-specific statements such as “blind holes cannot be patterned” or “folded DXF has no relief.” Those statements predate this working tree. Use the root README, code, and fresh tests as the authority for current behavior.

## Rebuild and publish

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\build_windows.ps1
```

For a manual regression run after a successful build:

```powershell
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
```

The source ZIP is ready to **extract and upload as repository files**. GitHub does not expose or compile source contained only inside a ZIP. The repository has no project license file yet; choose one before presenting the code as an open-source release. The existing `scripts/package_release_windows.ps1` is for a separate portable app and installer workflow and requires an installed Windows Qt/OCCT runtime.

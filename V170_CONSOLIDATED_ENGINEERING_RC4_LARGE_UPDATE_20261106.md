# CADWorkbench v1.70 — Consolidated Engineering RC4 (large update)

**2026-11-06 | Development source candidate | NOT a final Windows-verified release | One ZIP only**

This update continues from the complete RC3 source without replacing the last Windows-verified executable. Its aim is to close linked engineering workflows rather than expand the menu with unrelated prototypes. It **does not establish 85% completion of all 50 original targets**. Read `docs/V170_IMPLEMENTATION_STATUS.md` for the acceptance criteria and remaining work.

## A. Parametric dimensions that remain live after edits and reopen

- The supported parametric body stores an optional `parametric_definition.dimension_formulas` map in the native `.cadscene` project. The saved format is backward compatible with projects that do not contain it.
- The named-parameter editor loads saved expressions, validates physical dimensions, and saves the expressions rather than only their evaluated number.
- Driving dimensions can be explicitly overridden; dependent formulas reevaluate. Incompatible parameter links, cycles, unit mismatches, unknown symbols, invalid final solid geometry and inconsistent saved numeric snapshots fail with no partial B-rep commit.
- Undo/redo, save/reopen, and rejected-load rollback have native C++ regression cases added to the **existing** `cadcore_parametric_foundation` CTest executable. These native additions **must run on Windows** before release.
- Scope: the currently supported parametric dimension variables. This is **not** an arbitrary geometric-constraint/feature expression compiler for all CAD commands.

## B. Drilling feasibility from measured part geometry (non-executable)

- New standalone `src/core/DrillingFeasibility.h/.cpp` accepts a revisioned inspected stock envelope, measured axial holes and a manual drill-diameter / usable-length catalog.
- Screens unique top-entry Z-axis holes for nominal diameter match, tool reach, and clearance-plane requirements. Through holes remain **conditional** on separate fixture review even if the operator checks the review box; no manufacturing operation is approved.
- Emits a deterministic candidate visit order and XY review distance, individual rejected-hole reasons, source body and geometry revision, excluded-cylinder count, and a conspicuous **NOT MACHINE INSTRUCTIONS** disclaimer.
- `src/core/DrillingWorksheet.h/.cpp` connects the portable planner to fresh OpenCascade hole inspection and a `QSaveFile` atomic CSV export. The File menu exposes the bounded review workflow.
- CSV text is hardened against spreadsheet formula injection. A bad tool list or invalid input does not overwrite an existing report.
- This deliberately **does not** generate CAM, G-code, validated collision-free toolpaths, approved feeds/speeds, fixture checks, or machining instructions. Only supported cylindrical through/blind Z-axis faces are identified.

## C. Source-linked manually authored geometric-tolerance annotations

- `DrawingGdtFrameAnnotation` stores unique ID, source body, exact source geometry token, characteristic, declared tolerance, optional ordered A/B/C datum letters and paper coordinates.
- The drawing editor can add/remove up to eight frames; native project save/load and validation reject duplicate IDs/datums, invalid tolerance and malformed JSON.
- Orthographic SVG regeneration emits the frame and its source metadata. It **refuses** a stale or missing source body instead of silently claiming that the engineer's tolerance still applies.
- These are **human-authored text frames**, not automatically calculated geometric tolerances and not a complete ISO 1101 / ASME Y14.5 symbolic-authoring or verification engine.

## Tests added / integrated

- Portable: `tests/drilling_feasibility_v170_tests.cpp`, run from `scripts/verify_portable_consolidation.sh` with GCC and Clang. 537 new screening, deterministic behavior and failure-safety assertions per compiler.
- Native source: `tests/cadcore_tests.cpp` now exercises persistent formula edits, cycles, corrupt-file rejection, undo/redo, CAD geometry-to-drilling worksheet, atomic export, manually authored GD&T, save/reopen, datum validation and stale-source rejection.
- The new native cases run inside the **already registered** `--parametric-foundation` and `--orthographic-drawing` modes; source still contains **130 CTest registrations**.
- Source integration: `tests/v170_rc4_consolidation_preflight.py` (5 tests) checks wiring to core, UI, exports, persistence and existing CTest registrations.

## Verification run in this development environment

| Check | Result |
|---|---|
| Python unittest (source + independent geometry checks) | **305 passed** |
| GCC C++ portable, ASan + UBSan | **1,100 passed** (537 drilling, 377 graph, 17 sketch, 41 ancestry, 80 linked bodies, 21 rigid mates, 27 articulated mates) |
| Clang C++ portable, ASan + UBSan | **1,100 passed** (same distribution) |
| CMake Windows test registrations | **130** present in source |
| Linux full CMake configure | Stops at intentional Windows-only guard (`CMakeLists.txt:10`) |
| Native MSVC + Qt6 + OpenCascade compilation | **NOT RUN** |
| 130 native CTests, UI, save/reopen application acceptance | **NOT RUN** |

**Do not interpret the portable or source tests as full application compilation.** The new Qt and OpenCascade integration must build and run on Windows before promotion.

## Run native acceptance on Windows

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Also inspect the File > drilling worksheet, saved named formulas, geometric tolerance editor, SVG/PDF export and updated drawings, invalid project loading, undo/redo, repeated assembly refresh, and stale reference safeguards interactively. Keep the most recently *Windows-verified* executable separate until this run passes.

## Original v1.70 requirements still preventing 85%-complete sign-off

Full arbitrary B-spline/split/merge topological identity; universal multi-body parametric regeneration through every feature type; complete sketch solver and interactive constraints; all face-picked live mates; general animated mechanisms; sheet-metal rules/flat-fold associativity; automatic standards-correct GD&T and general hole callouts; executable CAM for drilling/pocket/contour with collision proof, stock-removal checks and postprocessing; complete automated Windows UI and release verification. Some have limited implementations; limited implementation **is not full acceptance**.

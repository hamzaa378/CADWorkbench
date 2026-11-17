# CADWorkbench v1.70 — Consolidated engineering RC3

**2026-11-06 | Development source candidate, NOT a final Windows-verified v1.70 release**

This single full-source snapshot consolidates RC2 and several high-impact changes in parametric/assembly modeling and manufacturing inspection. It is intentionally delivered as **one ZIP**, not a succession of intermediate archives. The latest user-confirmed executable remains the fallback and is not replaced by this package.

## Priority 1: parametric modeling and rebuild correctness

- Retains RC2's external-source-aware selective dependency planner, source-revision invalidation, sketch redundancy diagnostics and previous witnessed-reference and multi-stage feature lineage work.
- When modifying saved articulated driving coordinates, the new `stageLiveArticulatedRegeneration` calculates *all* dependent placements against the original reference poses and current fixed occurrence. The scene, saved joint value, geometry revisions, and undo state change in **one transaction** only after every staged OpenCascade result validates.
- A repeated refresh is a no-op when geometry is already correct. Failed edits do not partially update dependent bodies. This is scoped to persisted articulated networks; **it is not a universal mixed-feature rebuild scheduler**.

## Priority 2: engineering workflows

### Saved multi-connection rigid mates

- Extend an existing document-scoped live rigid mate graph from the Assembly menu by selecting another component pair and saving three-point (or supported point/axis/plane) observations.
- Remove individual connections only when the remaining graph still constrains all previously linked occurrences. Reject duplicate parent-child pairs and ambiguous/disconnected networks. Save/reopen and undo/redo protect these definitions.
- Constraints use component-local numerical observations, **not persistent picked-face references**. The solver is limited by the existing component, constraint, and network bounds. Refresh remains explicit.

### Articulated mechanism authoring and editing

- Document-scoped local-reference articulated networks preserve the fixed component ID, joint definitions, and original component frames in a strictly validated, optional `live_articulated_mates` section of v24 `.cadscene` projects.
- Added an Assembly-menu UI for saving a driven fixed/revolute/prismatic pair, refreshing or removing the network, and **editing saved driven revolute/prismatic coordinates**. The stored model layer also supports bounded multiple-joint networks and closed-loop solving. Full graphical multi-link authoring, picked face/axis mating, and universal motion playback are **not complete**.
- Driven edits use radians in the core, degrees in the UI for revolute joints, and millimetres for prismatic joints. Each edit is limit-checked, atomic and undoable, including no-op geometry with changed definitions. Inputs cannot arbitrarily alter the original source frames.

### Conservative Z-axis hole-table audit

- New `cad::inspectAxialHoles`, `buildAxialHoleTableCsv` and `exportAxialHoleTableCsv` inspect the current valid single solid with OpenCascade and export a revision-stamped CSV.
- Recognizes only supported complete Z-axis internal circular cylindrical walls with two verified full circular boundary rims. Reports center X/Y, diameter, axial Z extents and whether the bore spans the **global** stock envelope. Blind bores are reported separately. The CSV includes exclusion counts and guards against spreadsheet formula injection in metadata; export uses `QSaveFile` to avoid partial files.
- Rejects non-Z bore orientations, exterior cylinders, partial walls, multi-solid ambiguous stock and invalid shapes. This is a **geometric inspection aid**, not a validated hole table with engineering design intent, drilled-depth symbol conventions, GD&T, tool selection, safe G-code, or manufacturing authorization.

## Native regression source additions

- `tests/assembly_mate_document_v170_tests.cpp`: extend/remove rigid connections, graph connectivity, loops, save/reopen, rollback.
- `tests/articulated_assembly_scene_v170_tests.cpp`: original-frame save/reopen, repeated refresh, corrupt-record rejection, undo/redo, driven-angle edit and reset, rejected out-of-range edit and atomic persistence.
- `tests/cadcore_tests.cpp`: through/blind/exterior cylindrical inspection, revisioned export and spreadsheet hardening, multi-solid rejection and previous-result preservation.
- `tests/v170_rc3_integrated_preflight.py`: independent OpenCascade geometry reference examples plus narrow source contracts.

These **native C++ test bodies are written but NOT executed** in this environment. They run within existing Windows CTest executables; the CMake source still registers exactly **130 CTests**.

## Executed verification, 2026-11-06

| Check | Result |
|---|---|
| Python unittest discovery, including independent OpenCascade reference geometry | 300 passed |
| GCC portable C++ with ASan/UBSan | 563 passed |
| Clang portable C++ with ASan/UBSan | 563 passed |
| CMake `add_test` registrations in source | 130 |
| Native MSVC + Qt6 + OpenCascade compilation | **Not run** |
| Native Windows CTests, GUI, persistence migration | **Not run** |

Python/source contract tests, portable planner tests and independent OpenCascade **Python** geometry references do **not** establish that modified `SceneDocument.cpp`, `MainWindow.cpp`, or `HoleTable.cpp` compile in the Windows application.

### Reproduce on Linux/macOS-like portable environment

```bash
python -m unittest discover -s tests -p '*.py' -q
CAD_SKIP_PYTHON=1 ./scripts/verify_portable_consolidation.sh g++
CAD_SKIP_PYTHON=1 ./scripts/verify_portable_consolidation.sh clang++
```

### Mandatory Windows acceptance before release

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Then run interactive GUI acceptance: multi-link rigid mates, live articulation editing, all save/reopen/undo/redo paths, defective JSON rollback, full and blind hole CSV export, drawing regeneration, user-selected face references and manufacturing safety tests.

## Unfinished original v1.70 targets

The original 50-target roadmap is **not closed**. Major gaps still include fully general cross-body feature regeneration and topology naming through arbitrary surface reconstruction, complete sketch constraint authoring, face-picked mates, complete interactive multi-link mechanism editing, universal sheet-metal flat associativity, full ISO/ASME GD&T feature-control frames and dimension tolerances, associative machining drawings/hole callouts, production CAM drilling/pocketing/postprocessing, verified toolpath collision replay and automated Windows release/UI testing. See `docs/V170_IMPLEMENTATION_STATUS.md` for the 50-item scope ledger (an intentionally conservative ledger whose item statuses must be reconciled against all recent source increments).

Do not identify RC3 as v1.70 final, or manufacture parts from an automatic hole audit without reviewing the geometry and drawings.

# v1.70 unreleased increment: editing source-linked manufacturing dimensions

**Status:** preliminary development source, not an accepted final CAD release. Built upon the 2026-11-07 live-mate-preview preliminary snapshot. Native Windows compilation/CTest are pending.

## User-visible behavior

In the Drawing menu, after selecting exactly one saved drawing-sheet owner body, choose **Edit saved linked dimension and tolerances...**. Select an annotation by its stable ID, kind, source body and view. The dialog lets you change:

- orthographic view (TOP, FRONT or RIGHT), provided the current kernel can render this dimension in that view;
- paper leader offset (5–60 mm);
- explicitly entered upper and lower bilateral tolerance magnitudes (0–1000 mm).

The selected source body, edge reference(s), measurement kind, geometry-witness maps, annotation ID and source-revision acknowledgement remain unchanged. To change the reference geometry or kind, the user must explicitly reselect and create a new dimension. If the dialog has no changes, no scene revision is recorded. A successful edit is a single undoable scene edit and persists through save/reopen.

Before committing any change, the editor uses the **same drawing SVG kernel and edge-witness resolver** as the output path, with only this candidate annotation in a read-only preview. A changed witness-free (legacy index-only) source is rejected as stale; witnessed references must resolve uniquely. Invalid circle orientations, missing references, negative tolerance values and unrenderable dimensions are rejected. This preflight intentionally does *not* acknowledge the drawing's aggregate source revision, and does not check unrelated annotations. The revised full sheet still must pass export and source-revision readiness checks.

## Code touched

- `src/core/DrawingSheet.h/.cpp`: `validateDrawingEdgeDimensionPreview()` helper with no scene mutations.
- `src/ui/MainWindow.h/.cpp`: selection-driven editor and menu command.
- `tests/cadcore_tests.cpp`: native dimension edit, preflight, source identity, save/reopen and undo/redo regressions inside the existing orthographic drawing CTest.
- `tests/v170_linked_dimension_edit_contract.py`: four source-integration guards.

## Verification performed

- `python -m unittest discover -s tests -p '*.py' -q`: 385 tests passed, includes Python/source contract checks and independent OpenCascade Python reference checks.
- `CAD_SKIP_PYTHON=1 bash scripts/verify_portable_consolidation.sh g++`: 12/12 targets passed with address and undefined-behavior sanitizers.
- `CAD_SKIP_PYTHON=1 bash scripts/verify_portable_consolidation.sh clang++`: 12/12 targets passed with sanitizers.
- `CMakeLists.txt` still registers 130 native CTests.
- **Unverified:** MSVC/Qt6/OpenCascade compilation, main GUI interaction, and all native Windows CTests, including `cadcore_v170_interleaved_history` and `cadcore_orthographic_drawing` (new tests).

## User-approved final release requirements

Two of three workstreams >=85% accepted, the remaining >=65%, with a passing native Windows build and every mandatory test. Current working estimates remain approximately P=60%, A=60%, D=40–45%. These are rough scope estimates, *not* scored 20-scenario acceptance results. The 60-scenario acceptance suite has not yet been executed end to end. No final release is authorized.

# CADWorkbench v1.70 — Consolidated Engineering RC5

**2026-11-06 | Development source candidate | Not a completed v1.70 release**

This consolidated release continues from RC4. It introduces two connected
engineering workflows and integrates them into save/reopen and the document
acceptance audit. The full project is packaged once, with no intermediate ZIPs.

## 1. Project-native, named, editable configurations (roadmap 12)

A supported parametric body may save up to **32** named design configurations
in its `.cadscene` file. Each definition stores **1–16** named dimension
expressions, evaluated using the existing unit-aware configuration engine.
The configuration editor can validate, edit, store, reapply, and optionally
import/export the older `.cadcfg` sidecar text. Applying a stored preset
commits its geometry and active preset label together or rejects the whole
operation. Manual named-parameter edits clear the active preset label but
retain the saved configuration definitions. Undo/redo and project save/reopen
preserve the catalog and label. An unknown variable, a linked/driven
parameter, expression cycle, unit mismatch, invalid dimension, duplicated
configuration name, malformed project data, or failed rebuild is rejected.

**Boundary:** Only the currently supported parametric body and its named
dimensions. The feature-specific formula vocabulary is not a general
spreadsheet/assembly configurator. There is no full cross-body variant
matrix or part-number/BOM approval workflow.

## 2. Stable-ID feature-linked engineering / inspection annotations (roadmap 21)

The selected solid's shape-feature tree or interleaved history exposes
**stable feature node IDs**. A body may retain up to **100** manually authored
notes, categorized `DESIGN`, `INSPECTION`, or `PROCESS`, with the exact
geometry revision that the engineer reviewed. Notes survive feature edits,
undo/redo and save/reopen. Deleting a referenced feature does **not** silently
reassign the note: it is retained as an orphan for disposition.

The editor provides explicit `Review selected for current geometry` after a
confirmation. The engineer, not the software, is responsible for checking
the modified feature before acknowledging a fresh review. The File menu
exports a revision-stamped CSV via `QSaveFile`; suspicious spreadsheet
formula prefixes are escaped. Each row is marked `CURRENT_REVISION`,
`REVIEW_REQUIRED`, or `MISSING_FEATURE`.

**Boundary:** A note is tied to a stable feature ID, not a universally
persistent face reference. This is a controlled engineering review
worksheet, **not certified dimensional inspection or sign-off**.

## 3. Cross-workflow document audit (roadmap 49)

The existing whole-project audit now emits blocking `Error` findings
`FEATURE_NOTE_REVIEW_REQUIRED` for old geometry revisions and
`FEATURE_NOTE_SOURCE_MISSING` for disappeared feature IDs. It cannot
silently treat a stale inspection as current just because model save or
drawing export succeeded.

## Important regression scenarios included in native source

- Store and apply a named configuration, edit the underlying dimension,
  undo, redo, close and reopen; reject duplicates, cycles and corrupted JSON
  while preserving the previously open model.
- Create/edit feature-linked notes, reject nonexistent features, persist
  after reopen, reject malformed data and preserve orphaned annotations.
- Modify geometry and confirm the previously reviewed note is flagged
  for review; explicit rereview clears the audit issue only after current
  geometry is acknowledged.
- Save inspection CSV atomically, verify output bytes and protect formula-like
  note content from becoming an executable spreadsheet formula.

All added native regression scenarios run within the existing registered
CTests; they are **written, not executed in this environment**.

## Verification evidence

| Verification | Result |
|---|---|
| Python source/preflight and independent OpenCascade geometry checks | **312 passed** |
| GCC portable C++ tests, ASan/UBSan | **1,100 passed** |
| Clang portable C++ tests, ASan/UBSan | **1,100 passed** |
| Native Windows CTests in the source CMake file | **130 registrations preserved** |
| Windows MSVC + Qt/OpenCascade compilation | **NOT PERFORMED** |
| Native CTest execution, GUI and persistence acceptance | **NOT PERFORMED** |

These counts cover different test categories; they **must not** be added to
claim native application verification. Linux CMake is intentionally guarded
as Windows-only. The full Qt/OpenCascade source changed in this increment
has not been compiled in the current environment.

Run the required release test on a Windows machine with supported SDKs:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Before promotion, exercise the configuration editor, notes editor, error
paths, document audit, export, save/reopen and undo/redo in the actual Qt UI.
Keep the last verified executable unchanged until these gates pass.

## Roadmap acceptance warning

The original 50-target implementation ledger does not support certifying
**85% completion**. RC5 meaningfully advances roadmap items 12, 21 and 49,
but these still remain **partial** versus their full acceptance criteria.
Universal topology naming, complete general multi-body regeneration, full
sketch constraint authoring, production sheet metal, fully compliant GD&T,
real executable CAM, and several assembly/UI requirements remain unfinished.
See `docs/V170_IMPLEMENTATION_STATUS.md`. Do not label this archive a final
v1.70 release or manufacturing-approved application.

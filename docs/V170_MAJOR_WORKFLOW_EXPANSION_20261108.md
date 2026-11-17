# CADWorkbench v1.70 — major three-workstream workflow expansion (PRELIMINARY)

**Development basis:** `CADWorkbench_v170_ThreeWorkstreams_AtomicParamMotionMfg_PRELIMINARY_20261108.zip`. This release is a new complete source snapshot, not a patch. Preserve the last Windows-built version separately. The changes in this document were not compiled using Windows MSVC/Qt/OpenCascade in the packaging environment.

## 1. Parametric design alternatives: complete geometry, not spreadsheet arithmetic

A new **Edit > Explore geometry-rebuilt parametric design alternatives...** command works on 1–8 selected parametric bodies. The user chooses a directly editable positive dimension on each selected body and a percentage; the application evaluates reduced/current/expanded candidates independently in detached full-scene documents. Each candidate invokes `SceneDocument::setNamedParameterBatch` with complete linked-body regeneration and measures actual OCCT solid volumes and available material mass. A failed candidate remains a rejected row while valid candidates can still be compared. The user may commit ONE valid candidate as one undoable scene edit; commit repeats the full validation and linked propagation.

The public API `evaluateParametricDesignAlternatives` accepts up to eight arbitrary multi-body candidate maps. It provides a per-candidate validity status, issue text, total solid volume, relative volume change, known-material mass, and rebuilt-body count. No failed candidate changes the scene, its revision, its history, or the caller's output buffer.

**Limit:** The comparison does not automate Pareto optimization, solve cost, or implement universal topological naming. Only existing supported named parameters are editable.

## 2. Assembly: sampled moving-body world-space envelopes

**Assembly > Edit articulated drives together** now has **Export sampled swept-space envelopes CSV...** on its review dialog, alongside existing strict fit/motion CSV. For every interpolated drive pose (up to 48 intervals / 49 poses), the new `buildLiveArticulatedEnvelopeCsv` independently rebuilds the full articulated mechanism and accumulates an axis-aligned OCCT bounding box for each moving occurrence (up to 20). The exported CSV records XYZ min/max bounds, box volume, sampled-pose count, and provenance note. This helps plan available space and actuator clearance without changing the model or joint coordinates.

Invalid drive targets, failed kinematic closure, and invalid body geometry leave an existing caller CSV untouched. The implementation uses the same joint-motion interpolation and `stageLiveArticulatedRegeneration` solve as the existing saved mechanism, not guessed joint-coordinate extrema.

**Limit:** Boxes enclose **sampled poses**, not true continuous swept volumes. They do not certify that space between samples is free of collision, especially for fast or complex movement. This is a workspace-planning aid, not a dynamic-load/forces solver.

## 3. Sheet metal: all-or-nothing multi-part manufacturing jobs

Select **2–24 associative flat blanks**, then use **File > Export selected associative flat blanks as ONE manufacturing job...**. The public API `exportLinkedFlatManufacturingBatch` preflights each source-linked SVG, DXF and bend CSV before creating any output. It builds an outer staging folder on the destination volume and invokes the existing complete per-part manufacturing package for every part. Each part retains its geometry source ID/token, bend rules and verified SHA-256 manifest. The outer job includes `batch-index.csv` and `batch-manifest.json` with part identities, folders, and output hashes. The directory is published only after all parts finish. Existing folders are never overwritten, and staging is removed on error.

**Limit:** This packages neutral-axis flat developments, not material-certified or springback-compensated cutting programs. Current flat geometry still uses the supported rectangular multi-flange/relief model.

## Native verification and deferred failures

- Last user-provided **prior Windows baseline**: full application compiled successfully, 128/130 CTests passed. Two failures remain deferred **without modification or suppression**:
  - `cadcore_v170_assembly_mate_document` — reported clearance shortfall assertion.
  - `cadcore_folded_sheet_metal` — segmentation fault.
- New native test cases were added inside existing CTest executables for independent design alternatives, multi-drive pose envelopes, multi-part manufacturing package publication, duplicate selection, rejection and rollback.
- `python -m unittest discover -s tests -p '*.py' -q`: **492/492 passed**.
- Portable GCC `scripts/verify_portable_consolidation.sh`: **16/16 targets passed** (unsanitized).
- Portable Clang same script: **16/16 targets passed** (ASan+UBSan).
- Native Windows build and new CTests: **not executed for this snapshot**. The previous 128/130 result does not transfer to newly changed source.
- Formal 60-scenario acceptance: **not scored**. Estimates do not satisfy release approval.

### Focused new-workflow run (from a clean Windows extraction)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_NEW_WORKFLOWS.ps1 -VcpkgRoot C:\vcpkg
```

This builds the current release and runs three focused native suites for parametric, articulated scene, and flat export. The two deferred failures remain registered in the full suite. For final approval, run `RUN_V170_RELEASE_GATE.ps1` and address them, all acceptance scenarios, and interactive smoke tests.

## Planning-only functional estimates

| Workstream | Prior | This source estimate | Final minimum |
|---|---:|---:|---:|
| Parametric modeling | ~78% | ~80% | 85% |
| Assembly engineering | ~84% | ~86% | 85% |
| Drawings and sheet metal | ~70% | ~73% | 65% |

**These are subjective source-functionality estimates, not verification results or scored acceptance percentages.** The next development push should extend parametric cross-feature topology identity, coupled mechanism motion solving, and manufacturing-accurate linked detail; none is certified complete by this source snapshot.

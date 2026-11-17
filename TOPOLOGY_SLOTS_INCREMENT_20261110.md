# CADWorkbench v1.70 — Witness-safe machining datum and obround slots

**Status: PRELIMINARY DEVELOPMENT SOURCE — not a final release.**

This snapshot extends the preceding **FaceFastener** source candidate. The latest independently Windows-verified baseline remains **Fix 2** (130/130 native CTests, 546 Python tests). The present code has **not** undergone a new Windows MSVC/Qt/OpenCascade build or GUI acceptance check.

## Features implemented

### Face slot through all

- A saved, editable shape-feature operation that removes an obround (capsule) slot from a **general solid body with a selected planar support face**.
- Position: local face offsets `u`, `v` in millimetres. `radius` is the end radius, `center_spacing` is the distance between circular end centers, and `rotation_degrees` sets in-plane orientation (−180° to +180°). The overall slot length is `center_spacing + 2 × radius`; width is `2 × radius`.
- The slot is assembled as two cylindrical caps and a connecting rectangular prism using OpenCascade Boolean kernels. Cutter extent is recomputed against the **current** B-rep bounds after upstream changes; fixed stock thickness is not stored in the operation.
- The cutter's final cut builder is retained for kernel history; input validation and transactional feature rebuild remain in force. No new native CTest registration was added.

### Face slot pocket

- Uses the same persisted face position/angle/radius/spacing, but has an editable positive blind `depth` in millimetres. The tool starts just outside the support face to avoid coincident Boolean faces.
- Both slot operations are available in the **General feature tree for any body → Add feature** searchable picker. Searches for `slot`, `obround`, `capsule`, `adjustment` or `mounting` find the operations.
- Select a planar face before adding, and preferably insert at the end of the current feature stack so its witness refers to the correct stage. Previous projects remain readable, but an index-only legacy face reference still has weaker lineage.

### Stable machining placement under a planar split or merge

- A face centroid can move after a coplanar Boolean fusion even when the original machined region is still present. The earlier implementation recomputed `center + (u,v)` from that changed centroid, silently moving a feature.
- New feature-tree machining uses the **saved original face datum** if the original interior witness remains on the same trimmed successor. When the witness shifts, the datum follows the body shift. A rigid tangential translation, where *both* raw and shifted witnesses remain on the face, is distinguished by preserved face area and coherent centroid displacement.
- When a witnessed machining center no longer lies inside the resolved **trimmed** support face, regeneration fails rather than moving the cut to another face region. Ambiguous or deleted witnesses still fail closed; the document and dependent bodies are not committed on failure.
- The original project-file schema is unchanged: no new persisted keys. Legacy index-only references keep their previous centroid placement contract.

## Changed files

- `src/core/FaceMachiningDatum.h`: deterministic, standard-library-only face-datum selection.
- `src/core/ShapeFeatureTree.cpp`: saved slot operations, validation, datum preservation and inward B-rep machining.
- `src/core/BooleanReferenceLineage.cpp`: identify the new face-slot operations as geometrically witnessed modifiers.
- `src/ui/MainWindow.cpp`: slot controls, defaults, trim-safe face capturing and workshop search aliases.
- `tests/associative_features_v159_tests.cpp`: new scenarios inside an **existing** registered native regression target: translation, slot dimensions, blind depth, persistence, invalid rollback, merged faces, and in-plane rigid translation.
- `tests/face_machining_datum_portable_tests.cpp`, `tools/run_portable_regressions.py`: same C++ datum helper tested without Qt/OpenCascade (15 executable suites total).
- `tests/v170_obround_slot_datum_reference.py`: independent OpenCascade geometry references for orientation, pocket/through material volume, circular blank and coplanar stock merging.

## Local verification

- **573/573** Python/source/independent reference tests passed on Linux.
- **15/15** portable C++ executable suites compiled and ran with `g++ -std=c++20`.
- **130** unique CTest registrations retained, none skipped or deleted.
- The independent OpenCascade reference confirms capsule Boolean geometry for 0°, 15°, 30°, 45°, 90° and −60°; changes in stock thickness; a round stock blank; and the original datum after merged planar-face simplification.
- **Not checked here:** this exact C++ code compiling with MSVC/Qt/OpenCascade on Windows, the complete 130 native Windows CTests, real GUI interaction, detailed design tolerances/fit, and the 60 acceptance scenarios. The independent OCP runs are not a replacement for native C++ testing.

## Windows verification

Extract the ZIP to a *new, empty directory*. Do not overwrite the last Windows-tested Fix 2 baseline. In PowerShell, from the extracted project root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

**Expected only if the new code passes Windows verification:** 130/130 native CTests and 573/573 Python/source tests, followed by exit code **3** if manual acceptance is still pending. A failed build or failed native test must be fixed without suppressing test registrations. Avoid incremental build caching by extracting to a new folder.

## Remaining engineering limitations and release gate

- General solid shapes are supported **via planar face entry**; slots on arbitrary curved support surfaces, unresolved face splits, arbitrary rigid rotations with no transported witness, and unambiguous topology mapping through all possible deformations are still open.
- The rigid-translation check uses stable face area and coherent centroid shift; it is intentionally conservative and not a complete geometric transformation-inference system.
- Slot geometry is an obround Boolean: not a tolerance-certified hardware slot and not a complete sheet-metal punch/die manufacturing workflow. No automatic milling cutter/toolpath or interference tolerance certification.
- The compact mixed-history editor does not yet have specialized slot numeric controls; the General feature tree editor does.
- The broader workstreams of general mate solving/motion, fully associative drawings/GD&T and validated multi-flange sheet metal remain incomplete.
- Historical **provisional implementation estimates**: parametric modeling ~85%, assembly engineering ~90%, drawings/sheet metal ~81%; **manual acceptance** P=0/20, A=0/20, D=0/20. Do not treat these estimates as verified completion scores.

**Final release still blocked:** 130/130 new native tests, all critical GUI acceptance scenarios passed with evidence, two workstreams at least 17/20 and the third at least 13/20 (with every critical case passed), plus the previously established machining and Windows-native verification requirements.

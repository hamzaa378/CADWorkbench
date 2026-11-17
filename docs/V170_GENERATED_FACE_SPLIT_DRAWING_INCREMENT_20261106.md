# CADWorkbench v1.70: Generated-Face References, Split Recovery, and Drawing Revision Safety

**Status: development source candidate, not a verified Windows application release.**

This increment continues directly from the supplied `CADWorkbench_v170_Witness_Safe_Machining_Atomic_Fanout_Source_20261106.zip`. The previous known-good Windows binary must be preserved.

## 1. Persistent identity for generated planar faces

`ShapeFeatureTree.cpp` now captures an additional nearby, trimmed-interior witness on supported planar faces. The existing primary witness remains the authority for design location. The new secondary witness is used **only to disambiguate multiple primary-witness candidates** after a split or merge, and never to move a design location to a different face. The new three scalar fields are optional; previously saved scenes remain readable. Validation rejects partial secondary witnesses and rigid transformations move both points.

This makes face references attached to generated loft or machining geometry less dependent on explorer face indices. It is not universal topological naming: reconstructed/overlapping faces and other ambiguous cases still need reselection.

## 2. Safe normal-only movement of a generated face

A downstream feature directly attached to its parent result can now follow a face that translates along its own normal, as when an additive loft's cap changes height. The old witness is projected by the verified plane movement, and the machining center is translated by the same displacement. This is supported only when all of the following hold:

- The parent stage is explicitly referenced and has been freshly evaluated in this rebuild.
- The new planar normal agrees to `dot >= 0.9999` and the area changes by at most approximately 2% in logarithmic ratio.
- The in-plane centroid moves no more than `max(0.0001 mm, 0.5% * sqrt(face area))`.
- The translated primary witness lies in the trimmed interior of exactly **one** candidate.
- The resulting machining center remains on and inside the selected face.

A sideways shift, profile redesign, deleted face region, or multiple candidate caps fails closed. The existing Boolean-provenance path is unchanged and never uses this geometric fallback across arbitrary Boolean steps.

## 3. Associative drawing regeneration and metadata revisions

`File > Regenerate saved drawing SVG...` reuses the saved sheet ID, source bodies, projection, paper size, auxiliary/section setup, views, notes, persistent dimensions, parts list, balloons, and other annotations. The old command only refreshed revision tokens; that could make a stale exported drawing appear current without issuing a new file. The new path **exports successfully via QSaveFile before saving updated source revision tokens**. An export failure or cancelled save leaves the previously saved drawing revision unchanged.

A shared `DrawingSourceFingerprint.h` now computes the exact source metadata fingerprint for the native saved sheet and the exported SVG. SVGs with complete source metadata carry `data-cadworkbench-source-data-token`. `checkDrawingSourceSnapshot` compares it to the current model and reports `DRAWING_SOURCE_METADATA_STALE` when part number, description, revision, procurement/supplier, material, density, or engineering material provenance changes. Older SVGs without this attribute remain readable but cannot detect metadata-only changes. These are **snapshot exports**, not automatic updates of arbitrary files on disk; users must explicitly regenerate a drawing after edits.

## Verification performed in the development environment

- Full Python unit / source-contract / independent OCCT reference suite: **259 passed**.
- GCC C++ with AddressSanitizer + UBSan: Boolean ancestry **37**, feature dependency **347**, linked-body graph **80**, total **464**.
- Clang C++ with AddressSanitizer + UBSan: **37 + 347 + 80 = 464 passed**.
- Independent OCCT reference fixtures include changing a box cap from z=10 to z=14; lateral shifts and profile changes are rejected.
- Added native C++ assertions in the existing face-lineage and CAD core test files: two-witness capture and split, cap travel with attached pocket, malformed references, and BOM-only drawing staleness. **Not executed** because this environment lacks native Windows Qt/OCCT/MSVC.
- Existing CMake registrations remain **130**; that is a registration count, not a test pass result.

## Required Windows native acceptance

From the extracted source project in PowerShell:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

After all tests pass, manually check: select generated loft top face -> make face-normal pocket -> edit loft height -> verify pocket follows; split a face near the secondary witness -> verify the primary reference stays attached; update part material/revision -> check old SVG reports metadata stale -> regenerate sheet -> check new SVG is current. Also test cancel and filesystem write failure: no source revision token should be acknowledged.

**Limitations:** These rules do not recover arbitrary B-spline reparameterizations, general generated fillet-face identities, large planar profile changes, arbitrary multi-branch face mergers, or live-updating files. Passing portable checks does not prove Qt/MSVC integration correctness.

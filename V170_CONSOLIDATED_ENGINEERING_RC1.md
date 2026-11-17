# CADWorkbench v1.70 — consolidated engineering RC1

**Status: development release candidate, NOT completed v1.70 or Windows-verified.**

This candidate consolidates the latest v1.70 source with additional correctness work across parametric model rebuilding and engineering production documentation. It is one source snapshot rather than a Windows installer. The 50-target ledger continues to apply; neither original priority is complete merely because this source can be packaged.

## Implemented in this consolidation

1. **Revision-aware feature reuse:** `cad::dependency::planReuseVersioned` now invalidates *every* cached descendant when its underlying base-revision token changes. Empty tokens are errors; malformed parameter order, duplicate parameter keys, and empty feature types are rejected. This is a safe portable-core API for future applications that change base B-reps. Existing legacy callers retain the explicit unchanged-base contract; current interactive sketch source edits still do full downstream rebuilding rather than relying on the new API.
2. **Audited assembly closure:** the rigid network solver reports physical **maximum point/axis gap in mm** and **maximum angular mismatch in degrees**, separately from its legacy 10-mm-lever equivalent residual. On rejection it includes the offending connection index and does not change the output. The articulated solver rejects NaN joint axes and reflection matrices, preventing nonrigid transforms from entering assembly layout.
3. **Witnessed hole-location dimensions:** drawing annotations can measure center `X`, `Y`, or `Z` from the model origin using a **full circular edge** recovered through its saved witness. Signed ordinates and user-specified asymmetric tolerances persist through save/reopen, and drawing export recalculates from current geometry. A view cannot annotate its hidden axis: TOP allows X/Y, FRONT X/Z, RIGHT Y/Z. This is a model-coordinate ordinate and **not a standardized datum or certified GD&T**.

## Compatibility and limitation details

- Saved drawing dimensions are encoded as new `CENTER_X`, `CENTER_Y`, `CENTER_Z` enum values in existing optional v24 drawing metadata. Back up projects before opening them with older CADWorkbench binaries: an older binary may reject these newer dimension kinds. Legacy dimension records remain readable in this candidate.
- The project is intentionally Windows/MSVC with Qt6 and OpenCascade 8.0.1; the portable checks never compile `SceneDocument.cpp`, `DrawingSheet.cpp`, or `MainWindow.cpp` with their real dependencies.
- The new drawing controls require a witnessed **complete circular edge**. Model origin must be the engineer's intended measurement reference. No hole-table grouping, datum feature simulator, position tolerance zone, or standards certification is implied.
- Rigid-assembly report fields use physical units independently; the legacy residual remains a mixed mm-equivalent screen based on a 10 mm reference lever.

## Still blocking original Priority 1

- Universal persistent face/edge naming across unrestricted Boolean, arbitrary spline/loft/fillet reconstruction and inherently ambiguous split/merge histories.
- Fully generalized cross-body parametric feature graph with selective persistent caching across all feature kinds and complete sketch-constraint DOF/conflict editor.
- Associative sketch-to-path lofts/sweeps/revolves and arbitrary-plane sketch references across every feature.
- Native end-to-end verification of atomic fan-out/fan-in rebuilds, save/reopen, undo/redo and GUI authoring.

## Still blocking original Priority 2

- Persisted **live** articulated/rigid mate definitions and broad face-driven mate authoring, beyond current bounded solver and solved placements.
- Fully associative hole tables and GD&T datums/feature-control frames, standardized manufacturing drawing approvals, and automatic on-disk drawing revision workflows.
- Complete sheet-metal/flat-pattern production and verified manufacturing CAM toolpaths/post-processing.
- Real-application Windows compiler, CTest, GUI, persistence and manufacturing acceptance runs.

## Portable verification

```bash
./scripts/verify_portable_consolidation.sh g++
./scripts/verify_portable_consolidation.sh clang++
```

On Windows, from a freshly extracted source tree, run:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

**Release promotion rule:** no claim of v1.70 completion until the remaining acceptance items above are implemented and the entire native application builds, all registered native CTests pass, saved project migration passes, and live Qt workflows are manually verified. Preserve the last Windows-verified executable.

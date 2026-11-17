# CADWorkbench v1.70 — Three-workflow development preview (2026-11-08)

**PRELIMINARY SOURCE ONLY. Not a build-verified Windows release.** This archive is based on the previously saved `CADWorkbench_v170_Larger_Functional_Update_PRELIMINARY_Source_20261107.zip`, retaining the previous face-machining fixture correction, assembly changes and drawing changes.

## New end-to-end source changes

1. **Articulated mechanism pose preview:** `SceneDocument::previewLiveArticulatedJointCoordinate` uses the existing staged live-articulated solver and geometry transformation before a driven revolute or prismatic joint edit. It checks OCCT B-rep validity and, for up to 12 bodies, performs exact *static-pose nominal* pairwise fit testing including user-entered minimum clearance. The GUI opens a review dialog before changing the saved coordinate; no-op previews create no revision. On rejection, model, joint definitions and caller report remain unchanged. **Not swept clearance certification.** 13–24-body networks retain pose/solid validation but pairwise fit is explicitly skipped.
2. **Folded-sheet blank review before DXF:** `buildFoldedSheetFlatPatternSvg` validates the **exact same K factors, end relief, bend stations and six-decimal export constraints** via the existing DXF generator before creating a preview. GUI shows the blank, bend stations and relief notches before opening the export-file dialog. On cancel no DXF is written. Invalid requests do not overwrite previous SVG and computed flat pattern. This is a geometry preview, not tooling/springback approval.
3. **Parametric formula what-if solid regeneration:** `SceneDocument::previewNamedParameterFormulas` stages the same formula, parametric B-rep and linked-body regeneration as the production setter in a detached document and reports rebuilt body counts and solid volumes. The named-parameter dialog's Validate button invokes this check; failed previews leave committed geometry, revision history and caller output untouched. A Save still performs the production transaction independently.

## Regression additions

- Existing `articulated_assembly_scene_v170_tests.cpp`: revolute and prismatic previews, no mutation on preview or failed inputs, fit report versus committed pose.
- Existing `cadcore_tests.cpp`: folded-sheet SVG vs DXF stations and reliefs, invalid K factors and relief rollback, valid and cyclic formula preflight without mutation.
- New `tests/v170_articulated_flat_review_contract.py`: source-level guards for all three workflows and their native integration tests.
- Registered Windows-native CTests remain **130**; the added native cases live in existing test binaries. They **have not been executed on Windows** for this snapshot.

## Verification in this environment

- `python -m unittest discover -s tests -p '*.py' -q`: **407/407 passed**.
- `CAD_SANITIZE=0 CAD_SKIP_PYTHON=1 bash scripts/verify_portable_consolidation.sh g++`: **14/14 portable targets passed**.
- `CAD_SANITIZE=0 CAD_SKIP_PYTHON=1 bash scripts/verify_portable_consolidation.sh clang++`: **14/14 portable targets passed**.
- Qt, OpenCascade development headers and Windows MSVC toolchain are not available for native compilation here. These results do NOT establish native binary correctness.
- Full archive files are listed and SHA-256 protected by `SHA256SUMS.txt`, also copied to two named source manifests.

## Provisional coverage estimates, not formal release acceptance

| Workstream | Previous | After this pass (estimated) | Release target |
| --- | ---: | ---: | ---: |
| Parametric modeling | 65% | **67% (+2 points)** | 85% |
| Assembly engineering | 68% | **71% (+3 points)** | 85% |
| Drawings and sheet metal | 50% | **52% (+2 points)** | 65% |

These modest increases reflect integration of complete pre-commit workflows. They do not establish arbitrary persistent topological naming, validated motion over time, or manufacturing-grade flat patterns. The formal 60-scenario acceptance inventory remains unscored. Do not promote to final v1.70 until two workstreams meet 85% and the third 65%, all required critical scenarios pass, and Windows Release native tests pass 130/130.

## Windows acceptance instructions

1. Extract this ZIP in a **new directory**. Preserve the previous known-passing Windows baseline.
2. Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg`.
3. Inspect `cadcore_v170_interleaved_history` first if the historical face-machining failure recurs. Then validate `cadcore_v170_articulated_scene_transaction` and `cadcore_geometry_and_persistence` including the new tests.
4. Manually preview and cancel a driven prismatic/revolute edit; preview a relieved 2-bend flat blank and cancel export; validate an invalid named-parameter formula and confirm no model change, then validate and commit a valid one.
5. Investigate any failure before replacing the last passing Windows Release binary. No claim of final-release quality is made in this snapshot.

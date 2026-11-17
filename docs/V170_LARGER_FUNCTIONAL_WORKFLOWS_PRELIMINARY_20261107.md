# CADWorkbench v1.70: Larger functional workflow increment (2026-11-07)

**Artifact status: PRELIMINARY SOURCE SNAPSHOT.** NOT a build-verified Windows release and NOT evidence that the 85/85/65 release threshold has been reached.

This pass starts from `CADWorkbench_v170_Major_Three_Workstreams_Preliminary_Source_20261107.zip`, preserves the previously saved implementation and the face-machining test-fixture fix, and adds four connected engineering workflows instead of report-only features.

## A. Parametric modeling: advanced history parameter editing

In **Edit -> Mixed associative feature history**, select an existing step whose operator is not exposed by the compact editor. **Edit step** now opens a guarded numeric-parameter table. It retains the operation type, step ID, graph inputs, and captured topology witness values. It validates finite, bounded numeric input and runs the existing read-only interleaved-history kernel rebuild on all staged steps. An invalid or ambiguous operation is rejected without changing staged steps or the saved body. If successful, the edited stage remains staged until **Save** commits the entire history in a transaction.

Limits: Only numeric parameters already in the persisted feature definition are editable; it does not add new parameter keys, directly edit topology witness data, alter feature type or graph inputs, or guarantee persistent identity after arbitrary B-rep split/merge. The GUI still needs manual Windows testing and a native regression for unsupported advanced operator editing.

## B. Assembly engineering: geometry-aware rigid mate edit preview

The saved rigid-mate editor now stages the proposed solved poses in a detached document, transforms all participating solids with OpenCascade, and runs the same B-rep validity checks used by committed moves. For 2 to 12 bodies it also calls the existing exact nominal pairwise fit inspection, showing overlap pairs, contact pairs, the closest separated gap, and any pairs below a user-selected required clearance in mm. For 13 to 32 bodies all moved solids are checked but the quadratic pairwise fit audit is explicitly **skipped and disclosed**. Failed geometry/fit calculations leave report and source model unchanged.

The clearance threshold is optional. An unchanged mate can be inspected without producing any model revision. Overlaps are warnings, NOT automatic vetoes: intentional interferences are common in mechanical assemblies. This is a snapshot of the predicted pose, not swept collision certification, a material or GD&T tolerance guarantee, or proof of a closed-loop motion study.

Native regression cases in `tests/assembly_mate_document_v170_tests.cpp` cover no-op reporting, 3-pair clearance, positive overlap and a nonnegative threshold guard. Still pending Windows execution.

## C. Drawings: reviewed source regeneration before issue

**File -> Regenerate saved drawing SVG** now renders the complete proposed SVG with existing view identities, linked geometry dimensions, annotations and BOM into a read-only review dialog. Cancel exits without writing a file or acknowledging geometry/data revisions. Approval continues to the existing atomic SVG exporter, and only a successful export can save the new source revision. The preview uses the existing QtSvg module; no new vcpkg dependency is required.

Limits: This is an issuance preview, not always-on background drawing regeneration, not independent GD&T conformance, and not an independent check of the output file. Source annotations that cannot resolve still fail using the existing kernel/renderer rules.

## D. Sheet metal: actual-solid edit preflight

Editing an existing current folded body now calls `SceneDocument::previewFoldedSheetMetalEdit` first. This builds a detached candidate using the same OCCT solid generator as the committed edit, checks B-rep validity and measured positive volume, and calculates an indicative neutral flat length using K=0.33. The user sees these values and must confirm before the existing undoable edit runs. Invalid self-intersecting contours, stale definitions or invalid numeric inputs are rejected without modifying the original model or overwriting the preview outputs.

The numeric development is a planning aid; actual K factors must be confirmed for each bend with material/tooling data. The edit retains the existing ability to save/reopen, undo/redo, and mark old drawing-source tokens as stale.

Native regression cases in `tests/cadcore_tests.cpp` cover successful volume previews, invalid overlapping bend rejection, stale-definition rejection, unchanged tokens and unchanged output values on failure. Windows execution pending.

## Verification and limits

- Python and source contracts: **399/399 pass** (source contracts, portable independent OpenCascade reference tests, and preflight checks; not Qt GUI execution).
- GCC portable C++ targets: **14/14 pass without sanitizers**. GCC sanitizer pass: first **13/14 targets passed**; the last compilation exceeded the available invocation limit, so this is **not a full sanitized suite pass**.
- Clang portable C++ targets: **14/14 pass without sanitizers**.
- Native CTest inventory unchanged at **130**, including `cadcore_v170_interleaved_history`. No Windows/MSVC build or complete native CTest execution was performed for this ZIP.
- Native tests were added to existing suites, but were **not executed** in this environment.
- The previous fully passing Windows version remains the last accepted release; do not overwrite it with this preliminary source.

## Estimated functional progress (not acceptance scores)

These are rough coverage estimates reflecting added GUI workflows. They are NOT earned acceptance points from the 60-scenario gate and should not be treated as Windows-verified percentages.

| Area | Previous estimate | New provisional source estimate | Final gate |
| --- | ---: | ---: | ---: |
| Parametric modeling | 62% | 65% (+3 points) | 85% |
| Assembly engineering | 63% | 68% (+5 points) | 85% |
| Drawings + sheet metal | 45% | 50% (+5 points) | 65% |

**Formal acceptance: unscored.** A release requires 17/20 accepted scenarios in two workstreams and 13/20 in the third, critical face-machining regression passing, and 130/130 native Windows CTests. Do not count these provisional percentages toward release approval.

## Windows testing

1. Extract into a new directory, preserving the last passing version.
2. Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg`.
3. Check the face-machining regression, rigid mate document and folded sheet native CTests explicitly.
4. Manually test: change an unsupported advanced-history numeric parameter, review the shape, cancel and commit; edit a mate with 15 mm minimum clearance and inspect predicted overlap before commit; edit a folded sheet and cancel at the geometry review; regenerate a saved SVG and cancel at preview, then export and verify the source token moves only after a successful write.

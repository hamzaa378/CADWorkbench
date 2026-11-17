# CADWorkbench v1.70 — Backend Sprint 8: Boolean Ancestry and Drawing Revision Recovery

**Date:** 2026-11-16. **Status: PRELIMINARY SOURCE, NOT A WINDOWS-VERIFIED APP OR PRODUCTION RELEASE.**

This is one combined follow-on checkpoint based on the full **Sprint 7 ReferenceIntegrity** ZIP, with no baseline source files deleted. User requested expansion of persistent face/edge ancestry through Boolean splits and merges, plus revision-sensitive drawing recovery through undo, redo, save and reopen. Existing user constraint: **do not perform the Windows/MSVC application build yet**. Frontend remains frozen. The last independently verified Windows executable remains a separate rollback baseline.

## Concrete defects and corrections

### A. A witnessed edge could silently jump between identical solids

**File:** `src/core/ShapeFeatureTree.cpp`, `resolveStableEdgeAnchor`.

A saved v2/v3 witnessed edge permits two placement interpretations: physical world coordinates and body-relative coordinates transported by the changed body's reference centre. The old resolver returned as soon as one trial produced a unique match. If a Boolean merge or compound contained multiple equal occurrences, trial 1 could refer to one edge and trial 2 to **another**; the old resolver would silently accept trial 1. This is dangerous for downstream fillets, chamfers, face/edge-linked sketches, and drawings.

**Correction:** consider all permissible placement trials; require every successful placement to resolve the same OCCT edge index. Multiple matches inside a trial or disagreement between trials now fail with an explicit re-selection diagnostic. On failure the destination edge index is unchanged. Truly unique placements still pass.

A second correction bounds the physical witness tolerance to **[0.00001, 0.001] mm**. The former proportional `max(1e-5, oldLength*1e-6)` would permit 1 mm of witness deviation for a 1 km rail and worse for longer supported edges. A witness for topology identity must not become a length-scaled nearest-edge search.

**Native fixture:** `tests/face_lineage_v170_tests.cpp` now uses an actual OCCT compound of three evenly spaced identical boxes to test conflicting absolute/body-relative edge witnesses, actionable diagnostics and output preservation. The existing splitter/native tests continue to cover split-boundary rejection, surviving unique descendants, missing and ambiguous face ancestry. These new native cases are **NOT YET EXECUTED**, pending Windows/Qt/OCCT compilation.

### B. Multi-branch Boolean consensus did not test all pairs

**File:** `src/core/ReferenceRouteConsensus.h` (used by `SceneDocument.cpp`'s current interleaved Boolean rebuild).

A first-route-only tolerance comparison is not transitive. If route A is at 0, B is at +0.00009 mm, and C is at -0.00009 mm, each of B and C differs from A by less than the 0.0001 mm tolerance, but B and C differ by 0.00018 mm. They cannot all be confidently identified as one physical design witness.

**Correction:** retain the original schema and finite-value checks, then require **every pair** among the maximum 32 ancestry routes to agree on physical design witness, descriptor field names and continuous/discrete feature identity. Pairwise inconsistencies reject the whole feature rebuild before any geometry commit. `tests/reference_route_consensus_portable_tests.cpp` adds explicit three-route adversarial cases; the full suite passed **180,021 assertions** with each of GCC and Clang. This remains a conservative, non-certifying geometry correspondence policy. It deliberately rejects ambiguous joins rather than inventing a persistent name.

### C. Drawing revision review lacked a dedicated geometry-checked transaction

**Files:** `src/core/SceneDocument.h/.cpp`, `src/core/DrawingSheet.h` (included by the core implementation, not modified).

Introduced `SceneDocument::reviewDrawingEdgeDimension(sheetOwnerId,annotationId,error)`:

* Looks up the saved annotation and its actual current source body.
* If an old revision was recorded, **requires a witnessed edge** (an old index-only reference must be manually reselected).
* Resolves a uniquely identified current OCCT edge using the saved physical witness. Full-circle diameter/centre callouts may use the existing drawing-only circle-size recovery policy.
* Invokes the existing **real SVG drawing measurement preflight** before acknowledging the new source revision, ensuring dependent centre-distance circles and view constraints are also checked.
* Stages the revised annotation in a detached part vector, creates one undo checkpoint **only after** all checks pass, and publishes one complete state transition. A failed review preserves the former drawing and undo state.
* A same-token explicit review still validates the geometry; a valid no-op does not create an undo entry.

Strengthened `prepareDrawingSheetSettings`: changing the revision on an **existing witnessed annotation** now requires the saved anchor to resolve uniquely against the current body; it is not enough to rewrite the token in a generic editor. For backwards compatibility, legacy index-only references remain editable through their existing workflow, but this does **not** constitute proof of topological identity: the dedicated review API insists on a witnessed reference.

**Native regression:** `tests/document_audit_tests.cpp`, existing `cadcore_document_audit` CTest. A real OCCT box with a physical-edge dimension and a manual GD&T frame now tests:

1. Both references initially current; modifying geometry makes both stale.
2. Undo restores the previous source token and current state; redo re-stales both.
3. Refreshing only the aggregate sheet must **not** approve either annotation.
4. Save and reopen of a stale scene retain both stale IDs, stable sheet ID, B-rep source identity and the saved edge witness. A reopened process has no inherited undo stack.
5. A fabricated displaced edge witness with a new source token is rejected without changing the live sheet.
6. Explicit geometry-checked edge review clears only that edge; the GD&T frame remains stale until separately authored/reviewed.
7. Undo/redo of GD&T review preserves the correct stale/current state; saving and reopening the reviewed scene preserves both references.
8. Re-reviewing a current valid edge is an idempotent no-op with no additional undo record.

All the above native cases are **registered in the historical CTest target but not yet compiled or executed**. No test names removed and no new CTest registration created; **all 130 historic registrations remain**.

## Local verification actually performed

* Python/source suite: **887/887 PASS** (12 new source-level integration tests).
* Standard-library portable C++ suites: **35/35 GCC PASS** and **35/35 Clang PASS**; pairwise route-consensus suite executed 180,021 deterministic assertions independently under each compiler.
* Full-tree source audit: PASS; 398 relevant code/build/test files scanned, 84,302 lines, no reported missing CMake references or duplicate historic native CTest names. This audit does **not** imply MSVC native code compiled.
* Windows-native MSVC + Qt 6 + OCCT 8 application build: **NOT RUN**.
* All 130 Windows native CTests: **NOT RUN** on this candidate.
* 60 manual engineering acceptance scenarios: **NOT RUN**.
* Historical CTest `cadcore_universal_feature_tree` grouped unequal-size edge blend: still needs native Windows confirmation.

The eventual build remains the authority on C++ integration and kernel behavior. Do not treat these tests as topology naming guarantees, approved GD&T, standards compliance or verified native application behavior.

## Files intentionally changed

* `src/core/ShapeFeatureTree.cpp` — stricter and physically bounded v2/v3 edge witness resolution.
* `src/core/ReferenceRouteConsensus.h` — pairwise Boolean branch descriptor/witness agreement.
* `src/core/SceneDocument.h`, `src/core/SceneDocument.cpp` — dedicated geometry-checked drawing edge review API and generic token-rewrite guard.
* `tests/face_lineage_v170_tests.cpp` — actual OCCT compound ambiguity test.
* `tests/document_audit_tests.cpp` — drawing history/save/reopen native regression scenario.
* `tests/reference_route_consensus_portable_tests.cpp` — adversarial three-branch divergence cases.
* `tests/test_sprint8_topology_drawing_contract.py` — 12 source-level safety/integration contracts (new).
* `RUN_SPRINT8_WINDOWS_STAGED_BUILD.ps1` — preflight, full Python suite, backend-first native compilation and focused native CTests; full build only when requested (new).
* `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1` — raise minimum complete Python evidence to 887.
* This handover and `SPRINT8_QA_EVIDENCE` with exact local test logs.

## Future Windows verification (from a **fresh** PowerShell session)

The source ZIP contains **all** code and scripts. Put the combined ZIP in Downloads, then run:

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$zip = Join-Path $downloads 'CADWorkbench_v170_BackendSprint8_AncestryDrawingRecovery_COMBINED_PRELIMINARY_20261116.zip'
$dest = Join-Path $downloads 'CADWorkbench_Sprint8_FreshVerification'
if (!(Test-Path -LiteralPath $zip -PathType Leaf)) { throw "ZIP not found: $zip" }
if (Test-Path -LiteralPath $dest) { throw "Destination already exists: $dest" }
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$root = Join-Path $dest 'CADWorkbench_v170_BackendSprint8_AncestryDrawingRecovery_PRELIMINARY_20261116'
Set-Location -LiteralPath $root
py -3 .\tools\preflight_major_build.py --root .
if ($LASTEXITCODE -ne 0) { throw 'Manifest/build preflight failed' }
# Optional EARLY native test stage WITHOUT compiling the giant GUI:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT8_WINDOWS_STAGED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4 -TriageOnly
# Exit 4 is the documented triage-only success, NOT a full native release pass.
Write-Host "Staged build exit: $LASTEXITCODE"
# When explicitly ready for the complete build and 130 CTests, rerun WITHOUT -TriageOnly:
# powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT8_WINDOWS_STAGED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4
```

If the staged native build fails, collect the first MSVC/OCCT compiler error, not only the last status lines. Never claim success from synthetic test log fixtures. Keep the previous working executable intact until actual Windows and manual acceptance gates are green.

## Next engineering priorities

* Native OCCT verification of branched split/merge cases involving curved, periodic and coincident surfaces, and performance measurement on high-face-count models.
* Atomic recover-or-reselect workflow for annotations whose original edge has genuinely disappeared, with no guessed nearest edge.
* Geometry-aware review of legacy index-only dimensions through explicit UI re-selection (not silent index preservation).
* Native regression of failed-load scenes, drawing source deletion and cross-part multi-sheet drawing restore.
* Only after staged native compilation succeeds: cautiously reduce the size of `SceneDocument.cpp` and the giant UI translation unit, making each extraction separately testable.

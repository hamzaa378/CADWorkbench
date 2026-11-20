# Sprint 16 — Unified Engineering Preflight and Motion-Rate Integration

**PRELIMINARY SOURCE CANDIDATE.** Built independently from the complete Sprint 15 GitHub-ready ZIP while the user's Sprint 15 Windows build is underway. **DO NOT** replace the existing verified Sprint 11 Windows baseline or the user's active Sprint 15 build directory. All 130 original native CTest registrations are preserved, but the new native OCCT/Qt additions have not yet run through MSVC.

## Engineering development intent

Continue the established ~70% integration / 20% reliability-and-performance / 10% new-capabilities allocation, keeping GUI proliferation low and relying on existing native geometry logic. Sprint 16 is a concentrated incremental integration release, **not** a claim that any of the remaining major CAD workflows is universally complete.

## Implemented changes

1. **Unified read-only engineering readiness review:** `SceneDocument::buildIntegratedEngineeringReadinessCsv` and `exportIntegratedEngineeringReadinessCsv` combine the *actual existing* OpenCascade `validShape()` check, live strict SVG regeneration of saved drawing sheets, sheet-revision state, independent folded-solid OCCT refold checks, and persisted revolute/prismatic joint-motion tracks into a bounded 2 MiB CSV. The existing mature document audit is also reused to capture stale parametric trees, linked sketches, drawing annotations, feature inspection notes, invalid mate parents and other project-state findings. A single failed domain becomes an explicit `BLOCK` finding rather than hiding the rest of the report. Other non-issued checks are marked `REVIEW` or `CHECKED` with strict qualification. A final status line always says `engineering_approval=NOT_ISSUED`. No scene geometry, revision token, drawing approval or undo state is mutated by this review.

2. **Exact piecewise-linear joint-coordinate-rate inspection:** `src/core/JointKeyframeRateAudit.h` validates 2–200 real saved keyframes, joint limits, monotonically increasing time, finite numeric arithmetic and bounded slopes. It reports maximum joint coordinate rate (mm/s for prismatic, degrees/s for revolute), direction reversals, discontinuities in velocity and exact local time-window rate bounds. Results are staged atomically. The solver uses existing saved keyframe interpolation. **NOT** a bound on whole-solid 3D motion: rigid mate chains, moving axes and constraints can amplify speeds and the ideal interpolator has discontinuous velocity at rate changes. The existing sample-based collision scan is unchanged; arbitrary rotating-solid continuous collision proof remains unimplemented.

3. **Atomic multi-joint motion-track editing and synchronized timing:** New native APIs `previewJointMotionKeyframesBatch` and `setJointMotionKeyframesBatch` validate 1-32 independent saved tracks and all physical joint bounds before changing anything. The full proposed set receives one undo checkpoint; preview is strictly read-only. The existing Assembly menu now includes **Retime all saved joint motion tracks (one undo)** to uniformly rescale their saved times, preview per-joint rate changes, and then commit all tracks together. This enables coordinated playback editing without intermediate inconsistent schedules. No physical collision clearance is implied; the existing B-rep scan is a separate required check. Native regressions cover invalid second-track rollback, preview immutability, two-track undo/redo, save/reopen and persisted 5 mm/s slider-rate review.

4. **Existing Qt integration:** *Inspect → Integrated project readiness: geometry, drawings, refold and joint rates* opens a human-reviewable preview and offers an atomic CSV save. The output is labeled **UNISSUED** and intentionally does not auto-approve GD&T, sheet-metal fabrication, motion or manufacturing release.

5. **Regression protection:** new portable randomized stress test `tests/joint_keyframe_rate_audit_portable_tests.cpp` verifies 18,015 assertions under GCC and Clang. Existing native `document_audit_tests.cpp` includes integration tests for initial project/drawing/refold review, output immutability on invalid invocation, atomic export, and a changed-source stale drawing blocker. Existing `cadcore_tests.cpp` includes persisted prismatic keyframe rate inspection (0,10,0 mm over 4 seconds => 5 mm/s). New source/runtime test contracts `tests/test_sprint16_integrated_readiness.py`. No native CTest names added or removed.

## Files changed or added

- NEW `src/core/JointKeyframeRateAudit.h`
- NEW `tests/joint_keyframe_rate_audit_portable_tests.cpp`
- NEW `tests/test_sprint16_integrated_readiness.py`
- NEW `RUN_SPRINT16_WINDOWS_TARGETED_BUILD.ps1`
- NEW `SPRINT16_CONTINUATION_20261117.md`
- UPDATED `src/core/SceneDocument.h`, `src/core/SceneDocument.cpp` (includes atomic batch motion authoring)
- UPDATED `src/ui/MainWindow.cpp` (includes synchronized motion retiming command)
- UPDATED `tests/document_audit_tests.cpp`, `tests/cadcore_tests.cpp` (includes native two-track rollback/undo/redo/save-reopen acceptance)
- UPDATED `tools/run_portable_regressions.py`, `scripts/verify_portable_consolidation.sh`
- UPDATED `README.md`, `CONTINUE_HERE_LATEST_PRELIMINARY.md`, `SHA256SUMS.txt` and `SOURCE_MANIFEST_SHA256.txt`

## Verification performed here

- Python/source suite: **1,044/1,044 passed**, including independent CadQuery/OCP fixtures. One expected negative-test diagnostic prints `FAIL: Measurement status...`; the suite's terminal result is `OK`.
- GCC portable regression runner: **44/44 executables PASS**.
- Clang portable regression runner: **44/44 executables PASS**.
- Portable keyframe rate audit: **18,015 assertions PASS** independently with both compilers.
- Source file manifest/preflight/static audit, ZIP integrity, fresh extraction and Git trackability: to be confirmed at final packaging.
- New Windows GUI / OCCT native code: **NOT compiled here**. Historical native CTest registration count remains 130. **Do not report a 130/130 Sprint 16 pass** until the user's Windows run confirms it.
- Manual mechanical engineering acceptance: **0/60 formally recorded**. All 18 critical scenarios remain unaccepted.

## Windows verification from a fresh PowerShell session

```powershell
Set-Location "$env:USERPROFILE\Downloads"
$zip = 'CADWorkbench_v170_Sprint16_IntegratedReadiness_GitHub_Source_PRELIMINARY_20261117.zip'
if (!(Test-Path -LiteralPath $zip)) { throw 'Sprint 16 ZIP missing' }
$dest = "CADWorkbench_Sprint16_Test_$(Get-Date -Format yyyyMMdd_HHmmss)"
Expand-Archive -LiteralPath $zip -DestinationPath $dest
Set-Location (Join-Path $dest 'CADWorkbench')
$log = Join-Path $env:USERPROFILE 'Downloads\CADWorkbench_Sprint16_Targeted.log'
Start-Transcript -Path $log -Force
try {
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT16_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4
  $status = $LASTEXITCODE
} finally { Stop-Transcript }
Write-Host "TARGETED EXIT: $status" "LOG: $log"
```

Expected targeted-only success code: **4** (full build/test gate remains pending). After targeted success, rerun the same `.ps1` from the same tree with `-FullAudit` for full MSVC regression. The comprehensive audit may exit **3** if automation passes but 60 manual acceptance scenarios are pending; inspect its diagnostic files. Never skip individual native tests or mark them as completed based on the previous Sprint 11 results.

## Current gaps and next priorities

- The integrated report is **read-only reporting**, not the full remediation UI for each failing condition. Engineering reviewers still need to repair sources and explicitly sign off measurements, drawing references and manufactured parts.
- General **continuous rotating-solid collision proof** is not completed; keyframe-rate bounds are joint-coordinate bounds, not validated spatial Lipschitz bounds for arbitrary multi-link mechanisms.
- Arbitrary **multi-plane sheet-metal corner relief and 3D refold geometry** are not implemented. Supported constant-width folded shapes receive a strict independent OCCT volume conservation audit.
- Generalized **persistent topology tracking through arbitrary splits/merges** and fully hands-free approved associative drawing issues remain larger gaps.
- Do not merge Sprint 16 into the default branch or delete Sprint 15 build trees until the new targeted and full Windows gates pass. Preserve the latest personally verified Windows rollback source (Sprint 11) and its 130/130 evidence.

## Sprint 15 MSVC build failure repaired in combined Sprint 16 (2026-11-18)

The user's two Sprint 15 logs confirmed that `SceneDocument.cpp` was missing its direct `FoldedContourTopology.h` and `RefoldGeometryAudit.h` includes, causing C2065/C2653 and cascaded compiler diagnostics. Both includes are now present in the Sprint 16 source. `tools/preflight_major_build.py` now rejects this missing-header pattern before native compilation, and regression tests cover both successful header compilation and intentional include removal. Only Sprint 16 is delivered and tested; no Sprint 15 patch is included. Full analysis and verification limits: `SPRINT16_MSVC_REFOLD_REPAIR_20261118.md`. Previous Sprint 16 features are retained unchanged.


## 2026-11-18 Windows preflight follow-up

See `SPRINT16_WINDOWS_PREFLIGHT_REPAIR_20261118.md` for a Sprint 16-only compatibility fix. The user's Python 3.14 cp1252 locale and an obsolete PATH g++ blocked the earlier preflight **before** MSVC. Explicit UTF-8 reads, UTF-8-enabled Windows launchers, portable C++20 compiler probing and `.exe` naming address those test-harness errors. The diagnostic expected Python inventory is updated to 1,049 tests; Windows 130 native CTests and manual engineering acceptance are still unverified for this corrected candidate.

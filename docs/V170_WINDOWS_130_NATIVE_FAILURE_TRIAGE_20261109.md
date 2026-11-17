# CADWorkbench v1.70: Windows 130-case regression triage (9 November 2026)

This source checkpoint is a testing/hardening revision of the frozen `LastDevelopment_BeforeWindowsTests` release candidate. It **does not add features** or authorize the final release.

## Evidence supplied by the Windows Release run

- Windows Release compilation succeeded. `cadcore_v170_interleaved_history` (historical face-machining regression) passed.
- 126 of 130 registered CTests passed. Four cases failed:
  - `cadcore_v170_articulated_scene_transaction`: sampled workspace CSV contract
  - `cadcore_v170_assembly_mate_document`: nominal gap/clearance preview assertion
  - `cadcore_parametric_foundation`: combined dimension and suppressed-hole edit failed to reload
  - `cadcore_folded_sheet_metal`: Windows access violation / segfault, precise stage unknown

## Corrections made (must still be verified with MSVC)

1. **Parametric persistence:** the saved definition retains inactive hole parameters so users can restore features later. On reload, containment is checked only for an *active* hole, and distant repeated hole centers are checked only for an *active* pattern. Active invalid holes, parameter-domain errors, stored-solid checks, and geometry-token verification remain enforced. The native regression now also verifies that reactivating an out-of-bounds feature is rejected atomically.
2. **Articulated workspace:** the CSV `coverage_note` explicitly identifies `sampled pose AABB` and retains independently solved, sampled OCCT B-rep bounds. The numerical sampling algorithm is unchanged.
3. **Mate clearance:** the preview prints per-pair physical gap/status measurements. The native fixture uses an 18 mm threshold, which produces two unequivocally below-target nominal separations for its three-box geometry, rather than a 15 mm threshold exactly equal to one pair's gap. The underlying fit policy is unchanged.
4. **Folded-sheet crash:** no speculative OCCT fix has been applied. Eight `stderr` phase markers are added around the long regression, allowing the *next Windows run* to identify the last successful stage before the access violation. The case remains registered, enabled, and a release blocker.

## Retest

From a clean extracted source directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_FOCUSED_NATIVE_TRIAGE.ps1 -VcpkgRoot C:\vcpkg
```

This builds Windows Release and runs exactly the four failures in verbose, sequential CTest. It writes `build/v170_four_failure_diagnostics.log` and `build/v170_four_failure_diagnostics.xml` and exits nonzero if any fail. In a crash, capture the final `[folded phase] ...` line from the log. For a stack trace, run the Release `cadcore_tests.exe --folded-sheet-metal` in Visual Studio debugger.

If the four cases pass, then run the unchanged full 130-test gate:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_FINAL_PRETEST.ps1 -VcpkgRoot C:\vcpkg
```

**Release gate unchanged:** all 130 Windows native tests, interactive smoke tests, and separately recorded 60 acceptance scenarios must pass. Existing estimates (parametric 85%, assembly 90%, drawings/sheet metal 81%) are not measured acceptance scores.

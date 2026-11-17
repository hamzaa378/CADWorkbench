# CADWorkbench v1.70 — folded-edit crash diagnostic checkpoint (2026-11-09)

## Observed on Windows

The source from `CADWorkbench_v170_Windows130_RegressionRecovery_PRELIMINARY_20261109.zip` built and passed the following previously failing native tests:

- `cadcore_v170_articulated_scene_transaction` — 170 checks passed.
- `cadcore_v170_assembly_mate_document` — 135 checks passed.
- `cadcore_parametric_foundation` — 270 assertions passed.

The fourth, `cadcore_folded_sheet_metal`, still **crashes** after `[folded phase] editable folded geometry`, before the next phase marker. A segmentation fault in the Windows OCCT test process is **not** evidence of a normal, caught validation error. The root cause has **not** been established, and this checkpoint is **not** a confirmed crash fix.

## Source changes

1. The common folded-solid builder now checks `BRepPrimAPI_MakePrism::IsDone()` and a null result before exposing the shape. It retains the existing C++ exception/error handling and topology checks in production callers.
2. The existing folded-sheet regression logs ten additional flushed `[folded edit]` checkpoints around the preview, rejected preview, edit commit, undo/redo, save, and reload stages. This does not skip or weaken any assertion.
3. `RUN_V170_FOLDED_EDIT_DIAGNOSTICS.ps1` captures the single native test, its last marker, and JUnit output. The default invocation builds Windows Release first; `-SkipBuild` is only for reusing an already built working tree.

## Windows retest

From the extracted ZIP, in PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_FOLDED_EDIT_DIAGNOSTICS.ps1 -VcpkgRoot C:\vcpkg
```

The saved log is `build/v170_folded_edit_diagnostics.log`. If the test fails, send this log. The last `[folded edit]` message identifies which production call needs investigation. If it passes, run `RUN_V170_FINAL_PRETEST.ps1` and the separate 60-scenario acceptance evaluation.

## Release constraints

Source-level and portable C++ checks do **not** establish native success. The folded test remains enabled. No new features are introduced. Retain the last known Windows-built baseline until a fresh Windows run succeeds. The previous provisional functional estimates (85% parametric, 90% assembly, 81% drawings/sheet metal) are not formal acceptance scores.

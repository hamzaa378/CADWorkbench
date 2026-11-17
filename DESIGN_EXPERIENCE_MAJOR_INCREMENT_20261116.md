# CADWorkbench v1.70 — Design Experience Major UI Development (2026-11-16)

## Status and source

This is a **PRELIMINARY source branch** based byte-for-byte on the verified
`CADWorkbench_v170_95Target_ShopCalibration_PRELIMINARY_20261116_GitHub_Source.zip`
plus the explicitly documented changes. It is a native Windows C++/Qt/OpenCascade
application, not a runnable application ZIP or signed installer.

Preserve the last Windows-verified source (CTest #47 hotfix build: 130/130 native tests,
725/725 Python/source tests), separately from this new branch. The previous
ShopCalibration version passed 747 Python/reference tests and 20 portable C++
suites locally, but its new Windows build was pending.

## User-facing improvements (source implemented)

1. A selection-aware **PROPERTIES & ACTIONS** dock, with shortcuts into the
   existing feature history, rename, appearance, materials, part data, edge finishing,
   fit, visibility, and mass-property tools. Commands reuse their original
   QAction objects instead of duplicating modeling logic.
2. A dockable **DESIGN TIMELINE** for the selected body's saved feature history.
   Suppressed nodes and stale feature trees are explicit. Double-click opens the
   *existing* atomic feature editor, not a second unverified edit engine.
3. Right-click **DESIGN BROWSER** actions for everyday design editing.
4. Three coherent visibility actions: isolate selected, hide selected and show all.
   All use an atomic core batch API with one Undo/Redo checkpoint; invalid IDs
   reject the entire request. Core save/reopen and negative regression cases added.
5. **Workspace persistence** in local QSettings (window geometry, dock positions,
   active workspace), with View > Reset workspace layout. CAD model files are not
   rewritten by this preference system.
6. A persistent **bottom navigation strip** for camera views and body/face/edge/
   vertex selection, useful if the ribbon is hidden.
7. A **Recent projects** menu (ten items), saved in local settings. Missing files
   cannot be opened directly; opening prompts to save unsaved work.
8. A set of **original vector command glyphs** drawn at runtime for ribbon tool
   buttons (not derived from another CAD vendor's copyrighted icon artwork).
9. New View commands and shortcuts for selected-body visibility; a command ribbon
   toggle and all three dock toggle actions.
10. Dedicated **Drawing**, **Sheet Metal**, and **Surface** workspace tabs use
    existing commands without duplicating CAD algorithms. The workspace selector
    now navigates all seven workflows, rather than sending Drawing to File.
11. User-visible design-browser and timeline context refresh after scene and
    selection changes. Legacy scene serialization and engineering operations
    remain on the existing shared code path.

The visual organization is *inspired by* common CAD workspaces; this project is
not affiliated with or endorsed by Autodesk or Fusion 360.

## Controls and shortcuts

| Interaction | Action |
|---|---|
| Select a body | Left click in viewport or design browser |
| Add to selection | Ctrl+click |
| Select faces/edges | Bottom navigation strip or Select menu |
| Feature timeline | View > DESIGN TIMELINE; double-click a feature |
| Edit body | Right-click in design browser or use contextual inspector |
| Search commands | Ctrl+K (Ctrl+F focuses quick search) |
| Fit all / fit selected | F / Shift+F |
| Isolate selected | Ctrl+Shift+I |
| Hide selected | Ctrl+Shift+H |
| Show all | Ctrl+Shift+J |
| Reset panel layout | View > Reset workspace layout |
| Undo and redo | Standard Edit actions |

## Validation accomplished in Linux development runtime

- Python/source/OpenCascade reference regressions: **754/754 PASS**.
- Portable C++ executables: **20/20 PASS**.
- Native CTest registration inventory: **130** retained, unchanged.
- Added native assertions inside the existing geometry-and-persistence CTest:
  three-body isolate, one-step undo/redo, show/hide, missing-body rollback,
  empty selection, and save/reopen.
- Source manifest and ZIP integrity must pass on the emitted final archive.

**Not yet validated:** New Qt application compilation and UI smoke tests on Windows,
native execution of new visibility assertions, actual UI appearance, large assembly
responsiveness, and the 60 manual engineering acceptance scenarios. Local source
and portable tests do not prove the window renders correctly or meets the 95%
engineering completion targets.

## Windows fresh-source test from Downloads (PowerShell)

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$zip = Join-Path $downloads 'CADWorkbench_v170_DesignExperience_MAJOR_PRELIMINARY_20261116_GitHub_Source.zip'
$destination = Join-Path $downloads 'CADWorkbench_DesignExperience_Test'
if (!(Test-Path -LiteralPath $zip)) { throw 'Missing source ZIP' }
if (Test-Path -LiteralPath $destination) { throw 'Use a fresh test directory' }
Expand-Archive -LiteralPath $zip -DestinationPath $destination
$root = Join-Path $destination 'CADWorkbench_v170_DesignExperience_MAJOR_PRELIMINARY_20261116'
Set-Location $root
py -3 .\tools\preflight_major_build.py --root .
if ($LASTEXITCODE -ne 0) { throw 'Source manifest preflight failed' }
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Windows target: **130/130 native CTests and 754/754 Python/source tests**. An
exit status of 3 can mean the automated checks passed but manual acceptance is
still pending. Treat exit 1 and actual CTest failures as defects.

## Manual UI smoke checks to run on Windows

1. Start with empty project: ribbon, dock, navigation and status text render.
2. Add box and cylinder; select each in viewport and browser; contextual actions update.
3. Use Rename and Material from context panel; confirm correct source Part updates.
4. Right-click a feature child in the design browser; menu targets parent body.
5. Switch to Edge mode and choose an edge; only appropriate finishing commands enable.
6. Create a saved feature stack; check timeline node order and suppressed/stale badges.
7. Double-click timeline feature; existing editor opens, cancel without mutating.
8. Select two bodies, isolate; one undo restores all visibility, redo reapplies.
9. Hide selected, undo, show all, undo; check no partial visibility changes.
10. Save CAD scene, reopen and confirm visibility, parametric history and BOM unchanged.
11. Change workspace, move docks, close and reopen; previous layout restores.
12. Reset workspace layout; browser on left, properties on right, timeline below.
13. Hide command ribbon; navigation controls remain usable.
14. Save and reopen from Recent projects; missing paths are disabled.
15. Export STEP and drawing SVG from a representative part and check output.
16. Run full 130 CTests and the 60-scenario engineering acceptance workbook.

## Boundaries

- No claim of Fusion 360 functional equivalence; this is an independent CAD UI.
- No new topology solver, motion engine, sketch constraints, or drawing issue
  certification has been implemented by this UI-focused increment.
- Manual acceptance remains 0/20 (NOT_RUN) for each of the three workstreams.
- Full clean installer remains deferred until the ten-feature/engineering gates.

# CADWorkbench v1.70 — Design Experience 2 + Backend Productivity (2026-11-16)

**STATUS: PRELIMINARY.** This branch extends `CADWorkbench_v170_DesignExperience_MAJOR_PRELIMINARY_20261116_GitHub_Source.zip`.
The last Windows-verified automated engineering baseline remains the CTest47 corrected PersistentGeometry build
(130/130 native CTests, 725/725 Python/source tests). Neither this release nor its immediate DesignExperience parent has
been rebuilt with Windows Qt/MSVC in this development environment. Never silently substitute portable tests for native execution.

## Delivered functional changes

1. **Atomic scene-document batch rename** (`SceneDocument::renameBatch`): validates all proposed IDs and final names,
   supports name swaps, rejects case-insensitive final collisions, blank/overlong/multiline names and unknown IDs,
   and commits only after the entire plan is valid. One Undo/Redo event, unchanged geometry and persistent body IDs.
2. **Batch rename in the Qt UI** (Edit > Batch rename selected bodies, `Ctrl+Shift+R`, right-click design browser,
   or selection-aware inspector). Preview finding/replacing substrings, prefixes, suffixes, and optional three-digit numbering.
   Invalid/duplicate results disable Apply. The application calls only the transactional core API.
3. **Structured browser search** in `DesignBrowserQuery.h` with reusable standard-library-only tests:
   `name:`, `part:`, `material:`, `feature:`, `kind:solid/surface`, `state:visible/hidden/stale`, `has:features`.
   Combine fields with spaces (AND); use quotes for phrases. Unknown fields and malformed input fail closed.
   The browser view is filtered, not the geometry; source bodies and their feature histories remain unchanged.
4. **Browser scope chips** for All / Hidden / Stale / Solids / Surfaces, and a matching-body count.
   Search filters are retained when the model tree refreshes.
5. **Hierarchical assembly navigator**: a separate dock tab groups the same persisted component IDs by slash-separated
   `assemblyPath`, permits nested selection and double-click fit, and synchronizes with the design browser/viewport.
   It is read-only, never changes actual component parent relationships, and does not duplicate geometry or source parts.
6. **Actionable project-health audit**: filter errors / warnings, double-click a finding or select the affected body
   to focus it in the design browser and viewport. The audit remains read-only and never approves manufacturing.
7. **Regression controls**: native batch rename save/reopen/undo/redo/rollback cases inside the existing native
   geometry/persistence CTest, portable query tests, Python source-contract tests, no new CTest registrations.

## Local test results on source snapshot

- Python/source/OpenCascade reference: **761/761 passed** before final packaging.
- Portable C++ tests: **21/21 passed** before final packaging, including 17 structured-search assertions.
- Native Windows MSVC + 130 CTests: **NOT RUN on this branch**.
- Manual engineering acceptance: **0/20 NOT_RUN** for parametric, assembly, and drawings/sheet metal.
- No assertion of 95% engineering acceptance or production readiness.

## Install from full source on Windows (recommended)

In a fresh PowerShell session, download the matching ZIP into Downloads and run:

```powershell
$dl = Join-Path $env:USERPROFILE 'Downloads'
$zip = Join-Path $dl 'CADWorkbench_v170_DesignExperience2_Backend_PRELIMINARY_20261116_GitHub_Source.zip'
$folder = Join-Path $dl 'CADWorkbench_DesignExperience2_Trial'
if (!(Test-Path -LiteralPath $zip)) { throw 'Source ZIP missing' }
if (Test-Path -LiteralPath $folder) { throw 'Trial directory already exists; use a clean folder' }
Expand-Archive -LiteralPath $zip -DestinationPath $folder
Set-Location (Join-Path $folder 'CADWorkbench_v170_DesignExperience2_Backend_PRELIMINARY_20261116')
py -3 .\tools\preflight_major_build.py --root .
if ($LASTEXITCODE -ne 0) { throw 'Source preflight failed' }
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

The native test registration count is 130. The updated expected Python count is **761**. Audit status 3 means
all automated checks passed but 60 manual engineering scenarios remain unaccepted. Exit 1 means an automated failure.
Keep the previous passing Windows baseline in its own folder.

## Manual UI smoke checks (not automatically certified)

- Select three bodies; rename with Find/Replace, prefix, suffix, numbered suffix. Review preview collisions.
- Try a duplicate final name matching an unselected body; Apply must be disabled.
- Apply a valid rename; Undo must restore all names at once; Redo must reapply; save/reopen must preserve names.
- Confirm BOM, material, component occurrence ID and feature references are unchanged after renaming.
- Filter `material:steel`, `state:hidden`, `kind:surface`, `feature:fillet` and combinations; count must agree.
- Open Assembly Navigator, check nested `assemblyPath` folders, select several leaves and verify canvas/browser sync.
- Double-click a component in Assembly Navigator: viewport should fit that component; inspect untouched assembly metadata.
- Click All/Hidden/Stale/Solids/Surfaces chips. Clear filter and inspect tree expansion/selection behavior.
- Open project health audit; set Errors only, filter text, and double-click an issue with a valid body ID.
- Check context inspector, timeline, nav controls, workspace selector, recent projects, existing exporters.
- Run 130 CTests, 761 Python tests, and complete the 60 manual engineering scenarios with evidence.

## Limits and follow-up work

- Browser query engine performs ASCII case folding for ASCII keywords; accented text has case-sensitive UTF-8 bytes.
- The browser is a convenience view, not a topology naming or assembly mate solver.
- Batch rename updates display names, not part numbers, BOM grouping keys or external document filenames.
- There are no manufacturing approvals, geometry-identity guarantees through all Boolean splits/merges,
  or evidence of native Windows Qt rendering for this new branch.

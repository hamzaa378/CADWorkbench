# CADWorkbench v1.70 — Sprint 10 Integrated Engineering Workflow

**Date:** 2026-11-17. **Status:** locally verified combined *source candidate*. Windows MSVC, Qt/OpenCascade full native test and manual engineering acceptance still required for this changed source.

## Change policy and work allocation

The user selected **75% integration and completion of existing engineering workflows, 20% reliability/performance, and 5% genuinely missing capabilities**. This is a development-effort allocation, not a measured feature-completion percentage. Work on independent parametric/manufacturing, assembly/motion, and testing/diagnostics areas in parallel, merge to one source candidate, and gate each workstream independently. Preserve the Sprint 9 Windows-verified baseline (CADWorkbench.exe and 130/130 native CTests, 900 Python source tests before diagnostic hotfix) without modifying it. The Sprint 9 diagnostic-repair ZIP is the direct parent for Sprint 10; its Python-only repair has been carried forward.

### 75% — Complete connected engineering workflows

1. `src/core/HoleInspectionWorkflow.h` connects CAD-derived design-hole references to **independently supplied measured datums, projected axes, actual diameters, tolerance limits and pin sizes**. It strictly joins records using physical body ID, hole number, **matching saved geometry revision**, and one-to-one coverage. Measured records cannot overwrite nominal CAD coordinates. A missing measurement, duplicate identity or stale revision rejects the entire review without replacing the prior report. The existing `HolePositionInspection` engine performs numerical evaluation; it is still a conditional engineering worksheet, not CMM import, real datum alignment, or a certified GD&T evaluation.
2. `ManufacturingHandoff.cpp` now emits `hole_position_inspection_TEMPLATE.csv` in the **existing signed, UNISSUED manufacturing handoff**. Every row is populated from the exact same native B-rep `AxialHoleRecord` as `axial_hole_locations.csv` and bound to the body geometry token. **All measurement, tolerance and acceptance fields are blank**; rows have status `UNMEASURED`. This is usable as an independent metrology data-entry template **after copying it outside the signed package**. CSV fields are built directly from shape records, not parsed from an already escaped CSV row (supports commas/quotes in body IDs).
3. `tools/verify_manufacturing_handoff.py` validates the optional v2 template when present: one-to-one row coverage, matching source body/token/hole number and exact design coordinates/diameter, and no invented measurements or asserted approval. Existing signed v1/v2 handoffs **without** the new template still verify unchanged.

### 20% — Reliability and performance

4. Added `proximity::OrderInvariantTracker` in `SimultaneousProximity.h`. It canonicalizes observations by stable body ID, then delegates strict timestamp/identity, finite AABB and atomic error checks to the existing `Tracker`. This permits order changes when enumerating the same physical bodies across assembly regeneration poses without reassigning collision history pairs. The **original strict Tracker remains unchanged**. The articulated envelope exporter now uses the new adapter. This is still sampled-AABB broad-phase only, not exact continuous collision certification.
5. New regression cases exercise reversed observation order, duplicates, invalid bounds, stale revisions, incomplete measured sets, random measurement ordering, and large (512-hole) design patterns. The Windows staged compile gate now checks `cadcore`, `workshop_engineering_v164_tests`, `document_audit_tests` and `articulated_assembly_scene_v170_tests` before a full Qt application build, preserving the established 130 native CTests.

### 5% — Small missing capability

6. The unmeasured export template provides the previously absent bridge for inspectors to collect measurements while preserving CAD-design provenance. No unverified manufacturing status, automatic fixture suitability, or standards sign-off is introduced.

## Acceptance and limitations

- All 130 **existing CTest names** remain registered (NO missing or disabled tests). New native scenarios are integrated into CTest `cadcore_v164_tolerance_stacks` and `cadcore_document_audit`; articulated scene export is exercised by its existing native target.
- The new C++/Qt and OCCT source requires a **new Windows MSVC build** before any runtime claim. The prior Sprint 9 130/130 Windows result does not automatically verify Sprint 10 source changes.
- The independent Python source suite has 925 tests after adding 19 tests (seven synthetic manufacturing-handoff tests and 12 Sprint 10 source-contract tests). Full GCC/Clang suites have 38 portable C++ executables (one new, one expanded). See `qa/sprint10_local_*.log` for actual checked logs.
- Geometry metrology needs real measurement equipment, datum establishment, uncertainty limits and engineering sign-off. A hole table and pass/fail worksheet do not prove drillability, production quality, or ISO/ASME geometric conformity.
- No new drawing GUI or UI mode was added. No native CTest registrations or scene JSON serialization formats changed.
- Manual acceptance status remains **0/20 parametric, 0/20 assembly, 0/20 drawings/sheet metal formally executed**. Do not translate automated test counts into percentages of engineering readiness.

## Full Windows verification — fresh PowerShell

Save the Sprint 10 combined ZIP in Downloads and run from a new PowerShell window:

```powershell
Set-Location "$env:USERPROFILE\Downloads"
$zip = "CADWorkbench_v170_BackendSprint10_IntegratedEngineering_COMBINED_PRELIMINARY_20261117.zip"
if (!(Test-Path -LiteralPath $zip)) { throw "Sprint 10 ZIP not in Downloads" }
$dest = "CADWorkbench_Sprint10_Test_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
Expand-Archive -LiteralPath $zip -DestinationPath $dest
Set-Location (Join-Path $dest "CADWorkbench_v170_BackendSprint10_IntegratedEngineering_PRELIMINARY_20261117")
$log = Join-Path $env:USERPROFILE "Downloads\CADWorkbench_Sprint10_Targeted.log"
Start-Transcript -Path $log -Force
try {
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT10_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4
  $code=$LASTEXITCODE
} finally { Stop-Transcript }
Write-Host "TARGETED EXIT: $code (4 = targeted native pass, full audit pending)"
Write-Host "LOG: $log"
```

Only after the targeted gate passes, from the same project directory, run full app, 130 native CTests, and Python/source audit:

```powershell
$log = Join-Path $env:USERPROFILE "Downloads\CADWorkbench_Sprint10_FullAudit.log"
Start-Transcript -Path $log -Force
try {
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT10_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4 -FullAudit
  $code=$LASTEXITCODE
} finally { Stop-Transcript }
Write-Host "FULL EXIT: $code (3 may mean automated PASS but manual acceptance pending)"
Write-Host "LOG: $log"
```

Upload both logs on any failure. If the full audit returns exit 1 despite printed test passes, also supply `build/v170_comprehensive_audit/backend_diagnosis.json`; the inherited diagnostic parser repair handles current CTest summary wording but every new release candidate still requires verification.

## Next parallel workstreams

**Parametric/manufacturing integration:** Explicit independent-measurement file importer and schema validation, audit original source revisions against refreshed geometry, round-trip with actual drawings and verified metrology reference. **Assembly:** propagate physical body identity through imported component instance permutations and spatial indexing of broad-phase pair tracking; exact collision investigation for unresolved AABB contact. **Drawings/sheet metal:** complex multi-flange folded/flat associativity and real drawing release checklists. **Quality:** Windows build, all 130 CTests, then manual 60-scenario acceptance (target at least 19 of 20 in each workstream with all critical scenarios passing), no fake acceptance evidence. When any native test fails, fix it before promoting the candidate.

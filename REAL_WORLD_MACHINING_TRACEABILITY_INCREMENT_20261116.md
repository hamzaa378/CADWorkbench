# CADWorkbench v1.70 — Geometry-linked manufacturing handoff / machining traceability

**16 November 2026 — PRELIMINARY SOURCE. Not manufacturing approval, Windows-certified, or an installer.**

## Why this matters

The saved-design / STEP / BOM / inspection handoff was verifiable but contained no explicit review of geometrically identified holes. Procurement and manufacturing staff could not tell whether a body had no holes or the hole inspection was not supported. This increment closes the *traceability gap*, without claiming to solve multi-axis machining or generate G-code.

## Source changes

1. **Native geometry-derived axial-hole inventory:** `src/core/ManufacturingHandoff.cpp` calls the existing conservative `inspectAxialHoles` for each eligible solid body when building an immutable handoff. It exports `axial_hole_locations.csv` with stable body ID, geometry revision token, hole number, X/Y coordinates, diameter, Z limits and envelope-spanning classification. This is inspection evidence, not a hole callout or toolpath.
2. **100% part-coverage accounting:** `machining_coverage.csv` has exactly one record for each body. Possible values are `INSPECTED_AXIAL_ONLY`, `INSPECTED_NO_AXIAL_HOLES`, `PARTIAL_CYLINDERS_EXCLUDED`, `NOT_EVALUATED`, and `SURFACE_NOT_APPLICABLE`. A failed inspection is never silently treated as zero holes. Skipped cylinders and reasons are recorded and require separate review.
3. **Handoff schema v2 + backwards compatibility:** The handoff manifest includes `machining_holes` and `machining_review_required`. The verifier reads both earlier v1 handoffs and new v2 handoffs; a new v2 must have both tables and correct counts. Every file remains SHA-256 hashed inside the staged package before atomic publication.
4. **Real cross-file validation:** `tools/verify_manufacturing_handoff.py` validates body coverage against the occurrence list, hole counts per body, consistent source-geometry tokens, finite positive dimensions, unique sequential hole numbers, status semantics and exact manifest totals. Rehashing altered CSVs is insufficient to defeat these structural checks.
5. **Change impact and review requirements:** Comparing handoffs recognizes changes in machining tables, records added/removed hole measurements, and triggers `MACHINING_AXIAL_REVIEW` with `hole_location_inspection`, `tool_access_review` and `fixture_exit_review` evidence. Existing v1-to-v2 migrations are marked for review rather than silently accepted.
6. **GUI integration:** The existing File → Create UNISSUED manufacturing handoff command displays observed hole count and bodies needing follow-up review on completion.
7. **Native regression assertions:** Added to existing `cadcore_document_audit`, preserving exactly 130 CTest registrations. The assertions create and save a simple solid, export a handoff, and check its v2 tables and manifest. These C++ assertions are *not yet Windows-tested*.

## Safety and limits

- Hole inspection is *strictly* based on complete, axial (global Z) internal cylindrical faces of single solids. It does not identify all non-axial or partial cylindrical features, grooves, countersinks or machining setups.
- `PARTIAL_CYLINDERS_EXCLUDED` and `NOT_EVALUATED` explicitly require separate investigation. A zero-hole result is not a declaration that a part requires no machining.
- File hashes detect changes, not authenticity, identity or dimensional equality between the native model and STEP.
- This change does not implement actual 3D CAM, machining processes, purchasing approvals or issue a production release.
- Existing v1 handoff packages remain readable; current exports use v2 and preserve unissued review status.

## Verification in this container

- Python/source/reference: **704/704 passed**.
- C++ portable executables: **19/19 passed** (unmodified inventory).
- Native CTest registration: **130 preserved**; updated native source not yet compiled/run on Windows.
- Engineering manual acceptance: **0/20 each; NOT_RUN**. Provisional percentages remain not recertified.
- No installer or deployable Windows EXE included. Preserve the last Windows-tested Fix 2 baseline.

## Windows verification from a fresh PowerShell session

Download `CADWorkbench_v170_MachiningTraceability_PRELIMINARY_20261116_GitHub_Source.zip` to Downloads. Close running CADWorkbench instances. **Do not reuse an existing CMake build folder.**

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$zip = Join-Path $downloads 'CADWorkbench_v170_MachiningTraceability_PRELIMINARY_20261116_GitHub_Source.zip'
$destination = Join-Path $downloads 'CADWorkbench_MachiningTraceability_Native_Test'
if (!(Test-Path -LiteralPath $zip)) { throw 'Download the source ZIP first.' }
if (Test-Path -LiteralPath $destination) { throw 'Destination already exists: use a new empty folder.' }
Expand-Archive -LiteralPath $zip -DestinationPath $destination
$root = Join-Path $destination 'CADWorkbench_v170_MachiningTraceability_PRELIMINARY_20261116'
Set-Location $root
py -3 .\tools\preflight_major_build.py --root .
if ($LASTEXITCODE -ne 0) { throw 'Source manifest preflight failed.' }
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Acceptance target: **130/130 native CTests**, **704/704 Python/source tests** and application launch. Exit code `3` may mean automated checks passed but all 60 manual scenarios are still NOT_RUN; it is not a production release approval.

## Handoff review from a fresh PowerShell session

```powershell
$root = Join-Path $env:USERPROFILE 'Downloads\CADWorkbench_MachiningTraceability_Native_Test\CADWorkbench_v170_MachiningTraceability_PRELIMINARY_20261116'
Set-Location $root
py -3 .\tools\verify_manufacturing_handoff.py 'C:\path\to\new_unissued_handoff'
py -3 .\tools\compare_manufacturing_handoffs.py 'C:\path\previous_handoff' 'C:\path\new_unissued_handoff'
py -3 .\tools\assess_change_control.py 'C:\path\previous_handoff' 'C:\path\new_unissued_handoff'
```

Do not place review outputs inside either immutable handoff folder. Never use these outputs to substitute for engineering inspection and process approval.

## Next major build/release gate

Only after the Windows MSVC Qt6/OpenCascade build and all 130 native CTests are green should the new snapshot become the next candidate baseline. The independent 60-scenario engineering acceptance and final release gate remain required. Clean installation and production ZIP are not issued by this preliminary increment.

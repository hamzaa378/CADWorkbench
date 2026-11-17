# CADWorkbench v1.70 — Engineering review and safe export hardening

**PRELIMINARY source update, 15 November 2026. NOT a production, manufacturing, or Windows-verified release.**

## What changed relative to the RealWorldEngineeringHandoff snapshot

1. **Fix mixed-revision handoffs:** after exporting source/STEP/BOM/audit/inspection files, the C++ handoff exporter reopens and re-hashes the original saved CAD file. If it changed while exports were running, publication aborts and the temporary staging directory is removed. This is an external-edit race check; it does not prove geometric STEP equivalence.
2. **Check BOM-to-assembly consistency before publication:** each grouped BOM quantity must be positive and the sum of all quantities must equal the count of physical scene parts. A mismatch blocks handoff.
3. **Harden existing CSV exporters:** `PartsData.cpp` BOM and occurrence parts-list cells, and the `SceneDocument.cpp` body schedule text cells, now prefix a literal quote when their trimmed data begins with a spreadsheet formula introducer (`= + - @`). Double quotes remain CSV-escaped. This extends existing protections used by the new audit and procurement CSV exports.
4. **Cross-check exported package files:** the standalone verifier independently checks body IDs and counts across occurrences and body schedule; verifies BOM summed quantities, unique BOM items, and procurement issues linked to real BOM items with matching quantities; and verifies inspection requirement body IDs and review states. Bad or inconsistent data is rejected **even if its SHA-256 hashes have been regenerated**. This is data-consistency checking, not cryptographic authenticity or approval.
5. **Richer change impact report:** the revision comparison now includes body schedule and BOM digest changes, added/resolved procurement exception categories by part number (count-aware), added/changed/removed inspection requirements, stale inspection requirements, and explicit areas requiring review. Item order is not mistaken for identity.
6. **Actionable manufacturing punch list:** `tools/review_manufacturing_handoff.py` reads a verified, UNISSUED package and emits a review summary and optionally standalone CSV/JSON reports. It includes audit warnings, supplier/material exceptions and every feature inspection requirement. Outputs **cannot** be placed inside the protected handoff folder or overwrite existing files. Spreadsheet text is safely escaped. A fully empty checklist **does not** constitute a manufacturing release.

## Commands: read-only verification, comparison and review

From **a fresh Windows PowerShell terminal**, once the *new* Qt application has been compiled, tested, a CAD file saved, and a handoff created from File → Create UNISSUED manufacturing handoff package:

```powershell
$root = Join-Path $env:USERPROFILE 'Downloads\CADWorkbench_ProductionReadiness_Test\CADWorkbench_v170_RealWorld_ProductionReadiness_PRELIMINARY_20261115'
Set-Location $root
$handoff = 'C:\PATH\TO\CADWorkbench_Engineering_Handoff_...'
py -3 .\tools\verify_manufacturing_handoff.py "$handoff"
if ($LASTEXITCODE -ne 0) { throw 'Handoff integrity or cross-file consistency failed' }
py -3 .\tools\review_manufacturing_handoff.py "$handoff" `
  --csv "$env:USERPROFILE\Downloads\unissued_action_list.csv" `
  --json "$env:USERPROFILE\Downloads\unissued_action_list.json"
py -3 .\tools\compare_manufacturing_handoffs.py `
  'C:\PATH\TO\OLD_HANDOFF' "$handoff" `
  --json "$env:USERPROFILE\Downloads\unissued_revision_review.json"
```

Reports will refuse to overwrite files. Choose new output paths or remove reviewed old reports intentionally. The handoff folder is kept immutable.

## Full source ZIP: clean Windows audit

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$zip = Join-Path $downloads 'CADWorkbench_v170_RealWorld_ProductionReadiness_PRELIMINARY_20261115_GitHub_Source.zip'
$destination = Join-Path $downloads 'CADWorkbench_ProductionReadiness_Test'
if (!(Test-Path -LiteralPath $zip)) { throw 'Download the full source ZIP to Downloads first.' }
if (Test-Path -LiteralPath $destination) { throw 'Use a new destination folder; preserve the previous tested source.' }
Expand-Archive -LiteralPath $zip -DestinationPath $destination
Set-Location (Join-Path $destination 'CADWorkbench_v170_RealWorld_ProductionReadiness_PRELIMINARY_20261115')
powershell -NoProfile -ExecutionPolicy Bypass -File '.\RUN_V170_COMPREHENSIVE_AUDIT.ps1' `
   -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit: $LASTEXITCODE"
```

**Requirements:** Windows with the existing Qt/OpenCascade vcpkg + Visual Studio C++ environment. The archive is **source**, not a compiled installer. The audit may return code `3` when the automated tests pass but the 60 engineering acceptance cases remain NOT_RUN. Actual native results must be read from the audit log, not assumed.

## Verification performed in current development environment

- 678/678 project Python/source/reference tests passed.
- 19/19 portable standard C++ test executables passed.
- New handoff focused regression suite includes cross-file consistency, metadata changes and output safety.
- Windows-native build and 130 native CTests: **NOT run for this increment**.
- Engineering manual acceptance P/A/D: **NOT_RUN / NOT_RUN / NOT_RUN**; no 85% or 100% claim.

## Not included

No release signing, approved engineering drawings, manufacturing certification, G-code, supplier orders, inspection measurement results, complete F01-F10 acceptance, or installed Windows binary. SHA-256 checks find changes but are not authenticity signatures. The STEP exporter retains its existing limitations. This preliminary development update must not overwrite the current Windows build under test.

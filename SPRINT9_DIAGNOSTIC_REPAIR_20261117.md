# Sprint 9 diagnostic parser correction — November 17, 2026

This is a Python verification-tool hotfix based on the already Windows-compiled Sprint 9 source.

## What was wrong
The original `tools/diagnose_build.py` recognized older CTest summaries containing `0 tests failed`, but not the CTest 4.4 output `100% tests passed out of 130`. It falsely labeled a complete 130/130 passing CTest run as missing native evidence.

## Changes
- Accept both observed CTest success-summary formats; require all 130 tests, at least 900 passing Python tests, and explicit full-audit success.
- Add six negative/positive regression cases. The old synthetic `Native 130/130` fixture is still rejected.
- No changes to C++/Qt/OCCT source, build configuration, or existing native test registrations.
- Both source manifests updated for the changed Python files.

## Verification
- New targeted diagnostic unit tests executed successfully (19 total).
- The real uploaded Sprint 9 Windows audit transcript is reclassified `AUTOMATED_PASS`, with no issues.
- Native Windows MSVC/Qt/OCCT build **not rerun** for this documentation/tooling-only hotfix; the user's completed Sprint 9 log reported CADWorkbench.exe built and 130/130 native CTests passed.
- Manual acceptance stays 0/60 pending, so this remains NOT A FINAL RELEASE.

## Use with an existing compiled Sprint 9 tree
Run `APPLY_TO_EXISTING_SPRINT9.ps1 -ProjectRoot <path>` from the extracted combined ZIP; it updates only Python diagnostic files and the two checksum manifests. Existing binaries and build tree are preserved. Then run the diagnostic against `build/v170_comprehensive_audit/backend_full_console.log`, without rerunning MSVC:

```powershell
py -3 .\tools\diagnose_build.py --log .\build\v170_comprehensive_audit\backend_full_console.log --out .\build\v170_comprehensive_audit\backend_diagnosis.json --expected-python 900
```

This demonstrates automated evidence only; it does not complete engineering acceptance.

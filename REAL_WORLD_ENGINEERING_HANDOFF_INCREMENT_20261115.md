# CADWorkbench v1.70 — Real-world engineering handoff increment (15 Nov 2026)

**PRELIMINARY DEVELOPMENT BRANCH — NOT A VERIFIED WINDOWS BUILD OR A MANUFACTURING RELEASE.**

## Design goal

Bridge the gap between a CAD model and a controlled engineering handoff. The source now has a new **File → Create UNISSUED manufacturing handoff package...** workflow. It collects actual model outputs from one saved project state and does not mutate the document or silently approve fabrication.

### Capabilities implemented

1. **Transactional source-to-STEP engineering package.** Saved original project, structured STEP assembly, generated BOM, individual occurrence parts list, and body schedule are written to a private staging directory. On any failure the staging directory is deleted. An atomic same-parent rename publishes the completed folder, and existing output folders are never overwritten.
2. **Manufacturing-data readiness review.** Existing comprehensive document audit blocks handoffs with any ERROR findings. BOM-level supplier/stock exceptions are tabulated separately (missing identifiers/revisions, unknown material or mass, missing supplier/catalog numbers). Warnings remain visible. The source never assumes manufacturing approval.
3. **Feature-level inspection register.** Existing source-feature inspection notes are exported with the actual body/feature references, requirement text, and geometry token review status. This is an inspection-planning register, not a metrology report.
4. **Independent package-integrity verifier.** `tools/verify_manufacturing_handoff.py` strictly verifies the manifest schema, status, filenames, SHA-256 digests, file lengths, counts and CSV consistency. It rejects added files, symlinks, modified bytes, and falsely issued states. Hashes do not authenticate the author.
5. **Revision comparison.** `tools/compare_manufacturing_handoffs.py` independently validates two handoffs, reports stable occurrence-ID additions/removals, changed part fields, changed source CAD bytes and changed STEP bytes, and summarizes review exceptions. It is a design-change review aid, not geometric equivalence or revision sign-off.

### Files

- `src/core/ManufacturingHandoff.h/.cpp`: scene-integrated transactional export engine
- `src/ui/MainWindow.cpp/.h`: explicit menu action with confirmation and clear unissued warnings
- `tools/verify_manufacturing_handoff.py`: standalone portable verifier
- `tools/compare_manufacturing_handoffs.py`: change impact review
- `tests/v170_manufacturing_handoff_*.py`: verifier, comparison and source-integration tests
- `CMakeLists.txt`: registers the new Qt/OpenCascade source module

### Windows user workflow (after Windows build succeeds)

1. Open the newly built CADWorkbench application and a project containing valid solids and metadata.
2. Save the source design. The handoff will refuse unsaved or modified documents.
3. Open **File → Create UNISSUED manufacturing handoff package...**.
4. Confirm and choose an existing parent folder. The app creates a **new** folder, never overwrites an existing handoff.
5. Fix audit errors if blocked. Warnings or procurement exceptions are included for explicit engineering review.
6. Open PowerShell in the project source directory and run:

```powershell
$python = 'py'
& $python -3 .\tools\verify_manufacturing_handoff.py 'C:\PATH\TO\CADWorkbench_Engineering_Handoff_...'
& $python -3 .\tools\compare_manufacturing_handoffs.py 'C:\PATH\TO\OLD_HANDOFF' 'C:\PATH\TO\NEW_HANDOFF' --json 'C:\PATH\TO\change_review.json'
```

The output stays **UNISSUED_REVIEW_REQUIRED**, even after integrity verification. Approved drawings, tooling, metrology and engineering sign-off are outside this scope.

### Local verification

- Python/source/reference test suite: **656/656 PASS** before packaging; rerun on extracted ZIP before reporting final count.
- Portable standard C++ test executables: **19/19 PASS**.
- Handoff verifier synthetic tests: **16**; comparison tests: **7**; source integration checks: **9**.
- Native Windows CTest names: **130 preserved**, NOT EXECUTED for this increment.
- Windows C++/Qt/OpenCascade compilation: **NOT RUN**. Native build and manual acceptance needed.
- The previous Windows-tested Fix 2 baseline is unchanged.

### Known limitations

- This increment does **not** add G-code, machine setup, geometric tolerancing proof, numerical inspection results, approval signatures, manufacturing drawings, or a Windows installer.
- The assembly STEP writer's existing geometry/assembly limitations still apply.
- Comparing occurrence CSVs uses persistent body IDs and textual attributes, not kernel equivalence. Changed geometry without a CSV field change is detected only via STEP/source hashes, not localized precisely to a face.
- Audit warnings do not automatically block creation; humans must review them.
- Never use this package alone to justify manufacture or certify completion of F01–F10.

### Full Windows audit (run from clean extracted source root)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit: $LASTEXITCODE"
```

Expected development goal: 130 native CTests and **656** Python/source/reference tests, but only report PASS after the Windows audit actually runs. With manual acceptance `NOT_RUN`, exit 3 may be expected if the automated gate passes.

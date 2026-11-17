# CADWorkbench v1.70 — three-track engineering increment (source-only)

**Status:** preliminary / unverified on Windows. Based on the November 10 AssemblySheetQA source. Keep the earlier Fix 2 Windows-proven build as the rollback baseline. All 130 native CTest registrations and the 60-scenario release gate are unchanged.

## 1. Parametric modeling — conflicting duplicate-face protection

`ShapeFeatureTree::findFace` now examines both the original interior planar witness and the body-relative displaced witness before silently accepting a unique original witness. If they indicate distinct copied planar faces, it rejects the feature with an explicit rebind diagnostic. This prevents the accidental retarget of an attached pocket or hole to a copied solid when the body's mass center changes. The previously supported unique split/merge witness and translated-body cases are retained. Native test: `cadcore_v170_face_lineage`, using a three-cube compound with distinct supported witness points, plus the independent OCCT Python reference fixture.

**Scope:** No universal topological naming; only a specific dangerous duplication/translation ambiguity is addressed. The real Windows B-rep resolver test has not yet run for this increment.

## 2. Assembly engineering — independent fixed-frame paths

`auditMateStructure` now computes capped (0/1/2/3+) link-disjoint paths back to the fixed occurrence using residual unit-capacity max flow, with support for multiple separate mate links between the same two occurrences. The existing **Assembly > Audit saved rigid mate structure and cycle links...** report now includes each occurrence's redundancy, not just bridge links and single-occurrence failures. One path means loss of one link can disconnect that occurrence; two or three indicates alternative structural routes. The assessment is graph-only and not a geometric mate, dynamic-load, collision, or tolerance certification.

Regressions: original and expanded `assembly_network_structure_audit_tests.cpp` portable tests. The implementation is integrated in the live report dialog.

## 3. Drawings — batch linked-dimension health audit

New **File > Audit all linked drawing dimensions...** scans the saved drawing's linked annotations using the SAME OCCT witness and projected-dimension preview required for a real SVG render. Each annotation is classified `CURRENT`, `REGENERATE_REVIEW`, `BROKEN_REFERENCE`, or `MISSING_SOURCE`. The read-only dialog supports atomic CSV export. It NEVER acknowledges or edits source revision tokens; a drawing must still be explicitly regenerated and reviewed before issue. Native test cases were inserted into the existing `cadcore_orthographic_drawing` target, preserving the CTest inventory.

**Scope:** This does not automatically update drawing geometry, solve GD&T standards requirements, or implement multi-sheet associativity.

## Source-level verification

- 587/587 Python/source/OpenCascade reference tests passed locally.
- 16/16 standalone portable C++20 executables passed locally with g++ (`-O0` parallel build).
- Exactly 130 CTest test registrations retained; verification of native runtime remains pending.
- All source hashes included and verified in the package.
- Windows MSVC/Qt/OCCT application and native tests NOT yet rerun for this snapshot.

## Windows audit

Extract the ZIP to a **fresh, empty** folder, open PowerShell in the extracted project root, then:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Targets: 130 native CTests, 587 Python tests, no source-integrity failures. Exit code 3 means *only* the manual acceptance gate is pending **if** all automated checks pass. Any build or test failure must be investigated without disabling the test.

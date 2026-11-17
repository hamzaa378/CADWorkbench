# v1.70 development increment: static assembly fit and clearance audit

## Source baseline

Derived from `CADWorkbench_v1_70_Atomic_Linked_Body_Batch_Source_20261105.zip` (101 native CTests registered). The previous 101-test candidate has not been reported as Windows-verified. This new package does **not** claim completion of the 50-feature roadmap.

## Implemented

- **Inspect → Assembly fit audit: all pairs, clearance and overlap...** scans the currently selected 2–32 solid bodies; with no selection it scans the visible solids. Scene evaluation is read-only and uses the B-reps already in the document.
- Every unordered body pair is inspected using the existing OpenCascade `BRepExtrema_DistShapeShape` and `BRepAlgoAPI_Common` paths through `SceneDocument`. No bounding-box approximation is substituted for the reported distance or shared volume.
- Distinct states: **Interference** (nonzero shared solid volume), **Contact** (zero measured distance, no shared solid volume), **Below clearance** (positive separation below the specified millimetre limit), and **Clear**. Overlap takes precedence over distance, since nested solids can have nonzero surface separation while overlapping in volume.
- Rows record stable body IDs, names, snapshot geometry tokens, measured minimum distance and overlap volume; optional closest points are retained in the C++ report structure.
- A report-text view includes an overall classification summary and warns against treating static collision-free geometry as manufacturing or motion clearance certification.
- **Export CSV** uses an atomic `QSaveFile` write and quotes all identifiers and names to prevent malformed comma/quote/newline records. **Copy report** places plain text on the clipboard.
- Scan validation rejects duplicate/unknown/null/surface bodies, non-finite thresholds and groups above 32 bodies. A failed scan does not change the prior result or the document.
- The `assemblyFitReportCurrent` utility detects invalidation when any source B-rep geometry token changes, so stale results are not accidentally reused as fresh.

## Explicit limitations

This is a **snapshot inspection tool**, not a solver for assembly mates, design constraints, swept clearance envelopes or manufacturing tolerances. It does not modify CAD geometry or the linked-body graph. The two OpenCascade measurements can be expensive on complex B-reps; 32 bodies is an upper safety bound rather than a performance guarantee. Surface contact is not treated as a positive volumetric interference. Motion between sampled poses and thermal/tolerance variation must be assessed separately.

## Verification and release gate

- Portable classification: 512 checks under `g++` and `clang++`, plus Clang AddressSanitizer/UndefinedBehaviorSanitizer.
- 24 legacy/new source-contract and preflight suites passed at packaging time.
- Four OpenCascade independent reference geometries reproduce clear, below-clearance, contact, and overlap states, including 800 mm³ of overlapping volume.
- CMake parses and reaches its intentional Windows-only guard in this Linux environment.
- Three new Windows CTests are registered: `cadcore_v170_assembly_fit_policy`, `cadcore_v170_assembly_fit_audit`, and `cadcore_v170_assembly_fit_report_snapshot`.
- **104 native CTests registered, not executed here.** GUI compilation, QSaveFile interactions and the new SceneDocument integration require the actual Windows build.

To verify on Windows from the newly extracted source directory:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 104
```

After 104/104 tests pass, manually open a box pair at 8 mm gap, 3 mm gap, touching, and 4 mm overlap to inspect and export the report. Retain the last passing source/binary until this gate is complete.

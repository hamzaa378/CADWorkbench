# CADWorkbench v1.70: Four-feature 85% target, follow-up increment — 10 November 2026

This is a **PRELIMINARY SOURCE candidate**, not a Windows installer or a production release. The last Windows-tested Fix 2 source remains unchanged. No manual acceptance has been marked as passed.

## Implementation delivered in this increment

**F05 Geometric assembly mating:** The `PlanarMateSelection.h` B-rep observation adapter now supports selected *cylindrical* mating faces as well as planar faces. It checks true `GeomAbs_Cylinder` support, finite radius/area, projects the trimmed face centroid onto the cylinder's physical axis, canonicalizes direction, and returns a perpendicular world guide. Failed observations leave caller output untouched. A new button in the existing six-DOF mate solver pre-fills center/axis/guide from two real selected component faces, using stale-selection and wrong-part checks. Two cylinders **do not constrain axial sliding or rotation**; the dialog requires the user's affirmative choice to impose extra axial-position and twist locks. No mate is created before the final solve/confirmation. The surface radius is displayed, not treated as automatic clearance approval. Existing scene-file format remains unchanged.

**F08 Associative drawing review:** The all-saved-sheet unissued SVG/PDF preview command now writes `svg_sha256` and `pdf_sha256` columns to `drawing_index.csv` for each committed file. A standalone read-only verifier (`tools/verify_unissued_drawings.py`) validates the exact file sequence, SHA-256 values, all UNISSUED flags, and absence of extra files/symlinks. It rejects forged or incomplete folders; it does **not** certify drawing geometry or approve a revision. Existing multi-sheet preflight and rollback paths remain in place.

**F01/F02 stable topology:** The preceding curved-face and two-stage spline-edge witness preservation, Boolean provenance checks, and fail-closed rebinding are retained without unverified changes in this increment. Topological naming across all arbitrary splits and merges is still not complete. No F01/F02 certification is claimed.

## Observed verification (local development environment)

- Python/source/reference: **618/618 passed**, including 9 actual preview-directory integrity tests and 6 new source integration checks.
- Portable C++: **18/18 GCC executables passed** (not a Qt/OpenCascade Windows build).
- Native Windows CTest registration: **130 unchanged** (registration is not execution).
- New native test assertions: cylindrical selected-face observations, radius, unit axis, perpendicular guide, and failed-output preservation. These require a Windows build before being counted as passing.
- Source archive: regenerated SHA-256 manifests, verified against the extracted ZIP.
- Upgrade: guarded source-only upgrade from the exact previous `85Progress` GitHub source snapshot; tested for precheck, repeat install, rollback, and wrong-baseline rejection.
- Windows MSVC/Qt/OpenCascade compilation: **NOT RUN**. Native Windows CTests: **NOT RUN**. GUI acceptance: **NOT RUN**.

## Four individual 85% targets (at least 17/20 each, all critical checks)

| Feature | This increment | Target status |
|---|---|---|
| F01 face identity | Previous multi-stage witness and Boolean validation retained | Not certified; complex topology+Windows/GUI pending |
| F02 edge identity | Previous spline/rotation regression coverage retained | Not certified; Boolean→fillet/chamfer+Windows/GUI pending |
| F05 geometric mates | Added real selected cylinder observations and opt-in rigid locks | Not certified; Windows-native+GUI/live persistence pending |
| F08 associative drawings | Added cryptographic preview file verification | Not certified; Windows rendering, issue/approval and GUI pending |

Historical workstream estimates remain **Parametric ~85%, Assembly ~90%, Drawings/sheet metal ~81%**. These are NOT newly measured percentages. Separate 60-scenario manual acceptance is P **0/20 NOT_RUN**, A **0/20 NOT_RUN**, D **0/20 NOT_RUN**. The final release is BLOCKED.

## Test in PowerShell on Windows, without changing a passing baseline

1. Download and extract the **full source ZIP** into a brand-new directory. Enter the folder containing `CMakeLists.txt`.
2. Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 `
  -VcpkgRoot "C:\vcpkg" -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Target: **130/130 native CTests** and **618/618 Python/source tests**. Do not mark Windows tests PASS before actual log evidence. Exit 3 can mean all automated tests pass but manual acceptance is pending.

3. For F05: select two component occurrences, then exactly one cylindrical face per occurrence. Open the existing assembly six-DOF mate solver, click **Prefill from two selected cylindrical faces...**, inspect radius/axes and the opt-in extra locks, then preview, save/reopen and undo. Intentionally try a planar face, stale selection, and mismatched occurrences.
4. For F08: **File → Regenerate ALL saved drawing previews (not issued)...**. Choose PDF yes/no. In the created folder, verify:

```powershell
py -3 .\tools\verify_unissued_drawings.py "C:\path\to\CADWorkbench_Unissued_Drawings_..."
```

Check that tampering with an SVG/PDF fails verification. This does not authorize fabrication.

## Upgrade option

The guarded upgrade ZIP targets **only** the prior `CADWorkbench_v170_FourFeatures_85Progress_PRELIMINARY_20261110_GitHub_Source.zip` source tree. Extract the upgrade kit separately and run its `README_START_HERE.md`. `--check` validates source and payload hashes, `--apply` backs up originals outside the source tree, and `--rollback` restores them. Never apply to Fix 2 or the older FourFeatures snapshot.

## GitHub status

Commit the full source snapshot to a *development* branch with this note and SHA-256 manifest. Do not publish it as a verified installer/release; all 10 major features and the release engineering gate must be completed first.

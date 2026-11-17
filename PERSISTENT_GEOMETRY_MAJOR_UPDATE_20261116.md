# CADWorkbench v1.70 | Persistent Geometry — Major Development Increment

**Status:** Preliminary source candidate. **Do not treat as a production release.** The last Windows-tested Fix 2 source is preserved separately.

## Current baseline
- Immediate predecessor: `CADWorkbench_v170_MachiningTraceability_PRELIMINARY_20261116_GitHub_Source.zip`.
- New full-source snapshot: `CADWorkbench_v170_PersistentGeometry_MAJOR_PRELIMINARY_20261116_GitHub_Source.zip`.
- Upgrade is restricted to the **exact** MachiningTraceability source; never use on FiveEngineeringReviews or earlier source.
- Source remains C++/Qt6/OpenCascade; no schema changes to `.cadscene`, and no existing GUI feature or test registration is intentionally removed.

## New architecture and behavior

### 1. Similarity-based persistent geometry tracking
An explicit, one-to-one reference lineage stage can now use:
- Rigid translation or rotation (previous behavior preserved).
- Positive **uniform scaling** within the operation's existing validated limits (`factor` 1e-4..1e4).
- Mirror XY, XZ, YZ, or a mirror about a specified plane.

The current geometry, never a stale cache, must contain exactly one successor matching the saved physical witness. A split through the witness or an ambiguous duplicate still fails closed. All ancestry routes in a Boolean diamond must agree before commit. The source file remains unchanged if a dependent rebuild fails.

Uniform scale **transports geometry references, not engineering tool dimensions**. E.g., a downstream 4 mm drilled hole stays a 4 mm hole even when the upstream stock is scaled. Edge length scales by factor, curvature by its reciprocal; planar support areas scale by factor squared; analytic curved radius by factor (torus minor radius likewise). Unit tangent/normal/axis vectors are renormalized.

Analytic curved shell references (cylinder, sphere, cone, torus) are handled with existing current-kernel witness checking. Freeform B-spline/Bezier shell faces are deliberately blocked from automatic uniform-scale rebinding until additional geometric certification is implemented.

`supportsRigidReferenceLineage`, `rebaseFeatureThroughRigid`, and `transportDesignPointThroughRigid` remain strict rigid-only APIs for existing callers. New similarity APIs are used by the interleaved ancestry replay. No new persisted fields or migrations.

### 2. Static reference-health auditor for saved projects

`tools/audit_persistent_references.py` reads the saved `.cadscene` **without mutation** and reports all explicit interleaved feature references, their tracked paths, and missing/unsupported ancestry. This is an early-warning check, not OCCT verification. Some paths may appear TRACKED_TRANSFORM while still failing the real geometric rebuild because the witness was consumed or became ambiguous.

Windows PowerShell:

```powershell
py -3 .\tools\audit_persistent_references.py "C:\path\to\model.cadscene" --output "C:\path\to\reference_audit.json" --strict
```

Exit code 1 means a reference is statically blocked; 2 means malformed/unreadable input. The utility only emits a report, with atomic writes to avoid partial overwrites.

### 3. Wider regression coverage and rollback safeguards
- Native face-lineage regression now checks supported scale/mirror edges and faces, circular curvature, inverse scales, invalid-factor rollback, and independent tool sizes.
- Native curved-face regression covers scaled cylinder and torus support radii.
- Native interleaved-history test no longer incorrectly classifies positive uniform scaling as a forbidden ancestry stage; mirrors are tracked but unverified output still requires current-kernel matching.
- Portable regression checks verify public compatibility, transformation routing, known constraints, and physical OCCT reference calculations.
- Standalone saved-reference auditor tests cover source paths, missing witnesses, corrupt project input, CLI strict mode, duplicate IDs, and preserving the previous report on parse failure.

## Local verification evidence
- Python/source/OCP reference checks: **722 / 722 passed**.
- Portable C++ executables (Linux g++): **22 / 22 passed**.
- CMake: **130 existing native CTest registrations retained**.
- Full Windows MSVC+Qt+OpenCascade compilation: **not run in this environment**.
- Windows native CTests: **not run for this update**.
- Engineering GUI/manual acceptance: **P 0/20, A 0/20, D 0/20, NOT_RUN**.

**Important:** The native C++ additions are source-level and need a real Windows build. The independent OCP checks do not execute the Windows C++ source. No final-release certification is claimed.

## Major Windows build, clean test directory
Extract the **full source ZIP** to a new directory. Do not overwrite the verified Fix 2 baseline. In PowerShell:

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$zip = Join-Path $downloads 'CADWorkbench_v170_PersistentGeometry_MAJOR_PRELIMINARY_20261116_GitHub_Source.zip'
$target = Join-Path $downloads 'CADWorkbench_PersistentGeometry_MAJOR_Test'
if (!(Test-Path -LiteralPath $zip)) { throw 'Source ZIP is missing' }
if (Test-Path -LiteralPath $target) { throw 'Choose a fresh test folder; target exists' }
Expand-Archive -LiteralPath $zip -DestinationPath $target
Set-Location (Join-Path $target 'CADWorkbench_v170_PersistentGeometry_MAJOR_PRELIMINARY_20261116')
py -3 .\tools\preflight_major_build.py --root .
if ($LASTEXITCODE -ne 0) { throw 'Source preflight failed' }
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Target: 130/130 native CTests and 722/722 Python/reference checks. Audit exit code 3 may mean all automated checks passed but manual acceptance is still pending. Never hide failing tests or promote the snapshot to final release on that basis.

## Workstreams / remaining gaps
| Workstream | Historic estimate (not rescored) | Manual accepted |
|--|--:|--:|
| Parametric modeling | ~85% | 0/20 NOT_RUN |
| Assembly engineering | ~90% | 0/20 NOT_RUN |
| Drawings/sheet metal | ~81% | 0/20 NOT_RUN |

**Parametric outstanding:** arbitrary split/merge identity, surviving consumed fillet/chamfer edges, supported freeform face remapping, general sketch solving, and GUI acceptance.
**Assembly outstanding:** full geometry-driven mates, persisted hierarchy edits, DOF correctness, motion/collision end-to-end verification.
**Drawing/sheet outstanding:** auto-updating issued sheets with revision control, feature-linked GD&T, real multibend folding/relief geometry, manufacturing acceptance.

This increment improves the reference system and diagnostic workflow; it is not a claim that any major workstream has achieved 100%.

## Continuation notes
1. Preserve the new source snapshot and source manifest as an immutable candidate.
2. Review the exact MSVC/CTest failure output if the Windows build fails. Fix in a new snapshot; preserve this candidate.
3. For the next substantial modeling round, add current-kernel remapping for more complex split/rejoin histories, with explicit ambiguity handling rather than proximity guesses.
4. Run native and manual gates before any final release or clean installer.

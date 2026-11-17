# CADWorkbench v1.70 — single consolidated development delivery

Date: 2026-11-06. Base: `CADWorkbench_v170_Consolidated_Engineering_RC5_20261106.zip`.

## Honest release decision

**NOT VERIFIED AS FINAL V1.70.** This delivery incorporates two large, directly integrated engineering workflows, but does not complete the original 50-target production-level roadmap. The original requirement ledger still has **1 implemented for supported scope, 44 partial and 5 outstanding**. A Windows MSVC/Qt/OpenCascade build, its 130 native CTests and interactive GUI acceptance must pass. Do not relabel portable tests as native evidence, assume CAM is machine-ready, or replace a verified executable until those gates pass.

No intermediate ZIPs have been created or supplied in this delivery.

## A. Adaptive assembly motion inspection (roadmap 27, 28)

New portable `AdaptiveMotionScan` scheduler and its direct `SceneDocument::scanAssemblyMotionAdaptive` adapter. It keeps the committed assembly unchanged, includes every saved motion keyframe and interval endpoints, measures OpenCascade overlap and closest distance at additional times, prioritizes clearance-risk intervals, and respects a maximum 256-pose evaluation budget. A native Qt dialog can choose this mode or the legacy fixed-sample mode. Sampling step, warning threshold and budget are user-editable. Invalid measurements and evaluation errors leave the previous output unchanged; an explicit unresolved-interval indicator flags budget exhaustion.

**Crucial limitation:** This is a bounded, sampled motion scan, not mathematical continuous collision detection or safety clearance certification. Very narrow interference between sampled times can still be missed. All-pairs assembly scan still uses discrete sampling.

## B. Real geometry + BOM variant comparison (roadmap 12, 30)

The saved-configuration editor now compares stored design presets by rebuilding each one in a separate throwaway `SceneDocument`, running actual body validity checks, and computing a full assembly BOM and volume/mass totals. The current model is included as a baseline row. Unknown material density is clearly flagged rather than presented as a complete mass estimate. Validated reports can be atomically exported as spreadsheet-hardened CSV including target geometry revision, assembly geometry fingerprint and material/BOM fingerprint. Failed rebuild/export never alters the open document or previous output.

**Crucial limitation:** This covers saved configurations of one supported parametric source body and its regenerated dependents. It is not a simultaneous multi-part configuration matrix, procurement approval or lifecycle/PDM system. It has not been executed through the Windows Qt UI yet.

## C. Tests added

* Portable motion scheduler: 42 assertions, with narrow simulated collisions, keyframe retention, repeatability, budget exhaustion, invalid data, error rollback and interval-priority behavior.
* Native `cadcore_tests.cpp` additions (written, **not executed**): adaptive prismatic-joint motion checks; read-only and failure atomicity; stored configuration rebuild/BOM comparisons and CSV export with failure preservation.
* Source-integration contract: direct UI/API/CMake integration, retention of 130 native CTest registrations, and release-gate script.

### Evidence collected in the current environment

| Gate | Result |
|---|---|
| Python source/reference tests | 316 passed |
| GCC portable C++ with address/undefined-behavior sanitizers | 1,142 checks passed |
| Clang portable C++ with address/undefined-behavior sanitizers | 1,142 checks passed |
| Source ZIP and SHA-256 manifest | Generated and verified at package time |
| Native MSVC CAD application build and 130 CTests | Not executed |
| Qt interactive acceptance and actual save/reopen of new paths | Not executed |

## Windows acceptance (mandatory)

Open an MSVC developer PowerShell in the extracted project root, with Qt 6.5+, OpenCascade 8.0.1 and `C:\vcpkg` correctly provisioned:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Then manually exercise (1) assembly adaptive scan with a prismatic motion that crosses a collision between two ordinary sample times; verify the sample CSV and no document change, (2) saved configuration comparison, named formula changes, mass-density warning, BOM and CSV export, (3) close/reopen a model containing both joint tracks and named configurations, and (4) undo/redo and error rollback.

Any native failure blocks release. Keep the previous Windows-verified executable unchanged.

## Still blocking the original complete v1.70 scope

Unrestricted topological naming over arbitrary splits/merges, fully general cross-body feature editing, complete spline constraint/mate authoring, multi-level assemblies, production sheet metal and welding drawings, full standards-compliant GD&T, verified CAM 2.5D machining/stock simulation, machine-specific postprocessing, and end-to-end Windows automation remain incomplete. The user-facing target of 85% fully accepted requirements cannot be credibly claimed from portable tests or the currently documented implementation ledger.

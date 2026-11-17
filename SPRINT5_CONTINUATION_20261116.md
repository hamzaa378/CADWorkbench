# CADWorkbench v1.70 — Sprint 5: Connected motion safety / backend reliability

**Checkpoint: preliminary source candidate, 16 November 2026.**
**NO new MSVC/Qt/OpenCascade Windows build performed. NO native Windows CTests rerun.**
This source must not replace the last Windows-verified binary. Engineering/manual acceptance remains 0/20 across Parametric, Assembly and Drawing/Sheet Metal (0/60).

## Where we started

Complete Sprint 4 package, 578 archive members. Sprint 4 portable evidence: 830 Python tests and 33 independently compiled C++ executables (GCC and Clang). Historic Windows Sprint 2: CADWorkbench built and **129/130** native CTests passed; `cadcore_universal_feature_tree` failed at grouped unequal-size two-edge blend. Sprint 3 source fix has not been validated by an actual Windows native CTest. This sprint preserves that fix, all 130 native registrations and the MSVC `/bigobj` workaround.

## Real workflow changes (not isolated worksheet-only APIs)

1. `SceneDocument.cpp` now uses `MotionSweptClearance.h` in **buildLiveArticulatedEnvelopeCsv**. The world-space reference fixture is included in every same-time and swept pair comparison, although the exported row list remains moving components only. Previous exports could omit moving-vs-fixed fixture risk completely.
2. For fixed/prismatic-only assembly graphs, the export reports *between-pose* translational AABB contacts, closest segment and minimum gap. It detects possible collision even when the captured endpoint solids do not overlap. The extra CSV columns append after the original stable columns, before `coverage_note`. The old columns remain in order. AABB contact is broad phase only; actual CAD B-rep solid contact requires narrow-phase verification.
3. If ANY revolute joint exists, the new fields report `UNSUPPORTED_ROTATION`/`N/A`. If numeric bounds/rigid extent checks fail, they report `UNAVAILABLE_NUMERICS`/`N/A`. The valid sampled-pose report remains available. **Do not interpolate rotating body AABBs as if they translated linearly.** The 20-tracked-body limit now reserves 1 slot for the fixed fixture: at most 19 moving bodies.
4. `commitLiveArticulatedCoordinatesChecked` in **strict sampled-motion mode** additionally runs this continuous *translation-only* broad-phase path before it commits. A potential AABB encounter or insufficient clearance rejects the move without modifying geometry, saved joints or undo history. Rotating mechanisms retain the legacy sampled-motion-only behavior; no unsupported rotational certificate is claimed. No new UI controls.
5. Export API adds optional read-only structured results (`translationSweepSupported`, `closestSweptBoxGapMm`) for callers; they are committed only after the full successful export. Existing calls remain source compatible with defaults. Euclidean workspace gap now uses overflow-resistant `std::hypot`.

## New failure-injection regressions

- `tests/articulated_assembly_scene_v170_tests.cpp`: native Qt/OCCT tests added for fixed-fixture tracking, pure slider sweep support, rotation rejection label, invalid-output transactional preservation, and **hidden inter-pose fixture contact preventing strict commit**. These native tests have been authored but NOT EXECUTED (Windows build is deferred).
- `tests/test_sprint5_motion_workflow_contract.py`: 12 checks of workflow linkage, declarations, continuity/transaction semantics and preservation of 130 native registrations. These are source-level checks, not compiled native evidence.
- `tests/v170_sprint5_workcell_kernel_reference.py`: 5 real independent OCCT Python kernel tests. A translating 3x3x3 solid crosses a stationary 4x4x4 fixture; endpoints and the 3-interval interior samples have no material intersection, but the unsampled central pose has **27 mm³** solid intersection.
- `tools/audit_full_source.py` now guards against accidental removal of the strict workcell-safety wiring.
- `scripts/RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1` requires at least 847 successful Python/source/reference tests before accepting a full Windows audit.

## Verification evidence in this source archive

- `qa/sprint5_python_847.log` (actual count recorded by complete unittest run).
- `qa/sprint5_gcc_33.log` and `qa/sprint5_clang_33.log` (compiled portable executables).
- `qa/sprint5_source_audit.json` and `qa/sprint5_preflight.log`.
- `qa/SPRINT5_DELTA.json` comparison against Sprint 4 ZIP. No Sprint 4 file removed.
- `qa/SPRINT5_SHA256_MANIFEST.json` validates all packaged files except the manifest itself, the top-level ZIP and any checksum sidecar.

## Genuine engineering limits / risks

- AABB contact is a **candidate**, not proof of real OCCT-solid penetration. The strict gate may conservatively reject safe narrow-clearance assemblies. The translator assumes fixed-size world-axis bounding boxes and affine prismatic motion between keyframes; it explicitly marks unsupported changes. Rotational continuous collision detection and exact B-rep narrow-phase are still outstanding.
- The full Qt/OpenCascade integration is not locally compiled. All native integration improvements are **UNVERIFIED ON MSVC**. The existing grouped-blend CTest failure is also not yet resolved by verified native evidence.
- Splitting `SceneDocument.cpp`/`MainWindow.cpp` requires native build regression evidence before attempting significant refactoring. Frontend feature development remains frozen.
- Existing native test inventory is **130/130 registered**, not **130/130 passed** on this source. Manual engineering P/A/D acceptance remains not run.

## Windows verification later, after requested coding batches

From a fresh PowerShell terminal with the ZIP in Downloads and vcpkg already configured in `C:\vcpkg`:

```powershell
$downloads = Join-Path $env:USERPROFILE 'Downloads'
$archive = Join-Path $downloads 'CADWorkbench_v170_BackendSprint5_MotionSafety_COMBINED_PRELIMINARY_20261116.zip'
$dest = Join-Path $downloads 'CADWorkbench_Sprint5_Independent_Windows'
if (!(Test-Path -LiteralPath $archive)) { throw "ZIP missing: $archive" }
if (Test-Path -LiteralPath $dest) { throw 'Choose a new, unused destination.' }
Expand-Archive -LiteralPath $archive -DestinationPath $dest
$root = Join-Path $dest 'CADWorkbench_v170_BackendSprint5_MotionSafety_PRELIMINARY_20261116'
Set-Location -LiteralPath $root
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT5_DEFERRED_WINDOWS_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Retain all output under `build/v170_comprehensive_audit` and report the native CTest results and focused grouped-edge CTest replay. Audit exit 3 can be automated-pass/manual-acceptance-pending; this is not a production release.

## Next high-impact coding targets

1. Native OCCT **rotating** broad phase and exact B-rep narrow phase (current unsupported).
2. Link collision diagnostics to per-pair assembly interference report, and avoid recomputing geometry twice in strict commit by caching fully detached pose evidence.
3. Build a real persisted revision-sensitive drawing annotation freshness gate, ensuring source geometry changes cannot be silently acknowledged by a metadata refresh.
4. Improve stable face/edge lineage across split/merge, transaction recovery from native kernel exceptions and large-workflow geometry assertions.
5. Only after real Windows baseline validation: split oversized SceneDocument module carefully with CTest regressions.

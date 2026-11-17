# CADWorkbench v1.70 — 95% three-workstream engineering program

**Status 2026-11-16: Preliminary code increment, NOT a 95%-complete product.**

The three historical estimates P~85%, A~90%, D~81% were qualitative, not a scored engineering acceptance result. The new objective **95% for each of P/A/D** requires **19/20 accepted end-to-end engineering scenarios per workstream, with every critical scenario passed**. 60 current scenarios are `NOT_RUN` and none is automatically marked passed by source or native tests. The earlier default release gate is separate: two workstreams >=17/20 and third >=13/20, every critical scenario passed, Windows native build and CTests passed. The 95% target is stricter than the release gate.

## Baselines and actual evidence

- Last reported native Windows automated baseline: PersistentGeometry WinCTest47 source after face-normal test fix, **130/130 CTests and 725/725 Python/source tests passed** on user's Windows machine. The 60 engineering GUI cases remain unrun.
- Immediately preceding EdgeFinishing source: 734/734 source/reference tests and 19/19 portable C++ executables passed **locally**, but native Windows compiler/CTest was not run for that version.
- This new increment adds measured shop-coupon K-factor interpolation integrated into saved linked flat development editing. New source/reference and portable results must be read from current test logs, not inferred from the older builds. No manufacturing approval or material-forming certification is claimed.

## Implemented this increment: real shop coupon calibrated sheet development

- `src/core/BendCouponCalibration.h` implements independent measured one-bend coupon inference (`measured blank - sum of straight tangent lengths`, normalized by bend angle and sheet thickness), bounded by K in [0,1].
- For each bend of an existing saved folded-sheet source, compatible measured coupons within ±15 degrees are selected and interpolation is performed strictly within observed R/t range. Out-of-range R/t, unsupported bend angles, conflicting repeat measurements (>0.05 K), nonsensical dimensions and nonfinite values fail closed.
- `Edit > Edit linked flat blank K-factors and relief > Calculate K-factors from measured shop coupons...` opens a coupon entry dialog and pre-populates per-bend K-factors. It does **not** modify the model until the main dialog is accepted. The existing edit method performs atomic folded/flat regeneration.
- The UI does not store coupon provenance permanently or claim a material qualification; qualified material, tools, grain, springback and shop inspection must still be checked independently.
- `tests/bend_coupon_calibration_portable_tests.cpp` adds 28 compiled C++ assertions, and `tests/v170_shop_coupon_integration_contract.py` checks UI connection. These are source-level and portable tests until the user's Windows build verifies the native UI.

## Main obstacles on the route to 95% (technical and acceptance)

### Parametric modeling — priority and blocked checklist

| Risk | Current limitation | End-to-end scenario | Engineering work required |
|---|---|---|---|
| P1: Persistent topological naming | Witness-and-lineage references safely reject ambiguity but cannot always follow arbitrary split/merge/fillet/chamfer combinations | P12, P13, P18 **critical** | Subshape provenance graph, transform history, explicit user-approved rebind, ambiguity/rollback tests, save/reopen and stress |
| P2: Constraint solver completeness | Nontrivial sketch constraint families and under/overconstrained cycle behavior need scenario-level proof | P01, P02 **critical** | Complete constraint graph, real-time diagnostics, degeneracy handling, solver numerical conditioning |
| P3: Feature DAG completeness | Reorder/suppress/retarget complex interleaved operations remain high risk | P09, P10, P14 **critical**, P15-P17 | Transactional rebuild from dependency snapshots; large-history tests |
| P4: Advanced real parts | Complex guided loft/sweep, 3D face edits, STEP/IGES regeneration and performance | P07, P08, P19, P20 | NURBS guide continuity controls, reliable topology update, import/export round-trips |

### Assembly engineering

| Risk | Current limitation | End-to-end scenario | Engineering work required |
|---|---|---|---|
| A1: Hierarchical assembly ownership | `assemblyPath` and subtree-move support exist but nested assembly identity, local frames and configurations need full workflows | A01-A04, A03 **critical** | Persistent subassembly instances, local-to-world transform engine, atomic reparent/reopen |
| A2: Geometry-linked mates | Face/cylinder selection prefills constraints, but full persistent source-face reattachment after model edits is incomplete | A05, A12 **critical** | Persist witness ownership, resolve/rebind on edit, diagnose contradictory or singular mates |
| A3: General articulated solver | Closed-loop rigid/articulated tests exist but interactive 6-DOF motion and singular configurations need real-scene verification | A07, A08, A11 **critical** | Sparse global constraints, robust joint coordinates, interactive drag, loop closure |
| A4: Clearance through motion | Snapshot interference and motion audits do not certify full swept collision or tooling access | A09-A10, A13-A16 | Broad-phase + exact narrow-phase, adaptive sampling, collision certificates, performance |
| A5: Exchange and edit usability | Exploded views, hierarchical BOM and STEP assembly round-trip need acceptance | A17 **critical**, A18-A20 | Stable placement/configuration and verified interoperability |

### Drawings and sheet metal

| Risk | Current limitation | End-to-end scenario | Engineering work required |
|---|---|---|---|
| D1: Automatic associative drawing update | Saved source-linked dimensions and preview batches exist, but not all sheet entities regenerate automatically after arbitrary upstream edits | D04, D08, D09 **critical** | Unified reference resolver, regeneration transaction, visible stale/rebind states |
| D2: Technical drawing standards | Datum/GD&T review is narrower than feature-controlled GD&T validation; section/detail views require complete edits | D02-D07, D10-D12 | True 3D geometric association, tolerance semantics, standards-aware labels and DXF/PDF export QA |
| D3: Manufacturing-grade sheet metal | Folded/flat links, per-bend K, bend relief DXF, and shop preflights exist, but complex connected flanges and corner relief geometry are incomplete | D13-D16, D17 **critical**, D18-D19 | General flange topology, bend-table provenance, collision/overlap checks, unfold/refold round trips |
| D4: Shop handoff integrity | STEP, BOM, DXF, bend schedule, holes and change-control package exist but require live multi-part and external shop verification | D20 | Real part/manufacturing fixtures and certified release workflow |

## Approximate engineering effort to close all three 95% gates

These are **conditional staffing estimates**, not promises or a schedule for ChatGPT to deliver work later. They assume the existing Qt 6 / OCCT 8 project remains usable and changes are reviewed by experienced CAD/C++ engineers with real sample assemblies and manufactured parts.

| Work package | Rough full-time engineering effort | Main uncertainty |
|---|---:|---|
| General persistent topology, sketch and dependency regeneration | 12–25 engineer-weeks | Unstable subshape identity after complex OCCT edits |
| Hierarchical assembly, geometry mates, motion/clearance | 15–30 engineer-weeks | Closed loops, solver numerical conditioning and broad-phase performance |
| Fully linked drawing/GD&T and multi-flange sheet metal | 15–30 engineer-weeks | Standards semantics and manufacturing geometry complexity |
| Native/GUI 60-scenario verification, fixtures, regression, packaging | 8–15 engineer-weeks | Severity of defects found during real acceptance |
| **Total, not including all rework contingencies** | **50–100 engineer-weeks** | Scope and bug-discovery variation |

With **3 domain-focused engineers plus QA** working in parallel on independent branches, the rough planning envelope is **5–10 months** including integration; with one engineer it could require **12–24+ months**. These are indicative project-scoping ranges, not guaranteed dates. Achieving 95% on a paper checklist is not equivalent to production certification; the representative parts and failure cases selected for those 60 scenarios materially affect the meaning of 95%.

## Sequencing and release controls

1. **Native verification first:** test the latest source on Windows (full CMake/MSVC build, all 130 registered CTests, complete Python/reference suite). Preserve last passing ZIP and binaries separately.
2. **Critical path before cosmetic work:** P12/P13/P14, A07/A08/A11/A12, D04/D08/D09/D17. Track failing cases against stable reference parts with save/reopen/undo.
3. **Representative industrial fixtures:** bracket with changing holes/edge finishing; gearbox subassembly with nested joints and clearances; multi-flange enclosure with drawing sheets and DXF package.
4. **Manual evidence ledger:** record scenario, steps, observed result, screenshot/log/model, and evidence SHA. Use `python tools/verify_95_acceptance.py --evidence-root . --report build/acceptance_95.json`; it exits 3 until 19/20 per stream and all critical pass. Never mark any scenario passed from source tests alone.
5. **Only after gates:** create separate release-grade clean installer, source ZIP, upgrade ZIP, checksums and rollback guide. No source-only release should be presented as a tested Windows binary.

# CADWorkbench v1.70 — Last development checkpoint before Windows tests

## Scope and status

This is the **last planned source-development ZIP** in the current sequence. It is **not** an approved Release build. Previous known-good snapshots are unchanged; this is a new, separate archive.

## Three delivered changes

1. **Parametric modeling:** The atomic coordinated dimension+optional-feature preview now measures **before and after** B-rep volumes, face counts and edge counts for every changed solid, identifying regenerated descendants and topology-count changes. Every staged solid must have valid geometry and strictly positive measured volumes. The entire preview operates on a detached document; the live model remains unchanged until user acceptance. Changed counts are a risk indicator, **not** proof of persistent face identities.
2. **Articulated assembly:** An affine interval analysis computes each independent drive root's feasible target corridor after following chained ratios and offsets through the full coupling DAG. It includes reverse ratios, branching, follower limits, legacy unbounded drives, cycle rejection and failed-output atomicity. The interactive multi-drive editor displays joint-limit corridors alongside its existing geometry preview. The result does **not** certify continuous collision freedom, dynamic forces, or persist gear constraints.
3. **Sheet metal and manufacturing:** Stock-layout jobs from the interactive exporter now explicitly require matching sheet gauges and material grade assignments across all selected linked flat blanks. One unassigned grade cannot silently be mixed with an assigned grade. All-unassigned grades remain explicitly flagged in the stock manifest for operator review; saved source files are unchanged. Existing API callers can opt into the compatibility guard with `StockLayoutSettings::requireCompatibleGaugeAndMaterial`. The manufacturing export still stages its complete batch before publication. This does not qualify the process for cutting or bending.

## Baseline native failures retained as requested

The most recent *user-run* Windows Release build before this checkpoint produced `CADWorkbench.exe` and recorded **128/130 native CTests passing**. The two failing cases were `cadcore_v170_assembly_mate_document` (clearance-report assertion) and `cadcore_folded_sheet_metal` (segmentation fault). Neither case was deleted, ignored, disabled or labeled as passing. The source here has **not** been built or tested on Windows.

## Verification procedure

In a **fresh extracted folder**, from a Visual Studio Developer PowerShell with `C:\vcpkg` installed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_FINAL_PRETEST.ps1 -VcpkgRoot C:\vcpkg
```

The script checks source integrity, builds the complete Windows Release application, runs the **full 130-test CTest inventory**, reports all recorded failures from the fresh JUnit, and exits nonzero if anything fails. It does not exempt the two deferred cases. For the strict release gate, also run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

That second command also evaluates the 60-scenario acceptance scorecard. A missing or unscored scorecard must not be reported as a functional failure, or as a pass.

## Manual validation after native build

- Parametric: edit a dimension and suppress a feature on two bodies in one transaction; check before/after B-rep measurements, undo, redo, save and reopen; verify linked descendants and face/edge reference reattachment.
- Assembly: use chained and reverse ratio couplings, move leaders to travel limits, inspect feasible ranges, sample motion, and compare against actual assembly clearance; test undo/redo and reopen.
- Sheet metal: export a two-part job with identical gauge/material; verify hashes and stock reports; reject mismatched gauges and assigned-vs-unassigned grades; export linked DXF/SVG/CSV and inspect actual neutral bends and reliefs.
- Regression: reproduce `cadcore_v170_assembly_mate_document` and `cadcore_folded_sheet_metal` failures and preserve raw CTest logs for the subsequent correction phase.

## Honest completion note

Previously reported **85% parametric / 90% assembly / 81% sheet-metal** were provisional engineering estimates, not scored acceptance. This checkpoint does not increase those estimates. No final release may be claimed until the full Windows Release build, all 130 native CTests, and the agreed formal acceptance and interactive smoke checks have passed.

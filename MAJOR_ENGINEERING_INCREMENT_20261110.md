# CADWorkbench v1.70 — Major engineering increment (preliminary source)

**Status:** source-only engineering preview. The last fully Windows-verified baseline is **Fix 2**, with 130/130 native CTests and 546 Python/source tests reported by the Windows audit. This increment has **not** received a Windows MSVC/Qt build or end-to-end GUI acceptance. Do not distribute it as a production release.

## What was implemented

### A. Shape-independent and associative parametric features

- **Cylinder through all arbitrary axis** is a saved, editable Shape Feature Tree operation. It contains world-space entry x/y/z, axis x/y/z and radius. There is deliberately **no stored depth**.
- Every feature-graph regeneration obtains the **current input solid's OCCT bounding box**, computes its full projected extent along the arbitrary normalized axis and extends the cutter beyond both ends. Thus changing an earlier scale, source dimension or transform recomputes the through-hole. It supports valid arbitrary B-rep solids, not just box primitives.
- A valid geometric intersection and positive material removal are mandatory; missing cuts, invalid axes, invalid geometry and invalid downstream linked bodies are rejected without replacing the original feature output. It uses the existing atomic feature-tree/linked-body regeneration and persistent scene format.
- Existing finite-depth `Cylinder cut arbitrary axis` is retained unchanged. Planar-face and directed direct cutting remain available.
- Native regression assertions were added **inside the existing** `cadcore_associative_features_v159` test. They cover two upstream dimension changes, volume preservation, a derived Boolean child, failure rollback and scene save/reopen. No 131st registered test was added.

### B. UI discoverability

- The general feature-tree **Add feature** dialog is now searchable with case-insensitive substring matching, instant filtering, double-click selection and no acceptance when no matching command exists.
- Search for **through all**, **pocket**, **fillet**, **loft**, **pattern**, or an axis to narrow the many available operations. The existing Ctrl+K command palette stays in place; this filters operations within the feature editor itself.

### C. Assembly engineering

- Read-only **rigid mate structure audit** now reports, for each graph bridge connection, the occurrence names that lose connectivity to the fixed occurrence if that specific connection is removed. The report responds to a different fixed occurrence and handles parallel mate links.
- This is a structural connectivity result, **not** a claim of kinematic redundancy, collision freedom, 6-DOF solution or a pass/fail manufacturing standard.

### D. Sheet-metal engineering

- The **folded-sheet bend audit** now outputs exact flat-blank neutral-axis *straight flange intervals*, in addition to bend center stations and tangent boundaries. The intervals derive from the same per-bend allowance and per-flange lengths used to calculate the developed length.
- This is an inspectable shop-planning reference, **not** a tooling, springback, minimum flange, relief or bending-order certification.

## Independent verification in this environment

- Python/source and independent OpenCascade reference inventory: **562/562 passed**.
- **14 portable C++ executables passed**, including separately compiled assembly bridge impact and sheet flange interval checks.
- Existing native Windows CTest registration count: **130**; no tests removed or skipped.
- Independent OpenCascade references include through-hole regeneration at three changing stock dimensions, rotated-stock arbitrary-axis cut, and a missed-cut no-op.
- SHA-256 source integrity manifests regenerated and checked against every shipped source file.
- **Not executed here:** native MSVC/Qt build, 130 Windows CTests for this increment, actual GUI operation, 60 manual engineering scenarios.

## Windows test instructions

Extract the ZIP into a **new** empty directory, not over the confirmed Fix 2 baseline. In PowerShell, from the extracted root containing `CMakeLists.txt`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot 'C:\vcpkg' -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

The expected result **if the new code builds and passes** is 130/130 native and 562/562 Python checks, followed by exit code `3` due to remaining manual acceptance. Treat exit `1`, a missing native test or any geometry discrepancy as an automated failure. Preserve and share the complete output in that case.

## GUI exercise (later, when manual review resumes)

1. Select one solid in the body tree. Open **General feature tree for any body**.
2. Click **Add feature**, type `through all` into the new search field, and choose **Cylinder through all arbitrary axis**.
3. Enter x/y/z at a bore location, axis_x/y/z for drill direction and radius. The axis need not be parallel to a world axis.
4. Save the feature graph; verify the bore removes material. Reopen, edit an upstream box scale or base dimension, and ensure the bore reaches both far surfaces after regeneration.
5. Add a linked Boolean descendant, repeat the edit and verify its geometry follows the source. Attempt a cut outside the stock; the rejected edit must leave the last valid body and child unchanged.
6. In the structural mate audit, review new fixed-frame isolation entries; in the folded-sheet bend audit, review the flat flange intervals.

## Remaining work and release control

| Workstream | Historical unverified implementation estimate | GUI engineering acceptance | Significant gaps for a truthful 100% claim |
|---|---:|---:|---|
| Parametric modeling | ~85% | 0/20 | General persistent topology through arbitrary splits/merges, robust sketch solver and nontrivial multistage regeneration, feature edit GUI and persistence verification |
| Assembly engineering | ~90% | 0/20 | End-to-end hierarchical and articulated assembly constraints, motion/collision and clearance fidelity, save/reopen and UI acceptance |
| Drawings / sheet metal | ~81% | 0/20 | Associative drawings and GD&T lifecycle, general multi-flange fold/unfold validation, shop-specific handling, acceptance |

These are historical estimates, **not updated verification scores**. Shipping the preliminary source archive does **not** pass the final v1.70 gate. Final release requires Windows native/GUI verification, correction of any regression, all critical scenarios and 85%/85%/65% acceptance (in some order). No manual acceptance scenarios have been marked PASS by this work.

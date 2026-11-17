# CADWorkbench v1.64 — Manufacturing and section engineering workshop

**Release type:** source candidate based on user-confirmed v1.63 Windows Release 83/83 passing baseline. v1.64 must be built and tested independently on Windows before it can be called verified.

## Where to find the new tools

Open **Inspect → Manufacturing workshop: sections, machining, fits, cylinders...**. Select at most one body in the model browser before opening the dialog if you want model-based dimensions, assigned density, B-rep health, or cylindrical-surface inventory. All numerical calculators still work when nothing is selected.

## Implemented workflows

1. **Section geometry (eight profile families):** rectangle, solid circle, circular tube, rectangular box tube, symmetric I beam, C channel, T profile, L angle. Dimensions in millimetres. Supports cross-sectional area, centroid, centroidal second moments Ixx/Iyy, product Ixy, polar *area* moment, elastic bending section moduli, radii of gyration, and optional material mass per metre. Sharp-corner idealization; no fillets, standard section catalog, torsion warping, local buckling, welding, or certified section-grade data. The polar *area* moment is **not** the torsional constant for open sections.
2. **Process estimates (eight operation templates):** drilling, reaming, tapping, counterboring, countersinking, slot milling, face milling, and turning. Calculates RPM from cutting speed and tool diameter, machine RPM clipping, feed from feed per revolution or per tooth, nominal cutting time, nominal cutter-sweep envelope, approximate volume rate, optional chip mass, optional specific-energy power and machine power warning. These are planning estimates; they are NOT machine toolpaths or NC programs. Countersink/turning removal volumes are approximate envelopes, not actual stock subtraction.
3. **Signed tolerance stack:** editable nominal dimension, upper/lower deviation and plus/minus direction. Displays deterministic worst-case minimum/maximum and an independent normal-process RSS 3σ estimate separately. RSS is **not** an engineering guarantee without verified process distribution, independence, capability and correlation.
4. **Cylindrical-surface inventory from OCCT B-rep:** lists a selected solid's cylindrical face index, diameter, axial parameter span, axis reference center, direction, face area and face orientation. Reports B-rep validity. Allows explicitly copying a candidate cylinder diameter/span into the machining calculator. **No automatic hole identification**: shafts, bosses, bores and cylindrical trims can all appear. Face indices are not persistent topology IDs.
5. **Production readiness snapshot:** part number, revision, assigned material, density, body type, B-rep check, modeled volume, homogeneous-density mass and approximate bounding-block stock utilization. Warns about missing release metadata. Does not approve manufacturing.
6. **Worksheet export:** tab-specific UTF-8 TXT reports, CSV cylindrical inventory, and clipboard copy. No project mutation, geometry modification, or executable machine-control output.

## Sample workflow A: bracket section

1. Create a rectangular extrusion in CADWorkbench and select it.
2. Open manufacturing workshop; choose rectangle and set width `40 mm`, height `20 mm`, steel density `7850 kg/m³`.
3. Calculate. Expected area `800 mm²`; Ixx = `26,666.6667 mm⁴`; Iyy = `106,666.6667 mm⁴`; estimated mass `6.28 kg/m`.
4. Do **not** equate this idealized section to a finished bracket with holes and transitions.

## Sample workflow B: a modeled cylindrical surface

1. Model a solid containing a cylindrical cut. Select the body in the model browser.
2. Under **CAD cylinder inventory**, inspect candidate diameter and axis coordinates.
3. Explicitly select a face and click to use it in **Machining estimates**.
4. Confirm whether the face is an actual accessible bore. Set cutting speed, feed, actual travel depth, passes, tooling limits and power data. Never use this estimate to control machinery directly.

## Sample workflow C: fit accumulation

1. Set Housing `25 mm`, deviations `−0.04/+0.04`, direction `+`.
2. Set Spacer `8 mm`, deviations `−0.02/+0.02`, direction `−`.
3. Expected nominal result `17 mm`, worst-case limits `16.94–17.06 mm`, RSS half-band `≈0.04472 mm` with the stated independent 3σ assumption.
4. The actual allowable clearance or interference must be set from assembly function and tolerance standards.

## Tests and acceptance

- Portable: `tests/workshop_engineering_v164_tests.cpp` runs over 1,000 checks under GCC and Clang, and sanitizer verification where available. Registered as three native CTests to isolate sections, machining and tolerances.
- OCCT integration: `tests/workshop_geometry_v164_tests.cpp` tests box inspection, a Boolean-drilled box, cylindrical geometry, and invalid-input rollback. Registered as a fourth native CTest.
- Source preflight: `tests/v164_workshop_preflight.py` checks integration and expected test inventory.
- Windows Release build and **all 87 native tests** must pass. Run:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 87
```

## What is *not* delivered

This increment does **not** implement true CAM stock-removal simulation, machine-specific post-processors, NC verification, a standardized section database, standard fit designation tables, automatic through-hole/threads detection, geometry-linked persistent manufacturing dimensions, FEA, or the remaining general feature-graph and stable-topology roadmap. All v1.63 CAD operations are preserved.

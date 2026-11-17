# CADWorkbench v1.70: shape-independent / associative workstream increment

**Status: PRELIMINARY SOURCE ONLY, Windows compilation and GUI acceptance PENDING for this increment.**

## Implemented now

1. `SceneDocument::prepareHoleOnPlanarFace`: world-space drill-center coordinates on any selected *planar face of a solid*. Uses the face's oriented inward normal instead of requiring world -Z. Rejects stale face references, off-plane centers, surface-only inputs and invalid dimensions. The existing ISO 273 clearance-hole wizard offers this mode in Face-selection mode.
2. `DirectionalHoleSpan.h`: computes a finite, axis-independent through-all cutter range from the body's bounding box projections. This also repairs the old world -Z through-all behavior when the entry point is inside a solid. Counterbore and countersink head cutters stay anchored at the entry plane even when a through-all cutter begins outside the model.
3. `commitHole`: after a successful preview, dependent linked Booleans/sketch bodies regenerate **before any checkpoint or commit**. Regeneration failures do not mutate the original part/document. Direct edits on read-only derived bodies are rejected rather than silently breaking the dependency graph.
4. `ShapeFeatureTree`: adds an **editable, persistent** `Cylinder cut arbitrary axis` feature, with x/y/z, axis_x/y/z, radius, and depth parameters, input validation, native OCCT Boolean execution and integration into the existing feature-history editor. Revisions of dimensions/axis use the established rollback and dependency-rebuild pipeline. This is the associative way to create an arbitrary-axis **blind** cylindrical cut; a full through-all option is not yet represented in the node.
5. Regression additions: native checks inserted into the existing `cadcore_associative_features_v159` CTest target, 12 portable C++ suites (one new directional span suite), 555 Python/source and independent OCCT reference tests (9 added), and **130 existing CTests still registered**.

## Known limitations

- The ISO 273 wizard's *direct* hole is not itself parametrically editable. Use the Shape Feature Tree arbitrary-axis cylinder-cut operation for persistent axis/radius/depth editing.
- Directional through-all on general solids is direct and uses a bounding-box spanning cutter; it is not a geometric clearance or collision guarantee. Its cut must intersect the solid and is verified in the preview.
- Face-normal mode currently supports planar faces, not arbitrary curved supports or non-manifold bodies. Points must lie on the selected supporting plane (0.02 mm tolerance).
- The feature-tree arbitrary-axis cut uses a user-specified depth; there is not yet an associative through-all extent that recalculates as the solid thickness changes.
- Persistent topology is not universal. Arbitrary changes to splitting/merging face/edge graphs remain a known risk requiring topology witness validation and full GUI acceptance.
- No broad guarantee of real-world manufacturing correctness, tooling access, ISO tolerances, or standards certification is implied.

## Engineering workstreams: current evidence-based status

| Workstream | Historical unverified implementation estimate | Verified GUI acceptance | Still required for 100% claim |
|---|---:|---:|---|
| Parametric modeling | ~85% | 0/20 | Fully general sketch constraints; arbitrary-topology persistent names; associative loft/sweep and multiple-step edits; full editing GUI workflows; regeneration and persistence cases |
| Assembly engineering | ~90% | 0/20 | Hierarchy/rigid and articulated mate solving across general assemblies, motion-limit and clearance integrity, cycle diagnostics, saved-load equivalence, all acceptance cases |
| Drawings & sheet metal | ~81% | 0/20 | Fully associative drawing dimensions/GD&T workflow, updated views after source edits, general multi-flange/relief/fold-unfold, manufactured-geometry validation, all acceptance cases |

The above percentages are inherited, **not updated or validated completion scores**. All three areas cannot truthfully be declared 100% until the behaviors are implemented, compiled, and individually accepted. The established release threshold (two >=85% verified and third >=65%, all critical tests passing, face machining regression fixed) has not been met by manual engineering evidence.

## Verification performed here (Linux development environment)

- `python -m unittest discover -s tests -p '*.py' -q`: **555 passed**.
- `python tools/run_portable_regressions.py`: **12 executable suites passed**.
- `python tools/verify_130_tests.py`: **130 native CTests registered**.
- Independent OpenCascade B-rep: arbitrary-axis through-cut volume, edit-depth comparison, linked-fuse geometry reference and missed-cut no-op passed.
- **NOT run here:** Windows MSVC build, native Windows CTest execution, Qt GUI interaction, and 60 manual acceptance scenarios. The previously reported 130/130 Windows result applies to the input Fix 2 baseline only.

## Windows commands

From PowerShell in the extracted directory:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_COMPREHENSIVE_AUDIT.ps1 -VcpkgRoot $env:VCPKG_ROOT -SetupPythonEnv -Parallel 4
Write-Host "Audit exit code: $LASTEXITCODE"
```

Expected automated passing result: 130/130 native tests and 555/555 Python tests. An exit code **3** is correct only if automated tests passed and manual acceptance remains pending. Stop and report any other failure, especially native CTests added to the `associative_features_v159_tests.cpp` executable.

## GUI usage

- **Direct arbitrary-solid face-normal clearance hole:** Switch to Face selection, choose one planar face of a solid, use Create > ISO 273 metric fastener clearance hole; set Modeling method to Direct through-hole and Drilling axis to Inward normal of selected planar face; review world-space X/Y/Z, diameter and preview. Note this is direct editing, though linked descendants regenerate.
- **Editable arbitrary-axis cylinder cut:** Select an ordinary solid, open the Shape Feature Tree editor, add `Cylinder cut arbitrary axis`. Enter x/y/z, axis direction, radius and depth. Save the tree. Reopen to edit dimensions and check linked Boolean descendants update. Avoid zero-length axes. A failed cut must leave geometry unchanged.
- **Regressions:** Native `cadcore_associative_features_v159` tests include planar face normal, immutable failure cases, update and undo/redo, and arbitrary-axis history edits.

**Do not publish this snapshot as a final or manufacturing-certified release.**

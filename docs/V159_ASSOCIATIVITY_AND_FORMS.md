# CADWorkbench v1.59: compiler repair, associative Boolean bodies, sketch constraints and parametric forms

## Baseline and validation status

Based on the combined v1.58 source snapshot and user-provided Windows MSVC diagnostic from 4 November 2026. The previously user-confirmed fully passing native build remains v1.56 (74/74). The current v1.58 diagnostic shows `cadcore.lib` and the desktop application compiling, but the **original `cadcore_tests.cpp` fixture** fails with C2440 at its legacy extrusion-plane test cases. In this v1.59 candidate, both test case structs (`PlaneCase` and `ThroughAllCase`) now use `cad::ExtrusionSketchPlane`. The newer datum-plane `cad::SketchPlane` is retained unmodified. This source package has **77 native test registrations**, including an additional associative-features test, but **no Windows Release compilation or 77/77 pass has been run in this environment**.

## Newly added modeling capabilities

- **Stable planar face hints:** optional persistent centroid, outward normal and surface-area signatures on face-based modifier nodes. The feature resolver matches the intended planar face after face traversal is reordered when the shape remains close enough; uncertain or ambiguous matches fail safely. Unanchored older operations still use their saved `face_index`.
- **Stable edge matching API:** capture and re-resolve a selected edge by centroid and length. Not wired into every edge-based chamfer/fillet tool and not a general persistent topological naming engine.
- **Editable additive forms:** parametric `Revolve annulus Z`, `Sweep circle path`, and `Loft rectangles` nodes, using OCC revolve, pipe shell and thru-sections. Each accepts bounded numerical parameters, generates an additive solid and is fused to the node's upstream solid. These are **template-based additive operations**, not yet unrestricted sketch-driven revolve, sweep and loft.
- **Linked multi-body Boolean bodies:** create non-destructive, source-linked union, cut and intersection bodies; preserve source solids; allow subsequent derived results as new dependencies, in creation-order DAG. Source edits in supported feature-tree, sketch-extrusion, named-parameter/suppression, and transform methods stage all downstream linked results; a failed regeneration rolls back the source edit. Save/reopen captures identities, source geometry tokens, operations, and validates dependency ordering. Linked derived bodies cannot be directly transformed or given independent feature stacks in this version. A manual refresh command covers some other source-edit paths.
- **Interactive sketch relations:** toolbar constraint modes for horizontal, vertical, segment length, parallel, perpendicular, point-to-point distance, coincident vertices, fix and release. Length and distance use editable numerical inputs. Sketch solver validates candidates and constrained dragging, rejecting changes that violate profile validity. The underlying solver has more constraint types, but **not every one is exposed in this UI**.
- **Editor:** additional associative Boolean menu commands, linked-body browser details, and new feature-tree operation entries with numeric fields.

## Critical limitations and follow-up

1. Stable geometric hints can fail after substantial upstream topology changes; full face/edge naming/history remapping remains an open problem.
2. Cross-body automatic regeneration is integrated for documented mutation paths only. Legacy direct editing workflows not wired to the propagation transaction may need **Refresh linked Boolean bodies**. Derived dependencies are limited to Boolean links.
3. Additive revolve/sweep/loft geometry uses bounded templates rather than arbitrary selected profiles/paths/sections.
4. Sketch constraint handling is a first integrated editor, not a complete industrial fully constrained 2D solver UX.
5. Production Windows GUI smoke tests, visual workflows, complete native build, and all 77 CTests must be carried out on Windows before merging or calling this release verified.

## Windows build and verification

Run from an extracted source folder (not inside the ZIP):

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
PowerShell.exe -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
```

For faster issue isolation, build the native tests individually:

```powershell
cmake --build .\build --config Release --target cadcore_tests associative_features_v159_tests
ctest --test-dir .\build -C Release --output-on-failure -R "cadcore_associative_features_v159|cadcore_geometry_and_persistence"
```

If the build fails, capture the **first** MSVC `error C...` or linker `LNK...` message (not the final MSBuild summary). Source-contract checks are available via:

```powershell
python .\tests\v158_source_preflight.py
python .\tests\v159_source_preflight.py
```

These Python checks are supplementary, not substitutes for the actual native build and GUI validation. Keep the previous passing v1.56 checkout and a copy of the v1.58 ZIP until acceptance.

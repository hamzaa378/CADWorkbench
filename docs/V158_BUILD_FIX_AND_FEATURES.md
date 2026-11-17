# CADWorkbench v1.58 — combined compiler repair and CAD feature expansion

**Status:** Source-level validation candidate for Windows x64 / Visual Studio 2026 / Qt 6.11 / Open CASCADE 8.0.1. **NOT YET native-compiled in this environment.** The user-confirmed baseline is v1.56 (74/74 CTests). The earlier v1.57 candidate did **not** compile; this release repairs the reported first blocker.

## Repair of the supplied v1.57 MSVC error log

`src/core/SketchProfile.h` introduced a six-coordinate **struct `cad::SketchPlane`** describing arbitrary datum planes. `src/core/SceneDocument.h` also declared an unrelated **enum class `cad::SketchPlane`** with XY/XZ/YZ/Custom settings for an older extrusion command. C2011, C2079, C2027, C2065 and most Qt `QList<Part>::append` errors in the provided log follow from this name collision.

- The legacy plane-choice enum is now `cad::ExtrusionSketchPlane`, everywhere in the core, UI, and **the original CAD regression tests**.
- The modern `cad::SketchPlane` datum-plane struct remains unchanged for plane-aware curved sketch geometry, serialization, and API compatibility.
- A source preflight checks both declarations and older test references to prevent the same error from re-entering the release.

## New sketch canvas tools

- **Rectangle**: two-corner closed planar polygon, editable vertices.
- **Rounded slot**: centerline endpoints + radius, represented with two circular arc edges and two straight sides (uses existing OCCT curved extrusion pipeline).
- **True polygon offset (restricted)**: positive or negative signed distance for strictly convex closed straight-edge polygons, with geometric line intersections and trial extrusion validation before commit.
- **Horizontal / vertical edge alignment**: aligns the hovered straight edge and records the matching constraint.
- **Straighten curved edge**: replaces a hovered arc or Bézier edge by a line, with control-handle cleanup.
- Sketch undo and cancellation, keyboard F for fit, point/grid snapping and status guidance.

**Not claimed:** arbitrary trim/split of overlapping curves, offsets of concave/curved profiles, an automatic general-constraint solver canvas, or direct multi-loop outer sketches.

## New general feature-graph operations

All new operations are authored through **Edit → General feature tree for any body** and saved with the existing per-body dependency links, suppression flags, transactional rebuild and undo/redo:

| Category | New operations | Caveat |
|---|---|---|
| Symmetry | Mirror XY, XZ, YZ | Mirrors about world origin planes; user-selectable arbitrary planes not yet exposed |
| Repetition | Linear pattern X/Y/Z; circular pattern Z | Produces a compound of valid solid instances, not an automatically fused solid |
| Holes | Cylinder cut X/Y in addition to Z | World-axis placements in millimetres |
| Face features | Rectangular boss/pocket on a selected planar face | Face indices, not stable topological names; face-centric local frame |
| Thin-walled part | Open-face shell | OCCT kernel supports only geometries/thicknesses that validate |
| Molding | Draft face relative to a Z neutral plane | Face index and kernel success remain prerequisites |
| Speed | Sequential append reuse | Only unchanged, current histories with a new sequential feature use the cached output; edits and branches rebuild entirely |

The existing translation, rotation, uniform scale, fillet, chamfer, face-normal boss/pocket and branch Boolean logic are preserved. Pattern count capped at 24 and feature-tree nodes capped at 40.

## Desktop UI improvements

- Search the **model browser** with a filter entry above the tree. A solid's stored feature nodes are expandable with their type, suppressed/current state, source hints, and double-click edit access.
- The properties inspector is scrollable and reports the modifier-stack status.
- A new **Validate staged geometry (dry run)** button evaluates the proposed graph without modifying the current part.
- Reorder/removal dependency problems appear in the feature editor before Save. Qt popup, dock and control styling received restrained refinements.

## Regression and acceptance plan

One new C++ test target, `cadcore_feature_upgrade_v158`, is registered in CMake, raising the expected total from v1.57's 75 to **76**. It covers: three-dimensional linear and circular patterns, mirroring, X/Y bores, face rectangular pocket/boss, invalid parameter rollback, append reuse, undo/redo, and project save/reopen. These are **native tests pending a Windows build**, not confirmed passing results.

Preflight / source checks to run on any Python 3.11+ installation:

```powershell
python .\tests\v158_source_preflight.py
python .\tests\dependency_sketch_v157_contract.py
python .\tests\sketch_feature_v156_contract.py
python .\tests\kernel_contract_smoke.py
```

For the actual acceptance test, start PowerShell **inside the extracted source folder**:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'  # change if needed
.\scripts\build_windows.ps1
```

**Expected only after native success**: Visual Studio Release `ALL_BUILD` succeeds; CTest reports `76/76`; the live sketch editor and feature tree are clickable. Do **not** claim 76/76 based only on the source checks. If compilation fails, share the **first compiler error** and its associated source file, not only the final MSBuild summary. Keep the last known v1.56 functioning executable as a rollback option.

## Still-high-impact work — not completed in this candidate

1. Stable persistent topological naming for faces and edges, maintained through upstream geometry edits.
2. Full sketch constraints editing and degree-of-freedom solver feedback; circle and curved-profile trim/offset.
3. Editable *associative* multi-body sketches, Boolean features, and face-linked references across body IDs.
4. Revolve, sweep, loft, thickness, drafts, and extrusion profiles as full **graph node types**, rather than only existing standalone commands.
5. Selective dirty-subgraph regeneration with persistent intermediate results and performance benchmarks on large assemblies.
6. Comprehensive hit-test/selection modes and production-ready keyboard-command discoverability.

Earlier roadmap and other module documentation remain historical where they conflict with this release note.

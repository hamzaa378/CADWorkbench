# CADWorkbench v1.63 — mass, inertia and rigid-assembly engineering

## Status and baseline

**v1.62 is the latest independently confirmed Windows baseline:** the user reported a complete Release build and **81/81 native CTests passed**, 46.74 seconds, with executable SHA-256 `57E1B4B3624AB3EC2BA28F755EF67F9C11A27698F669A09DCDAD32FE324A4748`.

**v1.63 is a development candidate.** Two new CTests are registered (expected 83), but the complete MSVC/Qt/OCCT application has not been built or tested by the authoring environment. Preserve v1.62 until v1.63 passes the Windows acceptance gate.

## Newly functional behavior

1. **Per-body centroidal mass tensor.** Inspect -> Mass properties now reports full 3x3 tensor in kg·mm², ordered principal moments, normalized principal axes in world coordinates, radii of gyration, mass, volume, surface area, bounding dimensions and center of gravity. Its density may be entered temporarily without altering the body's stored material.
2. **Rigid-assembly mass and inertia.** Inspect -> Assembly center of gravity and inertia sums selected physical solids, using each body's assigned density and modeled placement. With no selected bodies, the command includes visible, non-surface, non-linked-Boolean bodies by default. It reports combined center of gravity and centroidal inertia using the parallel-axis theorem. Body selection can explicitly include linked Boolean geometry if it represents a physical component.
3. **Exportable inspection.** Both workflows show scrollable reports and save plain text. No geometry mutation, shape rebuild, undo checkpoint or project schema change is involved.
4. **Portable, independently tested math.** `MassTensorAnalysis` performs input validation, symmetric 3x3 diagonalization, nonnegative inertia checks, principal-axis degeneracy flags, density/volume unit conversion and parallel-axis aggregation. Native `SceneDocument` tests use actual B-rep primitives, transforms, undo/redo and save/reopen.

## Exact assumptions and limitations

- Body coordinates are in **mm**, density is in **kg/m³**. Inertia is about the corresponding center of mass and is reported in **kg·mm²** (also **kg·m²** after multiplying by 1e−6).
- An assembly is a list of modeled physical components, not the Boolean union of its solids. **Overlapping solids are counted separately**, including duplicate physical volume, and each body ID is permitted only once.
- The calculation assumes **homogeneous density within each selected solid**. It uses the currently modeled B-rep; absent or zero-density material assignments are rejected for assemblies.
- Principal axes for symmetric/repeated-moment bodies are non-unique; the report explicitly notes this. Results support mechanical pre-design calculations, not certified dynamic balancing, structural simulation or collision validation.
- This increment does not implement persistent face names, an assembly joint solver, arbitrary-section sweep/loft or the full 50-feature roadmap.

## Verification

On Windows (PowerShell from the extracted source root):

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 83
```

Then inspect the generated release executable and exercise both new Inspect commands. Test multiple boxes with assigned material densities and translated placements, capture the report, change the placement and verify that the center/inertia change. Run save/reopen on that scene.

Portable independent regression (Linux, macOS or Windows with a C++20 compiler):

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic -Isrc \
  src/core/MassTensorAnalysis.cpp tests/mass_tensor_math_v163_tests.cpp -o mass_tensor_tests
./mass_tensor_tests
```

## v1.70 roadmap status

This advances target **#14 Detailed mass tensor analysis** for individual solids and adds rigid-assembly aggregation. Full completion still requires the native Windows verification gate and any additional use cases discovered there. The other 49 roadmap targets retain their previous status.

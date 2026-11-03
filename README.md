# CADWorkbench

CADWorkbench is a native Windows CAD application written in C++20 with Qt 6 and Open CASCADE Technology. This repository contains the **v1.55.0 source snapshot with the latest working-tree additions**. Start with [CONTINUE_HERE.md](CONTINUE_HERE.md) if you are resuming development in another chat.

## What works in this source snapshot

- Direct solid modeling: primitives, sketches, oriented extrusions, revolve, sweeps, lofts, holes, patterns, Boolean operations, blends, shelling, draft, and transforms, plus dimensioned engineering templates.
- Constrained 2D sketches with points, segments, circles, geometric and dimensional constraints, circular cutouts, persistent polygon extrusion definitions, and edits that rebuild the solid. Newer constraints include symmetry, circle tangency, and point on a finite segment.
- A focused parametric rectangle/extrusion/hole workflow with named parameters, linked dimensions, linear through/blind-hole patterns, all-edge chamfer/fillet, optional-operation suppression, and one-step batch suppression. This is a bounded feature chain.
- Persistent reusable component occurrences with 3-axis position/orientation, rigid/revolute/prismatic joints, motion tracks, sampled interference scans, and BOM-linked metadata.
- Saved model-linked drawing sheets with view placements, details, sections, notes, parts lists, balloons, and selected-edge dimensions. Export SVG, PDF, or editable DXF; exported files are snapshots.
- Editable faceted folded sheet-metal bodies, flat-pattern DXF with individual bend K factors and optional end reliefs, and bend-table CSV.
- STEP assembly and IGES exchange, STL/OBJ mesh import and repair, surface bodies, material assignments, BOM/parts-list exports, configurable drawing completeness checks, and named local revision snapshots.

The scope and limitations are detailed in [CONTINUE_HERE.md](CONTINUE_HERE.md). CADWorkbench does not claim Fusion 360 feature parity or formal ISO/ASME certification.

## Build on Windows

Install a Windows C++ MSVC toolchain with Visual Studio CMake tools. Bootstrap [vcpkg](https://github.com/microsoft/vcpkg) and set `VCPKG_ROOT` to its folder. The checked-in [vcpkg.json](vcpkg.json) lists Qt and OCCT dependencies.

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\build_windows.ps1
```

The script configures CMake, builds a Release application and tests, then runs CTest. To build without rerunning tests, use `-SkipTests`. Dependency installation and the first compile can take substantially longer than an incremental build.

## Verification

The current Windows Release build passes and **72/72 CTest cases pass**. The latest coding rounds did **not** include live UI interaction, so menu behavior is compiled but not claimed as interactively verified. See [CONTINUE_HERE.md](CONTINUE_HERE.md) for the verified feature summary and next work.

## Source layout

| Path | Contents |
|---|---|
| [src/core](src/core) | CAD scene, geometry, sketches, drawings, assemblies, sheet metal, surfaces, meshes, and revisions |
| [src/ui](src/ui) | Qt desktop window, commands, ribbon, and viewport |
| [tests](tests) | Native C++ regressions and supplemental source checks |
| [scripts](scripts) | Windows build, runtime release packaging, and source snapshot packaging |
| [docs](docs) | QA notes and historical documentation |

Additional current area notes are in [BOM_PARTS_LIST.md](BOM_PARTS_LIST.md), [ORTHOGRAPHIC_DRAWINGS.md](ORTHOGRAPHIC_DRAWINGS.md), [PARAMETRIC_FOUNDATION.md](PARAMETRIC_FOUNDATION.md), [PERSISTENT_ASSEMBLIES.md](PERSISTENT_ASSEMBLIES.md), [SHEET_METAL.md](SHEET_METAL.md), and [STANDARDS_CHECKS.md](STANDARDS_CHECKS.md). The previous long README is preserved as [historical documentation](docs/README_HISTORY.md); some of its status statements and links predate this snapshot.

## Publishing the source

Run `./scripts/package_source_snapshot.ps1` to create a source ZIP with a SHA-256 manifest. Extract the ZIP and upload the **contents** of `CADWorkbench_v1_55_Source` to GitHub. The source archive intentionally excludes Git metadata, local build outputs, vcpkg installations, generated binaries, and caches. The separate `package_release_windows.ps1` script builds a portable application package and installer from an already compiled Windows Release app and installed runtime dependencies.

The project currently has no license file; choose one before distributing it as an open-source release.

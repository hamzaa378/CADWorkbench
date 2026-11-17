# CADWorkbench v1.70 — Three workstream workflow increment (PRELIMINARY)

Development base: `CADWorkbench_v170_ThreeAreas_ValidatedStages_RefinedMotion_FlatAgreement_PRELIMINARY_20261108.zip`. 8 November 2026.

## What changed

### Parametric modeling — transactional structural edits

The interactive mixed-history editor now rebuilds every stage in a **detached preview** before staging a **reorder** or **removal**. It validates every returned OCCT B-rep and finite volume. Broken input order, lost geometry, or missing sketch stages are rejected while preserving both the previously staged steps and the committed document. Save remains the only commit. This prevents accepting a structurally invalid history change, but does **not** solve general persistent topology after arbitrary splits/merges.

### Assembly engineering — selectable motion progression

The multi-drive trajectory preview now offers either linear drive progression (default, backward-compatible) or cubic smoothstep (`3t^2−2t^3`) progression. Each sampled coordinate is validated against explicit travel limits; legacy saved joints with pinned `minimum=maximum=initial` retain their prior movable behavior. Every sampled pose is still solved and OCCT-inspected by the existing mechanism preview. The new pure C++ interpolation component has independent numeric/regression coverage for endpoints, interior samples, multiple drives, duplicate indices, nonfinite values, bounds and atomic failure. **Neither mode is a dynamics solver or a continuous collision certificate.** Smoothstep changes the timing distribution of samples along the same joint-space line; sampled motion can still miss collisions between frames.

### Drawings and sheet metal — saved linked bend-table export

The saved associative flat blank exporter now adds a third output option, CSV bend schedule, alongside validated SVG and DXF. All three outputs use the same saved folded source and linked blank parameters. The CSV includes bend direction, signed angle, radius, per-bend K-factor, bend allowance, bend deduction, bend station, developed length, and sheet width. Files are written with Qt's single-file atomic save. Validation or cancellation makes no scene changes. This is a production-planning report, not manufacturing or bend-feasibility certification.

## Test results for this candidate

- Python/source-level and independent OpenCascade reference checks: **471/471 passing**.
- Portable C++: **16/16 GCC targets** passing (without sanitizer); **16/16 Clang targets** passing with ASan/UBSan.
- New isolated motion interpolation suite: **230 assertions passing** under both compiler runs.
- Native Qt/OpenCascade regression additions in `articulated_assembly_scene_v170_tests.cpp` and `cadcore_tests.cpp` exist but **were not run on MSVC**.
- Registered Windows native CTests: **130** (not executed in this environment); the original face-machining regression needs Windows confirmation.
- Formal 60-scenario acceptance results are **not scored**.

## Current provisional completion estimates (not formal acceptance)

| Workstream | Previous | This candidate | Required release threshold |
|---|---:|---:|---:|
| Parametric modeling | ~75% | ~76% | 85% |
| Assembly engineering | ~82% | ~83% | 85% |
| Drawings/sheet metal | ~67% | ~68% | 65% |

These are qualitative implementation estimates only. The current increase is intentionally small because GUI integration and end-to-end Windows checks are not yet verified. Continue improving the third workstream beyond its minimum, as requested.

## Verification on Windows

From a **fresh extracted folder**, in PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

For targeted prior-failure verification:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_PREVIOUS_FAILURES.ps1 -VcpkgRoot C:\vcpkg -OnlyInterleavedHistory
```

Then check the three new GUI interactions manually: (1) reorder/remove an invalid staged history, (2) linear vs. smoothstep multi-drive preview without committing, and (3) export the same saved linked flat development to SVG, DXF, and CSV. Preserve last passing Windows executable until full native regression and acceptance gates pass.

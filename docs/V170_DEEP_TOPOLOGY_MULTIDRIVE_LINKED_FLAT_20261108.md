# CADWorkbench v1.70 — Deeper engineering increment (PRELIMINARY)

Date: 2026-11-08. This increment is based on the prior Integrated Previews preliminary source snapshot. This is **not** a Windows build-verified release.

## 1. Repeated Boolean face rebasing

`BooleanReferenceLineage.cpp` now keeps the **original** design witness at its physical world location across successive Boolean splits and coplanar merges. On each rebase, OCCT ancestry must identify the successor, and the old witness must lie on that successor's trimmed planar interior. An optional second witness is preserved only if that *same* physical point still survives. Offsets (`u`, `v`) are still re-expressed in the successor's local plane. Unlike resampling at a new centroid every edit, this prevents silent witness drift.

Native coverage added to `face_lineage_v170_tests.cpp` for repeated trims, merge, and retained original witness. The general topological naming problem remains **unsolved**, especially degenerate, ambiguous, disappearing and curved face cases. No universal guarantee is claimed.

## 2. Multi-drive articulated assemblies

A new interactive **Edit multiple articulated drives (atomic preview)** action edits a combination of saved driven revolute/prismatic joint coordinates. The document validates the batch, runs **one full articulated graph solve**, makes an optional read-only exact nominal OCCT fit audit of up to twelve involved bodies, and can commit the batch in **one** undoable transaction. A failure leaves original coordinates and placements unchanged; a no-op creates no new revision. Angle UI units are degrees; underlying model uses radians; prismatic inputs are millimeters.

Native coverage added to `articulated_assembly_scene_v170_tests.cpp` for independent drives, invalid inputs, undo/redo, and save/reopen. This is nominal/static pose validation, **not** continuous collision-free motion, solver-completeness certification, or a general closed-loop simulator. When >12 bodies are involved, the fit audit is explicitly skipped.

## 3. Associative multi-flange neutral flat blank

A new **Associative flat blank from selected folded sheet** command creates a **separate stable-ID derived solid** from the saved multi-flange definition. Its thickness and width match the source, and its length comes from the existing multi-bend neutral-development computation (K = 0.33). Every edit of the folded source calls the common dependency planner and rebuilds the blank **in the same staged transaction**. Invalid source geometry or invalid dependencies roll back both bodies. Deletion of only the source is blocked; undo/redo and save/reopen link validation are implemented. Direct scaling, transforming, or destructive modeling operations on the derived blank are guarded to prevent silent divergence.

Native coverage added to `cadcore_tests.cpp`, including three flanges, regeneration, rollback, direct-scale guard, undo/redo, and save/reopen. **Important limit**: the generated solid is a simple neutral rectangular blank, *not* a production-certified unfolded geometry with bend reliefs, notch propagation, corner treatment, hem forming, overlap resolution, or manufacturability verification. DXF/SVG relief previews remain separate workflows. Do not manufacture from this neutral blank without checking the detailed development.

## Current implementation estimates (not formal acceptance scores)

| Workstream | Previous estimate | New estimate | Final target |
|---|---:|---:|---:|
| Parametric modeling | 67% | 69% | 85% |
| Assembly engineering | 71% | 74% | 85% |
| Drawings and sheet metal | 52% | 57% | 65% |

These increments are **planning assessments** only. The mandated 60-scenario acceptance checklist is not completed. No claim of meeting the final gate is made.

## Verification and release

- Python/source-contract checks: 420/420 pass in this environment.
- Portable C++ checks: 14 GCC and 14 Clang targets passed with ASan/UBSan in the preceding combined runs; the final delta is in Windows-only C++ and source contracts.
- Independent OCP geometry fixtures run within the Python/source suite, but **do not execute these new native C++ codepaths**.
- Registered Windows native CTests: 130, unchanged.
- Windows MSVC, native Qt UI and OpenCascade CTest execution of this snapshot: **NOT RUN**. The original face-machining regression remains pending native verification.

To verify on Windows from the newly extracted project root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

Do not replace the previously Windows-verified baseline until both the native build and all 130 tests pass. The final release requires two areas >=85% and the third >=65% **with formal end-to-end acceptance evidence**.

# CADWorkbench v1.70 — coordinated engineering workflows (PRELIMINARY)

## Development intent

Keep the two existing Windows-native regression failures in the CTest inventory without changing their test expectations or attempting to hide them. Advance substantial source functionality in all three workstreams, and provide a single complete source archive. These changes require a subsequent native Windows compile, three focused CTests, and eventually the entire 130-test release gate.

## Previous Windows evidence (user-supplied 8 November 2026)

- **Windows Release CMake, MSVC core library and CADWorkbench.exe built.**
- **128 of 130 native tests passed** in the previous source.
- `cadcore_v170_assembly_mate_document`: failed assertion `predicted gap shortfalls were not shown with requested clearance`.
- `cadcore_folded_sheet_metal`: `***Exception: SegFault`.
- Both failures remain **known blockers, deferred for later corrections**. The v1.70 final release is not approved. A native crash is a serious defect even when its correction is deferred.
- Neither failing test is disabled or removed. No production pass/fail values are fabricated.

## A — Parametric modeling: multi-body transaction editor

- `SceneDocument::previewNamedParameterBatch` runs the production checked edit and downstream linked-body propagation on a detached document for up to 12 source bodies, reports the rebuilt solids and exact OCCT volumes, and preserves the caller's output on failure.
- `SceneDocument::setNamedParameterBatch` repeats the validated staging and commits all sources and linked dependents with **one scene checkpoint**. Failed secondary-body edits leave every original source and descendant intact.
- New **Edit named dimensions across selected parametric bodies** action provides a multi-body table, a full-scene geometry preview, and explicit approval before commit.
- Explicit edits of parameters carrying saved expressions use the established single-part expression-replacement rule. Driven values are excluded from direct editing.
- Limitation: sources are staged in stable ID order; a coordinated change that would require accepting an invalid intermediate source geometry is rejected. Arbitrary topology-naming after arbitrary face merges is not certified.

## B — Assembly engineering: exportable articulated motion evidence

- `SceneDocument::buildLiveArticulatedMotionCsv` uses the same saved drive ranges, linear/smoothstep interpolator, complete articulated solver and nominal OCCT fit checks as sampled motion preview.
- CSV captures the sample fraction, interpolation, threshold, joint coordinates and a complete escaped pose review. Each sample is solved independently; unsafe strict paths reject the entire output atomically.
- **Export sampled motion CSV** is accessible from the multi-drive review. Export does not move the model or commit any coordinate. The user can cancel the commit after saving the evidence.
- Limitation: sampling is **not continuous collision or dynamic safety certification**. The existing 12-occurrence nominal-pair audit limit remains enforced by strict policy.

## C — Drawings/sheet metal: associative manufacturing package

- `SceneDocument::exportLinkedFlatManufacturingPackage` checks the current saved linked blank, regenerates SVG/DXF/bend CSV from its one authoritative folded source, and creates a JSON manifest with source IDs, current geometry tokens, flange dimensions, signed bend angles, per-bend K-factors and relief parameters.
- SHA-256 hashes for each output allow package receivers to detect file modifications.
- Uses a **temporary sibling folder** and publishes the whole package by directory rename after all files finish writing. Existing destinations are never overwritten.
- New **Export associative flat manufacturing package** action creates a uniquely named folder beneath the user-selected directory.
- Limitation: this is a neutral geometric fabrication package, **not** a tooling, springback or forming-feasibility certification.

## Verification performed outside Windows

- Full Python/source and independent OCCT reference suite.
- Portable C++ checks with GCC and Clang; Clang includes ASan/UBSan.
- Added native regression assertions to existing test executables without changing the expected 130 CTest names.
- A **source hash/ZIP integrity** check is performed before delivery.
- Actual Windows compilation of *this revised source* and the new native assertions remains **pending**.

## Focused Windows test (development only)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_NEW_WORKFLOWS.ps1 -VcpkgRoot C:\vcpkg
```

This validates the three relevant existing CTest targets without claiming the rest of the product is green. It is not a substitute for the full gate.

## Final v1.70 release gate

After the deferred failures are corrected, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

Native build and all 130 CTests plus evidence for the formal 60 acceptance scenarios and the 85% / 85% / 65% requirement must pass. Prior functional percentage estimates are not acceptance evidence.

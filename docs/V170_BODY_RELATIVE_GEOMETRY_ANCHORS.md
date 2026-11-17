# CADWorkbench v1.70: Body-relative geometry anchors (development increment)

## Goal

Preserve face-attached and selected-edge feature references after large translations
of their upstream source body. The earlier absolute-centroid descriptors could
fail simply because the body moved farther than one face width or edge length.
This is a focused improvement toward roadmap #1 (stable topological naming) and
#7 (associative planar-face attachment), **not** complete persistent topology naming.

## Implemented behavior

- New planar-face anchors add `face_anchor_body_x`, `_y`, `_z` to the original
  seven face descriptor fields. The body reference center is computed from
  volume mass properties, falling back to surface or linear geometry for
  non-solid shapes. Face resolution compensates for the change in that center
  before applying the existing area, normal, distance, and ambiguity tests.
- New unique-edge anchors add `edge_body_x`, `_y`, `_z` to the original four
  descriptor fields. Capture and matching continue to use the unique-edge
  topology index, not duplicate face traversal edge occurrences.
- Old saved seven-field face anchors and four-field edge anchors remain accepted
  with their original **absolute-world-position** matching semantics. They are
  not silently upgraded because the original source body reference center
  cannot be reconstructed reliably from an old file.
- Incomplete/orphaned body-reference fields are rejected before geometry work.
- Feature-tree fillet and chamfer operations retain the new anchor fields when
  resolving selected edges. Face-based features also retain these fields in
  the editing dialog and save/reopen document format.
- Ambiguous or missing geometry still fails closed. No inference is made across
  face splitting/merging, similarly shaped repeated geometry, or arbitrary
  topology changes.

## Native regression coverage

Existing registered CTest targets were extended without increasing the count:
`cadcore_v170_face_attached_sketch`, `cadcore_selected_edge_v160`, and
`cadcore_associative_features_v159`. They exercise large translations,
legacy-anchor acceptance, missing optional fields, real feature-tree geometry,
rollback, and persistence. The candidate still registers **95** CTests.

## Independent checks

`python tests/v170_body_relative_anchor_reference.py` uses CadQuery/OCP to
compare real face and edge centroid geometry under several translations,
including all six faces and twelve edges of a rectangular box. It is not a
substitute for MSVC compilation. `python tests/v170_body_relative_anchor_preflight.py`
checks the source integration contract.

## Windows acceptance

From a fresh extraction:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 95
```

**Baseline status:** The user confirmed 94/94 for the earlier v1.70 advanced
profiles package. The immediate face-attached 95-test candidate and this new
increment have not yet been reported as passing Windows tests in this chat.

## Limitations

This is centroid-compensated geometric matching, not a persistent topological
naming kernel. Large rigid translations are supported for newly captured
anchors; rotation, mirror, scaling, face splitting/merging, and symmetric
ambiguous geometry still require explicit reattachment or additional lineage.

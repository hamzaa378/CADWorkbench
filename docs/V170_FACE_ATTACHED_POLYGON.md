# CADWorkbench v1.70 — Face-attached polygon sketch increment

This is an additional **v1.70 development increment** based on the user-confirmed **94/94 Windows-tested** advanced-profile candidate. It is **not** an assertion that every target on the original 50-feature roadmap is implemented.

## Added to the existing feature tree

- `Sketch polygon face boss`: 3–24-vertex straight-edged planar sketch creates a solid protrusion outward from the selected face.
- `Sketch polygon face pocket`: 3–24-vertex planar sketch cuts into the solid along the inward face normal.
- Face-local sketch authoring: coordinates are millimetres relative to the selected planar face surface centroid; `u`/`v` translate the outline within that face, and depth controls extrusion.
- Captured planar-face descriptor: stored with the feature node where the selected face is the final feature-tree output; the feature attempts to relocate the face when earlier operations alter geometry, rejecting ambiguous or missing matches.
- Same sketch canvas and exact-coordinate entry as earlier polygon features.
- Out-of-plane face removal, invalid sketches, disconnected bosses, and no-op cuts fail without committing edited geometry.
- Save/reopen, undo/redo, branch-dependent regeneration and cache behavior reuse the existing document architecture.

## Scope and limits

- Straight polygon edges only, no circular holes or constrained B-splines in the face profile. No automatic constraint projection onto a 3D face.
- The feature is attached by a **geometric face anchor**, not by fully persistent topological naming. Split/merged faces, large translations, geometry-symmetric ambiguities and drastic face changes may require reselection.
- A face anchor is automatically captured only when adding at the end of the tree with one face selected from the current body. Insertion at an earlier position uses the numeric face index (which is less robust).
- The local frame matches the existing `Face rectangle` tools; the selected face's outward normal is estimated from the solid centroid. Concave/reentrant faces can require careful manual verification.
- A boss must not add a disconnected solid. A pocket must remove material. These checks are part of the transactional geometry rebuild.
- No general cross-body dependency engine, multi-plane linked sketches, CAM G-code, or blanket v1.70 completeness claim.

## Windows acceptance

The project defines **95 native CTests** (up from 94). In PowerShell at the extracted project root:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 95
```

The new test `cadcore_v170_face_attached_sketch` verifies both boss and pocket, different face orientations, 3D volume, upstream translation, edited depth, invalid anchors, disconnected/no-op geometry rejection, undo/redo, and save/reopen.

## Evidence available here

Python preflight and contract checks, independent OCP kernel geometry checks, and packaged checksum/patch reconstruction are executed on Linux. The **native Windows MSVC build and 95 CTests are not run here** and cannot be marked passing until the user runs the above script.

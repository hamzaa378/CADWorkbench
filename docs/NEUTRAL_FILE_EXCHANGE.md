# Neutral-file exchange

CADWorkbench currently supports three bounded neutral-file workflows:

| Format | Import | Export | Scope |
|---|---|---|---|
| STEP (`.step`, `.stp`) | Yes | Yes | XDE solid/assembly names, nested paths, reusable definitions, occurrences, and placements where present in the file. |
| IGES (`.iges`, `.igs`) | Solid bodies only | Yes | Imported solid bodies are flattened into scene bodies. The exchange does not retain IGES product hierarchy, entity labels, surface-only references, or PMI. |
| STL (`.stl`) | Closed manifold triangle meshes | Yes | Imports as faceted B-rep solids with a bounded sewing tolerance. STL coordinates are interpreted as millimeters. |
| OBJ (`.obj`) | Closed polygon meshes | No | Polygon faces are triangulated and sewn into faceted B-rep solids. OBJ coordinates are interpreted as millimeters; textures, materials, UVs, and normals are discarded. |

IGES and OBJ import are each limited to 512 MiB and committed as one undoable operation. Transferred solids are validated before any body is added. Files without closed solid bodies are rejected transactionally. OBJ faces accept positive or negative vertex indices and polygons are triangulated from the first vertex; concave/non-planar source polygons may not preserve their original surface tessellation. IGES export writes the current scene geometry in millimeters as a B-rep exchange model; it does not preserve assembly placement metadata or CADWorkbench materials and purchasing fields.

STEP is the preferred format for assembly data. See [`STEP_IMPORT.md`](STEP_IMPORT.md) for its hierarchy and persistence limits, and [`STL_MESH_IMPORT.md`](STL_MESH_IMPORT.md) for mesh welding and closed-shell requirements.

## Verification

Windows x64 Release regressions pass **15 IGES assertions** and **17 OBJ assertions**. IGES tests export and reimport a box and cylinder, check validity and combined volume, verify undo/redo, reject missing input without changing the scene, and reject exporting an empty scene. OBJ tests import triangulated quads with negative indices, check solid validity and volume, verify undo/redo, and reject open, malformed, and missing meshes. This is automated core verification; live UI testing was not run for this update.

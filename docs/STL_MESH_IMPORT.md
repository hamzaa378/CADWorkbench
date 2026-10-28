# Closed STL mesh import

CADWorkbench can import a closed manifold STL file as a faceted solid body through **File → Import closed STL mesh as faceted solid**. The importer sews nearby triangle edges using a default tolerance of 0.01 mm, converts closed shells to valid solids, derives the body name from the file, and adds the result in one undoable operation. STL carries no unit metadata; CADWorkbench interprets coordinates as millimetres.

Import is limited to 512 MiB. The weld tolerance can be specified through the core API from 0.000001 to 1 mm. Missing, empty, unreadable, invalid, and open meshes are rejected without changing the active document. A mesh with disconnected closed shells can be imported as one scene body containing multiple solids when the geometry kernel accepts each shell.

This is a bounded neutral-file intake workflow. It does not repair self-intersections, fill open boundaries, remesh, decimate, preserve colors or per-facet metadata, or create editable surface/mesh bodies. STL export remains available separately.

Windows Release regression coverage passes **17 assertions**: it exports a faceted box, imports it, checks solid validity, triangle-face count and volume, verifies undo/redo, and rejects missing input, invalid tolerance, and an open triangle. The current focused result is recorded in [`QA_MATRIX.md`](QA_MATRIX.md). No live UI test was run for this work session.

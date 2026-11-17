# CADWorkbench v1.70: External associative sketch extrusion

## Implemented (development candidate)

A constrained-sketch extrusion can now serve as the live geometric source of a second, independent solid. The dependent uses the source's **stored SketchModel, boundary point IDs and SketchPlane**, but has its own editable extrusion **depth** and signed **normal offset**. The source is not consumed or duplicated in the dependency graph. The normal is normalized before applying a physical millimetre offset.

**Create → Associative extrusion from selected body's sketch...** selects a source body with an existing, current editable constrained sketch. **Edit → Edit linked sketch extrusion depth/offset...** changes the derived solid. The model browser and Inspector show source identity and whether the link is current. The document audit flags stale links.

The existing linked-Boolean DAG now recognizes **unary sketch-derived nodes** alongside two-input Boolean nodes. Dependents rebuild in topological order when the source sketch is edited using the constrained-sketch editor, including chained Boolean cuts, unions and intersections. Failed geometry, unsupported stale source sketches, invalid parameters, cycles and missing sources fail before the document checkpoint. Existing Boolean retargeting/preview can include the new unary dependencies. Deleting a source requires selecting its live sketch-linked dependents too. Linked result geometry cannot be changed directly through the guarded modeling commands.

## Persistence and compatibility

The current v24 `.cadscene` JSON retains a new optional `linked_sketch_extrusion` record on the dependent body (`source`, `source_token`, `depth_mm`, `normal_offset_mm`). Existing files without this record continue to load. New files validate source identity, source geometry token, derived volume and placement on reopening; corrupt or missing links are rejected without replacing the open document. **Older CADWorkbench builds do not understand this optional record and may discard it if they reopen and resave a newer project.** Make a backup before opening the project in an older build. No new binary file format or migration utility is claimed.

## Scope and limitations

- Source must already have a **current constrained-polygon/sketch extrusion** stored in the document. Arbitrary imported edges, assembly occurrence faces and external CAD documents are *not* yet supported as sketch sources.
- The linked body is a **standalone extrusion**, not an additive feature inside another body's modifier stack. Use an associative Boolean result to combine it with other solids.
- Depth and normal offset are editable. The source sketch is edited using its existing sketch editor. Renaming either body retains the stable-ID reference.
- The update uses transactional rebuilds, not a universal persistent topological naming service or a full geometric assembly mate solver.
- If the source's saved sketch becomes stale after an unsupported direct geometry change, edits fail rather than using stale coordinates.

## Tests / acceptance

**New native CTests:** `cadcore_v170_linked_sketch_chain` and `cadcore_v170_linked_sketch_persistence`. The first covers source-edit propagation through a Boolean grandchild, physical normal offsets, depth edits, rollback, undo/redo, impact inspection and deletion protection. The second covers save/reopen, editing a reloaded source, missing source-ID corruption and tampered saved geometry. The portable linked-body graph planner additionally checks unary/binary mixed DAGs, cycle detection and impact order.

Total registered native tests: **106**. Run this on Windows from the extracted source root:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 106
```

**Verification boundary:** GCC/Clang portable graph tests, source-contract suites, static CMake parse, independent OCCT reference scenarios and archive integrity can be verified outside Windows. A full MSVC build and all 106 Qt/OpenCascade CTests require the user's Windows environment. This package is a candidate until that exact run passes. The last explicitly reported Windows-pass result in this conversation is the 94-test advanced-profile increment; later candidates have not been confirmed.

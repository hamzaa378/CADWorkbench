# Parametric sketch foundation

CADWorkbench v1.37 adds a bounded editable feature chain for an XY rectangle, base extrusion, and one circular through-hole cut. The v1.45 extension adds a second fully constrained rectangle sketch with an editable sketch origin, including negative coordinates. Version 1.47 adds an optional linear pattern; v1.48 supports X or Y direction with editable integer count and center spacing. Version 1.54 adds an optional all-edge chamfer; v1.55 adds an optional all-edge fillet. Each stores an editable named dimension. All feature records and parameters persist and edits rebuild the affected solid.

## Create and edit

1. Choose **Create → Constrained rectangle + editable extrusion...** and enter width, height, and extrusion depth in millimeters. For a movable profile, use **Create → Constrained offset rectangle + extrusion...** and set origin X/Y as well.
2. Select the resulting body and choose **Create → Add parametric through-hole...**. Enter the hole diameter and center coordinates in the sketch plane.
3. Choose **Edit → Show selected sketch constraints / feature history...** to inspect the stored constraints, named parameters, and ordered operations.
4. Choose **Create → Pattern parametric through-hole...** after adding one through-hole. Select X or Y, then set 2–100 instances and center spacing. Holes must not overlap and all circles must remain inside the rectangle.
5. Choose **Edit → Edit named parameters...** to edit rectangle `width`, `height`, `depth`, hole `hole_diameter`, `hole_x`, `hole_y`, and (when patterned) `hole_pattern_count` and `hole_pattern_spacing` together. The complete parameter set is validated and rebuilt atomically as one undo/redo step. The single-parameter core API delegates to the same transaction path.
6. Choose **Edit → Add parametric all-edge chamfer...** on a current parametric body, then set `chamfer_distance` in **Edit → Edit named parameters...**. The chamfer follows the hole and pattern operations, is rebuilt after base dimension edits, and can be suppressed or restored from the feature-history editor. The kernel rejects distances that cannot be applied to every current edge.
7. Choose **Edit → Add parametric all-edge fillet...** to add a rounded blend instead. Edit `fillet_radius` through the same named-parameter dialog and suppress or restore it in the feature-history editor. The bounded workflow accepts one blend operation at a time: adding a chamfer and fillet to the same history is rejected.
8. Choose **Create → Add parametric blind hole to selected base...** on a plain rectangle extrusion. Set its diameter, center, and depth; the `hole_depth` parameter can be edited, linked, and suppressed/restored from feature history. The center must remain inside the profile and the hole must leave at least 0.01 mm of material at the bottom.
9. Choose **Create → Constrained trapezoid extrusion...**, **Constrained triangle from side lengths...**, or **Constrained polygon from vertices...** for a solved polygon solid. Select it and choose **Edit → Edit selected constrained polygon sketch...** to change its dimensions, anchored vertices, and extrusion depth. Save and reopen the scene to continue editing; a later direct geometry edit marks this sketch stale.

Submitting unchanged named parameter values (individually or in a batch) is a no-op: after input validation, it skips the geometry rebuild and preserves the existing solid identity and undo/redo history. Invalid values are still rejected even when the other submitted values are unchanged. Dependent hole-containment and linear-pattern spacing checks run before any replacement solid is constructed; rejected edits preserve the existing shape, parameters, geometry token, and undo state.
When reopening a saved scene, the loader also checks that each rectangle's origin plus its width and height stays within the supported ±1,000,000 mm sketch-coordinate range. This applies to archived stale histories as well as editable ones. For current (editable) through-hole histories, it additionally probes the saved solid both near the center and close to the named bore wall of each hole, at 10%, 50%, and 90% of the extrusion depth (offset from tangent edges to avoid classifier ambiguity). The near-wall check rejects some forged count/diameter combinations that preserve volume while disagreeing with the saved radii; multiple depths also detect some wider, shallower blind bores misrepresented as through-holes; a matching volume and bounding box alone cannot establish that holes are where their parameters say they are. It rejects detected mismatches without replacing the open scene. This is a targeted sanity check, not a general geometry-equivalence proof; correctly formed stale histories remain inspectable but read-only.

For linear-hole patterns, creation subtracts the added bores as one compound-cutter Boolean, and named-parameter rebuilds subtract all current bores in one compound cut; this avoids repeated Boolean operations as the pattern approaches the supported 100-instance limit. The outer edit transaction and its single undo/redo checkpoint are unchanged.

The history contains a rectangle sketch, extrusion, circle sketch, through-hole cut, optionally a linear-pattern operation, and optionally all-edge chamfer followed by all-edge fillet. Both blend operations can coexist in that order; the history rebuilds chamfer before fillet and each can be suppressed/restored independently. The pattern stores its axis, count as a bounded integer, and center spacing in millimeters. Chamfer and fillet store positive `chamfer_distance` or `fillet_radius` parameters; rebuild validates the kernel result before replacing the solid. Scene schema v19 stores X/Y pattern direction; schema-v18 X-only patterns migrate as X, and schemas v1–v17 remain readable. Coordinated edits allow body dimensions, profile origin, and dependent hole locations to change in one rebuild, avoiding invalid intermediate states; invalid combined values leave the model untouched.

If a later direct-modeling operation changes the body, the history remains reviewable but becomes stale; parameter edits are blocked so they cannot silently overwrite later work. The through-hole center must stay inside the rectangle profile and the hole diameter must be positive and valid for the profile.

## Bounded named-parameter dependencies (core API)

The parametric core now supports directed dimension links of the form `target = source * scale + offset` through
`setParameterLink(partId, target, source, scale, offset)`, `removeParameterLink(partId, target)`, and `parameterLinks(partId)`.
The existing overload without an offset remains equivalent to an offset of zero. For example, link `height`
to `width` with scale `0.5` and offset `5 mm`, then editing `width` automatically updates `height`
and rebuilds the shape in one undoable transaction. This can express a bounded allowance rule such as a wall
or clearance dimension that tracks a driver plus a fixed amount. Chained dependencies are evaluated source-first; cycles,
direct editing of driven values, invalid scales and downstream sketch-constraint violations are rejected
without changing the document. Only existing positive dimensions (`width`, `height`, `depth`,
`hole_diameter`, `hole_pattern_spacing`) can participate. Coordinates and integer pattern counts cannot.
The graph is capped at four links per body, scales must be positive, and offsets are bounded to ±1,000,000 mm
(no arbitrary expressions).
Calling `setParameterLink` again for an existing target replaces that dependency in one undoable edit. The
proposed graph and all derived dimensions are validated before rebuilding; rejected cycles, out-of-range
values, or downstream sketch violations keep the prior link and solid. An identical link is a no-op and
preserves undo/redo. Removing a link leaves the current numeric target value unchanged and is undoable
without a geometry rebuild.

Use **Edit → Edit named parameter links...** to create, replace, or remove a link on the selected parametric body. The editor lists only supported dimensions, displays and edits the source, scale, and offset, and applies changes through the core transaction so a failed edit keeps the existing link and solid.

Links are saved as optional `parametric_definition.parameter_links` entries beginning in schema v19. Zero-offset links
retain their compact source/scale form. Schema v20 adds a bounded optional offset; v19 links without offsets
still load as zero-offset relationships, and older scenes without the link field continue loading.
The loader validates node names, acyclicity, scales, offsets, and agreement
between stored dimension values and their derived values, even for stale histories. This field is an
extension to v19/v20; older executables do not understand schema v20 and cannot safely open scenes using affine links.
The focused Windows Release regression passes **65 assertions** through `cadcore_tests --parametric-links` (registered as `cadcore_parametric_links` in CTest); it covers chained affine links, schema-v19 compatibility, offset persistence, invalid-offset rejection, atomic rebuild, and tamper detection. Schema v21 adds persisted blind-hole feature records and the `hole_depth` dimension; earlier scenes remain readable, while older executables cannot safely open a scene containing that operation.
This is a limited directed parameter graph, not a general constraint solver or arbitrary feature DAG.

## Scope

The parametric feature template remains bounded: it does not support arbitrary feature reorder/insert, branches, or a general downstream feature graph. The core now contains a reusable 2D `SketchSolver` API for point-and-segment geometry. It solves coincidence, horizontal/vertical, fixed-point, distance, equal-length, parallel, perpendicular, angle, signed horizontal/vertical distance, point-on-line, midpoint, and collinear constraints, and reports numerical rank and remaining degrees of freedom. A general constrained-polygon extrusion API uses the solver. The Create menu exposes trapezoid, triangle, and **Constrained polygon from vertices...** workflows; the last accepts 3–12 X/Y pairs and fixes each entered vertex. Their point, segment, constraint, boundary, depth, and geometry-token records persist in the scene. **Edit → Edit selected constrained polygon sketch...** updates dimensional and anchor values, rebuilds the solid, and supports undo/redo after reopening. Direct geometry edits make the sketch history stale. The UI still cannot add/remove non-fixed geometric constraints, and arcs/splines and dimensional expressions remain future work. Supported parametric chains include one XY rectangle, one extrusion, either one circular through-hole with an optional X- or Y-direction pattern or one blind hole, and optional all-edge chamfer/fillet operations in the supported order. Blind holes are not yet accepted by the linear-pattern operation.

The solver uses a damped least-squares iteration and returns a result only if every residual meets the requested tolerance; it leaves the input geometry untouched. Its API is described in [`src/core/SketchSolver.h`](src/core/SketchSolver.h). `sketch_solver_tests` covers solved, underconstrained, conflicting, and invalid sketches; constrained solid creation; editable save/reopen; dimension rebuild; no-op and invalid-edit guards; stale-history rejection; and undo/redo. Complex constraint systems and direct UI interaction remain unverified.

## Verification

Earlier v1.48 Windows Release focused regression (before the no-op, preflight, and saved-extents changes): **PASS, 113 assertions**. It checks origin-anchored and offset sketches, negative origin/hole coordinates, X/Y pattern placement and limits, containment, geometry volume, rebuild results, coordinated parameter edits, invalid-edit atomicity, schema-v19 persistence and schema-v18 migration, stale-reference invalidation, and undo/redo.

Earlier v1.48 live Windows UI verification: **PASS**. On a 40 × 30 × 20 mm extrusion, the UI created one 8 mm through-hole at (20, 15), then applied a Y-axis pattern with 2 instances and 8 mm center spacing. The viewport showed both holes and Properties reported 21,989.381 mm³, 10 faces, and 48 edges. The named-parameter editor displayed the pattern values, and the history view showed the constraints and all five ordered operations. The rebuilt executable launched with the correct `CADWorkbench v1.48.0` title.

Current continuation verification (parametric passes 5–7): incremental patches are checked for exact byte-for-byte application to their preceding checkpoint, and the cumulative patch is checked against the original source ZIP. On Linux/GNU 14.2.0, CMake configuration with `-DBUILD_TESTING=ON` exits 1 at the Windows/MSVC guard in `CMakeLists.txt:10`; no native C++ build or `cadcore_tests` execution is available here. An independent Python/OCP test script passed 10 reference scenarios: single, tangent, separated, and 100-hole X/Y compound-cut geometry and depth probes, a volume-preserving count/diameter mismatch, and a volume-preserving blind-hole mismatch. These reference checks support the implementation but are **not** compiled CADWorkbench regression tests. No live UI testing was performed.

Dependency-link continuation checks on this Linux workspace: the pass-8 diff was checked for clean application
to the saved seven-fix v1.48.0 CMake source and the changed-file archive was checked by byte comparison.
CMake configuration with `-DBUILD_TESTING=ON` remains blocked by the Windows/MSVC guard at
`CMakeLists.txt:10`, so neither `cadcore_tests --parametric-links` nor `--parametric-foundation`
has been compiled or executed here. Independent OCP reference checks are not a substitute for native
parameter-link regression execution. No live UI testing was performed.

Current Windows continuation verification: the editable parametric blind-hole operation passes **31 focused assertions** in `cadcore_tests --parametric-blind-hole` (also registered as `cadcore_parametric_blind_hole`). Coverage includes depth and profile validation, linked depth edits, atomic rebuild, suppression, undo/redo, schema-v21 persistence, reopen, and tamper/version rejection. The broader Windows Release CTest suite result is recorded in `docs/QA_MATRIX.md` and `docs/local_verification.json`. No live UI test was run for this continuation.

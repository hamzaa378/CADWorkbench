# Parametric sketch foundation

CADWorkbench v1.37 adds a bounded editable feature chain for an XY rectangle, base extrusion, and one circular through-hole cut. The v1.45 extension adds a second fully constrained rectangle sketch with an editable sketch origin, including negative coordinates. Version 1.47 adds an optional linear pattern; v1.48 supports X or Y direction with editable integer count and center spacing. All feature records and parameters persist and edits rebuild the affected solid.

## Create and edit

1. Choose **Create → Constrained rectangle + editable extrusion...** and enter width, height, and extrusion depth in millimeters. For a movable profile, use **Create → Constrained offset rectangle + extrusion...** and set origin X/Y as well.
2. Select the resulting body and choose **Create → Add parametric through-hole...**. Enter the hole diameter and center coordinates in the sketch plane.
3. Choose **Edit → Show selected sketch constraints / feature history...** to inspect the stored constraints, named parameters, and ordered operations.
4. Choose **Create → Pattern parametric through-hole...** after adding one through-hole. Select X or Y, then set 2–100 instances and center spacing. Holes must not overlap and all circles must remain inside the rectangle.
5. Choose **Edit → Edit named parameters...** to edit rectangle `width`, `height`, `depth`, hole `hole_diameter`, `hole_x`, `hole_y`, and (when patterned) `hole_pattern_count` and `hole_pattern_spacing` together. The complete parameter set is validated and rebuilt atomically as one undo/redo step. The single-parameter core API delegates to the same transaction path.

The history contains a rectangle sketch, extrusion, circle sketch, through-hole cut, and optionally a linear-pattern operation. The pattern stores its axis, count as a bounded integer, and center spacing in millimeters. Scene schema v19 stores X/Y pattern direction; schema-v18 X-only patterns migrate as X, and schemas v1–v17 remain readable. Coordinated edits allow body dimensions, profile origin, and dependent hole locations to change in one rebuild, avoiding invalid intermediate states; invalid combined values leave the model untouched.

If a later direct-modeling operation changes the body, the history remains reviewable but becomes stale; parameter edits are blocked so they cannot silently overwrite later work. The through-hole center must stay inside the rectangle profile and the hole diameter must be positive and valid for the profile.

## Scope

This is a constrained feature template, not a general-purpose sketch solver. It does not support arbitrary sketch entities, general coincidence/distance/angle solving, expressions, dependency graphs, feature reorder/suppression, arbitrary feature branches, or downstream parametric operations. The supported chain is one XY rectangle, one extrusion, one circular through-hole, and an optional X- or Y-direction linear hole pattern.

## Verification

Windows Release focused regression: **PASS, 113 assertions**. It checks origin-anchored and offset sketches, negative origin/hole coordinates, X/Y pattern placement and limits, containment, geometry volume, rebuild results, coordinated parameter edits, invalid-edit atomicity, schema-v19 persistence and schema-v18 migration, stale-reference invalidation, and undo/redo.

Live Windows UI verification: **PASS**. On a 40 × 30 × 20 mm extrusion, the UI created one 8 mm through-hole at (20, 15), then applied a Y-axis pattern with 2 instances and 8 mm center spacing. The viewport showed both holes and Properties reported 21,989.381 mm³, 10 faces, and 48 edges. The named-parameter editor displayed the pattern values, and the history view showed the constraints and all five ordered operations. The rebuilt executable launched with the correct `CADWorkbench v1.48.0` title.

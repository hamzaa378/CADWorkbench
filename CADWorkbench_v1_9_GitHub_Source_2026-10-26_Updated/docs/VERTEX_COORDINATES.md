# Selected vertex coordinates

Choose **Vertices (inspection)** in the Select menu, pick one vertex in the viewport, then run **Measure selected vertex coordinates** from Inspect. The report gives the picked point's X, Y, and Z coordinates in millimeters. The query does not edit the model.

The focused Windows regression passes 57 assertions over the box's full topological vertex traversal, including corner coordinates, bounds, invalid indices, missing bodies, null output, and stale-reference rejection after a transform. The Windows x64 Release app and tests built successfully; full CTest passed 37/37; the source audit passed 136 checks with all 112 public `SceneDocument` operations covered.

Live UI verification selected a corner of a 40 × 30 × 20 mm box and reported **X 0.000, Y 0.000, Z 20.000 mm**, matching the picked corner.

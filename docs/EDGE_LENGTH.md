# Selected edge length

Choose **Edges (inspection)** in the Select menu, pick one edge in the viewport, then run **Measure selected edge length** from Inspect. The report gives the selected edge's length in millimeters and its one-based topology index. The query does not edit the model.

The focused regression passes 38 assertions across a 10 × 20 × 30 mm box, including measurements from all 24 topological edge references, invalid index and missing body rejection, null output rejection, stale geometry token rejection after a transform, and unchanged document history. The Windows x64 Release app and tests built successfully; full CTest passed 37/37; the source audit passed 136 checks with all 112 public `SceneDocument` operations covered.

Live UI verification created a 40 × 30 × 20 mm box, selected an edge in the viewport, and reported **20.000 mm**, matching the box's Z extent.

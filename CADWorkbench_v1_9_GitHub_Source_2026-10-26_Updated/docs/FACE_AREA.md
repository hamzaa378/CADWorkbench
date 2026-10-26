# Selected face area

Choose **Faces (inspection)** in the Inspect ribbon, pick one face in the viewport, then use **Measure selected face area**. The report gives the selected face's area in square millimeters and its one-based topology index. The measurement does not edit the model.

The core regression passed 12 assertions. A 10 mm cube reports 100 mm² for a face; invalid and out-of-range indices, missing bodies, and null output are rejected. A face reference is tied to the body's geometry token, so a reference becomes invalid after the body changes; a fresh reference continues to work. The Windows x64 Release app and tests built successfully, full CTest passed 34/34, and the source audit passed 136 checks with all 109 public `SceneDocument` operations covered.

Live UI verification used a 40 × 30 × 20 mm box. Picking its top face and running the Inspect command returned **1,200.000 mm²**, matching the face's 40 × 30 mm dimensions.

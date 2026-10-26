# Distance between selected vertices

Choose **Vertices (inspection)** in the Select menu, select two vertices on the model, then run **Measure distance between vertices** from Inspect. The query reports the straight-line distance in millimeters and does not edit the model.

The focused Windows regression passes 12 assertions. Opposite corners of a 10 × 20 × 30 mm box measure √1400 mm; null output, missing bodies, and stale geometry references are rejected. The Windows x64 Release app and tests built successfully; full CTest passed 37/37; the source audit passed 136 checks with all 112 public `SceneDocument` operations covered.

Live UI confirmed the command appears in Inspect and correctly asks for exactly two picked vertices when fewer are selected. The numeric two-point viewport result remains pending: the available desktop automation input cannot issue Ctrl-click to retain a second subshape selection.

To complete the live numeric check manually, choose **Vertices (inspection)**, hold Ctrl while picking two opposite corners of the 40 × 30 × 20 mm test box, then run the Inspect command. The reported diagonal should be **53.852 mm**.

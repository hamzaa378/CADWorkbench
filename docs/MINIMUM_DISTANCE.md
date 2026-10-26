# Minimum distance inspection

CADWorkbench can measure the minimum Euclidean clearance between two scene bodies. The read-only core operation returns the distance in model units and a closest-point pair, one point on each body. The Inspect menu and Inspect ribbon command display the distance and both points.

Select exactly two bodies and choose **Inspect → Minimum distance between bodies**. The command requires Body selection mode. Intersecting or touching bodies report zero distance. The query does not alter geometry or document history.

The core regression covers two separated boxes with a 5 mm gap, intersecting boxes, face-touching boxes, vertically separated boxes, closest-point consistency, invalid IDs, duplicate IDs, null output, and unchanged document state. The focused Windows regression passed 23 assertions, and the current full Windows CTest suite passed 34/34.

Live Windows UI verification passed using two 10 × 12 × 14 mm boxes, with the second body translated 15 mm along X. The Inspect dialog reported 5.0000 mm and closest points (10, 0, 14) and (15, 0, 14) mm. The ribbon command was visible and the result was confirmed in the UI.

The result reports one valid closest-point pair. For shapes with multiple equally close points, the kernel may return any one of those pairs. This command measures distance only; it does not report interference volume or certify contact conditions.

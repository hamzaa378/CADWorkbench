# Interference volume inspection

CADWorkbench can compute the shared volume and centroid of the common region between two scene bodies as a read-only inspection. Select exactly two bodies in Body selection mode, then choose **Inspect → Interference volume between bodies** from the menu or ribbon. The result shows the shared volume in mm³ and, for positive-volume overlap, the common region’s centroid in model coordinates. Disjoint and face-touching solids report zero shared volume.

The focused Windows regression passed 27 assertions. It checks an exact 840 mm³ overlap and centroid, full containment (720 mm³) and centroid, disjoint and face-touching bodies, rejection of duplicate or missing IDs and null output, and unchanged document history. The current full Windows CTest suite passed 34/34. Source audit passed 136 checks, with all 109 public `SceneDocument` operations referenced by regression. A separate Windows x64 Release build linked the updated application and staged the Qt/Open CASCADE runtime under `build_resume/Release`.

The normal `build/Release` app could not be relinked because the existing process held that output file open (LNK1104); the isolated `build_resume/Release/CADWorkbench.exe` built and deployed successfully. Live Windows UI verification passed: two 10 × 12 × 14 mm boxes overlapped by 5 mm along X reported 840.0000 mm³ and centroid (7.5, 6, 7) mm. Moving them to face contact reported 0.0000 mm³. The contained-body case is covered by the core regression but was not run in the live UI.

## Deferred live UI check

The separated, overlap, and face-contact cases were run live:

1. Create two 10 × 12 × 14 mm boxes. Move the second box 15 mm along X, select both, and run **Inspect → Minimum distance between bodies**. Expected distance: 5 mm.
2. Move the second box 10 mm back along X and run **Inspect → Interference volume between bodies**. Expected shared volume: 840 mm³; centroid: (7.5, 6, 7) mm.
3. Create an 8 × 9 × 10 mm box and move it to (1, 1, 1) mm inside the first box. Expected volume: 720 mm³; centroid: (5, 5.5, 6) mm. This case remains unverified in the live UI.
4. Face-contact check returned zero shared volume live. Core regression confirms that inspection does not change document history.

This is a geometric overlap check only. It does not validate assembly fit, tolerances, material properties, or manufacturing suitability.

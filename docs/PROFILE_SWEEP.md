# Profile Sweep Tools

CADWorkbench provides two profile sweep commands from **Create > Sweep circular profile...** and **Create > Sweep rectangular profile...**. Both commands also appear in the Create ribbon's Profiles group.

Each tool creates a solid by carrying a closed profile along a smooth 3D interpolated centerline. The dialog controls profile size, path length, lateral endpoint offset, and vertical endpoint rise. The circular tool asks for radius; the rectangular tool asks for width and height. A successful operation adds one named body and is undoable in one step.

## Current scope

The path is generated from these four dimensions; users cannot yet draw or select an arbitrary sketch path, select a profile sketch, define twist/rotation, control guide rails, or add the sweep to an existing body as a join/cut. These are direct-solid operations and do not create a parametric feature timeline.

## Verification

Windows x64 Release build passed. The focused `cadcore_sweeps` test passed, covering invalid radius rejection without insertion, circular and rectangular sweep solid creation and valid topology, generated body names, and undo for both tools (11 assertions). The full native CTest suite also passed 4/4. In the live Windows application, both commands were confirmed in the Create ribbon and Create menu; their dialogs were not exercised in this pass.

# STEP Assembly Import/Export

CADWorkbench uses OCCT XDE for `.step` and `.stp` exchange. Use **File → Import STEP...** or **File → Export STEP...**. Assembly paths, product names, reusable component definitions, distinct occurrence identities, and fixed placements are retained when represented by the STEP file. Solid geometry is validated before import is committed, and an import is one undoable document operation. Files without product names or hierarchy use fallback names and a flat path.

The exchange is limited to supported solid and assembly data. PMI, joints, sketch constraints, non-solid reference geometry, appearance fidelity, and unsupported vendor-specific metadata are outside the current contract. Round-trip retention depends on what the input file actually contains.

## Verification

Windows x64 Release focused regression: **PASS, 28 assertions**. The test round-trips a two-solid assembly fixture and checks names, nested paths, repeated component-definition identity, unique occurrence IDs, placements, volume, atomic undo, malformed-input handling, and transactional scene behavior. Full v1.40 Windows CTest: **PASS, 50/50**.

Earlier live UI testing verified the import file picker. Live UI verification was not run for v1.40 by request; current v1.40 verification is the Windows build and automated regression suite.

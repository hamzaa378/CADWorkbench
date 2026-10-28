# Persistent assemblies, joints, and motion studies (v1.46)

## Reusable components and occurrences

Use **Assembly → Make selected body a component** to assign a persistent definition ID and first occurrence ID. **Insert component occurrence** creates another placed occurrence with its own body ID, occurrence ID, and XYZ translation. Occurrences persist in `.cadscene`, participate in undo/redo, and can be grouped in the BOM. STEP XDE import/export preserves names, reusable definitions, nested paths, occurrence identities, and placement when represented in the STEP file.

An occurrence initially contains a geometry copy. After editing the source geometry, use **Assembly → Synchronize selected component definition to its occurrences** to rebuild the other occurrences from that source while retaining each occurrence's XYZ placement. This is an explicit synchronization operation; it is not a live parametric link and it does not support different per-instance feature overrides.

## Joint tree and pose solving

A child occurrence can have one parent and one joint. The supported constraints are:

- **Rigid:** carries the child with the parent.
- **Revolute:** stores a world-space axis, origin, and degree limits; its drive rotates the complete descendant subassembly.
- **Prismatic:** stores a normalized axis and millimetre travel limits; its drive translates the complete descendant subassembly.

Use **Create rigid/revolute/prismatic joint** with the parent selected first and child second. The joint axis dialog sets the revolute or prismatic axis. Select a joint child and use the corresponding **Drive** command to move it within its stored limits. Parent transforms update nested joint frames and carry descendants. Invalid references, cycles, duplicate child joints, degenerate axes, non-finite limits, and out-of-range drive targets are rejected before mutation.

`solveAssemblyPose` accepts a map of revolute angle targets (degrees) and prismatic offsets (mm), orders them parent-first, and applies the resulting pose as one undoable transaction. It rejects the complete pose if any target is invalid. This bounded tree solver does not solve closed kinematic loops or over-constrained systems.

## Motion studies

Use **Edit selected joint motion track** to store 2–200 time/value keyframes on one revolute or prismatic joint. Times must increase strictly and stay within 0–3,600 seconds; values must stay within the joint's limits. **Evaluate assembly motion study at time** linearly interpolates each saved track and sends the combined targets to the parent-first pose solver. Keyframes persist with the joint record and edits are undoable.

This provides deterministic frame evaluation, not a real-time playback timeline, collision study, motor model, or acceleration/velocity solver. Angles use degrees and slider motion uses millimetres.

## Scope and verification

The CAD body geometry receives full OCCT rigid transformations during joint motion. Occurrence placement fields persist XYZ translation; orientation is currently represented in the transformed B-rep rather than a separate editable 4×4 transform. Reusable definitions have stable shared IDs and explicit source-to-occurrence synchronization, but not live shared feature history. Assemblies reject cycles and are represented as parent-child trees.

The assembly-joint regression passes **133 assertions** covering rigid/revolute/prismatic behavior, descendant motion, combined parent-first pose solving, linear keyframe interpolation, persistence, limits, atomic undo/redo, malformed references, and explicit definition synchronization. Scene schema v17 stores prismatic limits and motion tracks; schema v18 adds the optional parametric hole-pattern record, and schema v19 extends it with X/Y pattern direction while migrating schema-v18 patterns as X. Schemas v1-v18 remain readable. The full Windows Release build and regression results for this release are recorded in [`QA_MATRIX.md`](QA_MATRIX.md) and [`local_verification.json`](local_verification.json). Live UI verification was excluded from the current work request.

## Related workflows

- [`BOM_PARTS_LIST.md`](BOM_PARTS_LIST.md) describes grouped component quantities and occurrence data.
- [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) describes drawing source IDs and revision status.
- [`STEP_IMPORT.md`](STEP_IMPORT.md) describes neutral-file assembly exchange.

# Persistent assemblies, joints, and motion studies (v1.46)

## Reusable components and occurrences

Use **Assembly → Make selected body a component** to assign a persistent definition ID and first occurrence ID. **Insert component occurrence** creates another placed occurrence with its own body ID, occurrence ID, and XYZ translation. Occurrences persist in `.cadscene`, participate in undo/redo, and can be grouped in the BOM. STEP XDE import/export preserves names, reusable definitions, nested paths, occurrence identities, and placement when represented in the STEP file.

An occurrence initially contains a geometry copy. After editing the source, use **Assembly → Synchronize selected component definition to its occurrences** to rebuild the other occurrences from that source while retaining each occurrence's XYZ placement and local visibility. Synchronization also copies shared BOM and material fields (part number, description, revision, assembly path, procurement type, supplier data, assigned material, density, and engineering provenance/modulus) and definition color. The update is one undoable transaction. This is an explicit synchronization operation; it is not a live parametric link and it does not support different per-instance feature overrides.

New occurrences created through **Insert component occurrence** or **Duplicate** inherit their source component’s part number, description, revision, assembly path, procurement type, supplier, and supplier part number. This keeps newly created instances under the same grouped BOM identity unless their metadata or geometry is subsequently changed. Existing occurrences receive the latest shared data when the explicit synchronization command runs.

## Linear and circular occurrence patterns

Use **Assembly → Linear pattern of component occurrences...** to create 2–100 total placements from one selected free component occurrence and an XYZ spacing vector. Use **Circular Z-axis pattern of component occurrences...** for 2–100 total placements around a specified world-space center and world-Z angle step. Both commands keep the source as the first instance, preserve shared component/BOM/material identity, generate distinct occurrence IDs, and create all additional occurrences as one undoable transaction. Patterns are persisted as ordinary placed occurrences, so they remain editable with existing occurrence placement and BOM tools.

The first implementation is intentionally bounded: patterns are limited to 100 total instances, circular orientation rotates about world Z, and a source with a joint or joint descendants must be patterned after it is removed from that joint tree. These commands do not create a live associative pattern feature that recomputes when its source geometry changes.

**BOM grouping boundary:** Component and part-number BOM grouping keys now length-prefix each metadata field. Because part numbers, assembly paths, descriptions, and supplier fields may contain `|`, the former delimiter-joined representation could conflate different identities. The updated encoding keeps distinct identities separate without changing the BOM file format or how identical parts aggregate.

**Large-assembly BOM:** The full-document BOM and its CSV export now support all bodies in a scene (up to the scene object limit). The separate source-scoped BOM retains its explicit 1–50 selected-body limit. Previously, the full-document overload accidentally passed through that selection limit and returned an empty BOM whenever a scene had 51 or more bodies.

**Bounded rotational placement (introduced in schema v19):** `setFreeOccurrenceYaw(bodyId, degrees)` stores an absolute world-Z yaw in `placement_yaw_degrees` for a free component occurrence, about its saved XYZ pivot. Insertion from a yawed source strips its placement; duplication retains the yaw; explicit definition synchronization preserves each target's XYZ and yaw. The field is optional, and older v19 scenes without it load as 0 degrees. Invalid, non-numeric or out-of-range yaw is rejected during load. The operation is transactional and undoable. It rejects jointed occurrences and occurrences with joint descendants. Existing world-Z rotations compose into a recorded yaw; world-X/Y and arbitrary-axis rotations on already yawed occurrences are rejected before mutation. This does not represent X/Y rotations, other pre-existing manual rotations, or the revolute/prismatic solver's motion; the existing pose guards remain in force. STEP orientation interchange is not extended by this patch. No general 6-DOF transform or assembly hierarchy is claimed.

## Joint tree and pose solving

A child occurrence can have one parent and one joint. The supported constraints are:

- **Rigid:** carries the child with the parent.
- **Revolute:** stores a world-space axis, origin, and degree limits; its drive rotates the complete descendant subassembly.
- **Prismatic:** stores a normalized axis and millimetre travel limits; its drive translates the complete descendant subassembly.

Use **Create rigid/revolute/prismatic joint** with the parent selected first and child second. The joint axis dialog sets the revolute or prismatic axis. Select a joint child and use the corresponding **Drive** command to move it within its stored limits. Parent transforms update nested joint frames and carry descendants. Invalid references, cycles, duplicate child joints, degenerate axes, non-finite limits, and out-of-range drive targets are rejected before mutation. New revolute joints always start at 0°, so their angle limits must include 0° (an inclusive endpoint is allowed); this is also required to reopen the saved joint. The scene loader rejects joint records whose child is not a component occurrence, preserving the same ownership requirement as joint creation.

`solveAssemblyPose` accepts a map of revolute angle targets (degrees) and prismatic offsets (mm), orders them parent-first, and applies the resulting pose as one undoable transaction. It rejects the complete pose if any target is invalid. This bounded tree solver does not solve closed kinematic loops or over-constrained systems.

## Motion studies

Use **Edit selected joint motion track** to store 2–200 time/value keyframes on one revolute or prismatic joint. Times must increase strictly and stay within 0–3,600 seconds; values must stay within the joint's limits. **Evaluate assembly motion study at time** linearly interpolates each saved track and sends the combined targets to the parent-first pose solver. Keyframes persist with the joint record and edits are undoable.

This provides deterministic frame evaluation, not a real-time playback timeline, collision study, motor model, or acceleration/velocity solver. Angles use degrees and slider motion uses millimetres.

## Scope and verification

Joint-driven revolute and prismatic motion applies OCCT transformations to the occurrence geometry. Free component occurrences also store a bounded, editable world-Z yaw about their saved XYZ pivot. That optional field was introduced in schema v19 and remains readable in schema v20. It does not provide a general 6-DOF transform, X/Y placement rotations, arbitrary per-instance feature history, or STEP orientation interchange. Reusable definitions use explicit synchronization rather than a live shared feature history; assemblies remain parent-child trees and reject cycles.

The assembly-joint regression passes **133 assertions** covering rigid/revolute/prismatic behavior, descendant motion, combined parent-first pose solving, linear keyframe interpolation, persistence, limits, atomic undo/redo, malformed references, and explicit definition synchronization. Scene schema v17 stores prismatic limits and motion tracks; schema v18 adds the optional parametric hole-pattern record, and schema v19 extends it with X/Y pattern direction while migrating schema-v18 patterns as X. Schemas v1-v18 remain readable. The full Windows Release build and regression results for this release are recorded in [`QA_MATRIX.md`](QA_MATRIX.md) and [`local_verification.json`](local_verification.json). Live UI verification was excluded from the current work request.

## Related workflows

- [`BOM_PARTS_LIST.md`](BOM_PARTS_LIST.md) describes grouped component quantities and occurrence data.
- [`ORTHOGRAPHIC_DRAWINGS.md`](ORTHOGRAPHIC_DRAWINGS.md) describes drawing source IDs and revision status.
- [`STEP_IMPORT.md`](STEP_IMPORT.md) describes neutral-file assembly exchange.

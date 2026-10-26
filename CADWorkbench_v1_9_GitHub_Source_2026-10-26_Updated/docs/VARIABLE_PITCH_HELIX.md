# Variable-pitch spring

The variable-pitch spring command creates a solid circular wire swept along a helix whose pitch changes smoothly from the first turn to the last.

## Controls

Open **Create > Variable-pitch spring...** or select **Variable-pitch spring** in the Create ribbon. The command accepts wire radius, coil radius, start pitch, end pitch, and turn count (0.25–20). Dimensions must be positive; the coil radius must exceed the wire radius; and both endpoint pitches must exceed the wire diameter to prevent turn overlap. The result is one undoable body named **Variable Pitch Helix**.

The centerline's vertical position follows a quadratic interpolation of the start and end pitch values, so local pitch varies continuously along the coil.

## Verification (Windows x64 Release, 2026-10-25)

- Release build: **PASS** for the application and core test targets.
- Focused regression: **PASS, 10 assertions**, covering too-small start/end pitch, overlapping radius, turn limit, valid solid topology, body insertion/name, undo, and redo.
- Full CTest: **PASS, 10/10 tests**.
- Live app workflow: **PASS**. The spring dialog opened with wire radius, coil radius, start pitch, end pitch, and turn-count controls; accepting defaults created a visible **Variable Pitch Helix** body.

## Scope

This command generates a circular wire with a fixed coil radius and a smoothly varying pitch. It does not create editable feature-history nodes, variable coil radius, closed spring ends, or validate manufacturing performance.

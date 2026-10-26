# Tapered spring

Create a conical spring by sweeping a circular wire around a helix whose coil radius changes linearly from the start radius to the end radius.

## Controls

Open **Create > Tapered spring...** or select it from the Create ribbon. Set wire radius, start coil radius, end coil radius, pitch, and turn count (0.25–20). Dimensions must be positive; both coil radii must exceed the wire radius; pitch must exceed the wire diameter. The resulting **Tapered Helix** is added as one undoable body.

## Verification (Windows x64 Release, 2026-10-25)

- Release build: **PASS**.
- Focused regression: **PASS, 10 assertions** covering invalid start/end radii, too-small pitch, turn limit, valid topology, body insertion/name, undo, and redo.
- Full CTest: **PASS, 10/10 tests**.
- Live menu/dialog workflow: **PASS**. The spring dialog opened with wire radius, start/end coil radius, pitch, and turn-count controls; accepting defaults created a visible **Tapered Helix** body.

## Scope

The coil radius changes linearly while pitch and wire radius remain constant. The command does not provide variable pitch, closed spring ends, editable feature history, or manufacturing validation.

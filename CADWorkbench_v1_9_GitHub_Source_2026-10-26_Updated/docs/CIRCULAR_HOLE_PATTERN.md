# Circular hole pattern

Apply repeated cylindrical cuts to a selected solid around a bolt circle. The command creates every hole first, validates each resulting shape, and commits the combined cut as one undoable document change.

Open **Modify > Circular hole pattern...** or use the matching Modify ribbon command. Set the pattern center X/Y, entry Z, bolt-circle radius, hole diameter, instance count (2–1000), pattern angle (1–360 degrees), and either through-all or blind depth. Full circles distribute instances evenly without duplicating the start point; partial arcs include both endpoints.

## Verification (Windows x64 Release, 2026-10-25)

- Focused core regression: **PASS, 15 assertions** covering invalid input, missing source, valid four-hole cut topology, volume reduction, atomic rollback, and one-step undo/redo.
- Full CTest: **PASS, 10/10 tests**.
- Live UI workflow: **PASS**. The dialog exposed center, entry height, bolt-circle, diameter, depth, instance-count, angle, and through-all controls. On a selected elliptical prism, a four-hole through-all pattern at a 8 mm bolt-circle radius reduced reported volume from 16,964.600 mm³ to 16,116.370 mm³; faces and edges changed from 3/6 to 7/30.

The live cut was checked on the source solid. Editing an already-patterned result or selecting non-planar entry surfaces was not part of this check.

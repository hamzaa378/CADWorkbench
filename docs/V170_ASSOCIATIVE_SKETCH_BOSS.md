# CADWorkbench v1.70: Associative cross-body sketch bosses

## Implemented

**Create → Associative boss from another body's sketch...** builds a hidden sketch-linked extrusion (the additive tool) and a visible linked **Fuse** result. Both are committed in one undo checkpoint. The source constrained sketch and original target body are preserved, as are stable body IDs. The boss is a **separate derived result body**, not yet an in-place modifier in the target body's feature tree.

The tool has independent editable depth and sketch-normal offset. Changing the source sketch, retargeting the linked tool to a different current sketch, or editing the tool dimensions rebuilds the tool and its Boolean descendants using the existing cross-body DAG. Undo/redo and `.cadscene` v24 persistence use the established `linked_sketch_extrusion` and `linked_boolean` records; there is **no file-format migration**.

## Geometry safety conditions

- Positive, finite depth with bounded normal offset; a live, saved constrained polygon sketch and an existing solid target are required.
- The fused result must have **exactly one solid component**. OpenCascade can return a valid compound containing separate solids for a disjoint union; that outcome is rejected as a *detached boss*.
- The fused result must increase target volume by more than a numeric tolerance. A fully buried cutter (or any other non-additive operation) fails rather than being mistaken for a successful boss.
- These guards run at creation, when a source sketch or tool changes, when an existing linked Boolean is retargeted to Fuse with a sketch-linked tool, and on loading persisted source-linked Fuse geometry. An unsuccessful operation leaves the previously committed model unchanged.
- Related existing sketch-driven **Cut** guards remain in place: a pocket must continue to remove measurable volume.

## Workflow

1. Create a **constrained polygon extrusion** with a current editable sketch.
2. Create a solid target. The sketch plane and selected normal offset must place the prospective additive tool so it overlaps or shares a suitable face with the target.
3. Choose **Create → Associative boss from another body's sketch...**, select both bodies, set depth and offset. A hidden tool and a visible linked Fuse result appear.
4. Edit the sketch or the hidden tool using the existing sketch and **Edit linked sketch source, depth and offset...** controls. The visible Fuse result updates transactionally.
5. To make the rendered scene unambiguous, hide the original target body if it overlaps the visible derived result.

## Scope and limitations

- Only current saved **constrained polygon** sketches can drive this workflow. It does not introduce curved-sketch support, automatically constrained profiles or face-lineage naming.
- The connectedness gate requires **one solid**; legitimate intended multiple-solid unions are not accepted as *associative bosses*. For a multi-solid operation, use other modeling tools.
- Tool placement is measured in the source sketch plane's normal direction; automatic face termination, draft angles, fillets, and boss merging into an existing body's feature tree remain to be implemented.
- Existing generic linked Booleans with a **sketch-derived Fuse tool** also receive the connectedness/additive guard on future edits. A previously saved detached sketch Fuse might now be rejected on load; retain a copy of older scene files before upgrading.

## Checks

Added two native CTests to the existing executable: `cadcore_v170_associative_sketch_boss` and `cadcore_v170_associative_sketch_boss_persistence`. They exercise valid additions, invalid/disjoint/buried tools, transitive sketch updates, rollback, one-step undo/redo, dependency inspection, source deletion protection and save/reopen.

The optional `tests/v170_sketch_boss_kernel_reference.py` checks the expected volumes and component counts independently using CadQuery/OpenCascade, but it is **not** a substitute for compiling the native Windows application.

**Windows acceptance gate: 111 registered CTests.** In the extracted project root:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 111
```

Windows MSVC/Qt compilation and native CTest execution remain unverified until a result from the actual Windows environment is provided.

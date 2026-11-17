# CADWorkbench v1.70 development increment — ordered in-place sketch modifiers

## Implemented scope

The previous source allowed a first in-place boss/pocket and a separate, optional second stage. This increment retains those fields for existing projects and introduces an **ordered chain of up to eight** source-linked sketch boss/pocket modifiers on an ordinary, eligible target body. All stages operate on the output of the preceding stage, preserve the target's body ID, and retain the target's frozen base B-rep for deterministic recomputation. Each stage must reference a distinct live constrained polygon sketch from another body. Tools are normal extrusions along their source sketch plane normals, with explicit depth and normal offset.

The third through eighth stages are represented by `Part::AdditionalInPlaceSketchStage` entries with source ID, source geometry revision, operation, depth and offset. They are exposed in the existing in-place sketch feature dialog through **Append next modifier (stages 3-8)**, **Edit stage 3-8**, and **Remove last stage 3-8**. Stages are removed in reverse order to protect chain dependencies. The existing stage-one and stage-two UI and APIs remain supported.

`LinkedBodyGraph` now admits extra dependency inputs for an in-place body, rejecting missing, duplicate and circular references. All three source-to-graph adaptation paths (regeneration, impact preview and atomic batch preview) include the extra inputs. The target is rebuilt in stage order on any source change, and downstream linked Boolean bodies rebuild through the same topological graph. Invalid geometry and edits fail before the staged scene is committed or an undo checkpoint is taken. Save/reopen checks all sources, revision tokens, Boolean outputs, reconstructed volume and bounding coordinates. Source deletion remains prohibited when a live dependent needs it.

## Compatibility correction

The old scene loader incorrectly required exactly six JSON fields in every `in_place_sketch_feature` record, although its existing second-stage writer adds a seventh `follow_up` field. The loader now enforces exactly the six mandatory fields plus the optional `follow_up` and `extra_stages` keys, validates all nested fields and rejects malformed or duplicate stage references. Existing first- and second-stage v24 scene shapes remain the same. The additive `extra_stages` field is understood only by this development increment; **older builds may discard it or fail to load the scene**. Keep a separate backup when opening new-format projects in older software.

## New regression coverage

Four new native CTest entries bring the registered Windows total to 123:

1. `cadcore_v170_inplace_ordered_stack`: four-stage geometry, source invalidation, undo/redo and LIFO removal.
2. `cadcore_v170_inplace_ordered_stack_persistence`: save/reopen and update of a third-stage sketch source.
3. `cadcore_v170_inplace_ordered_stack_corruption`: reject a tampered saved stage while leaving an open scene intact.
4. `cadcore_v170_inplace_ordered_stack_rollback`: reject duplicate sources, detached additive tools, and removal of stage two when later stages remain.

The standalone linked-body DAG test adds checks for arbitrary additional sources and invalid references. An independent CadQuery/OpenCascade geometry oracle validates the four-stage volumes. Source preflight tests verify connection and registration. Windows MSVC compilation and native CTest execution remain **pending**, not passed.

## Boundaries and remaining v1.70 scope

This is an eight-stage **external constrained-polygon sketch** modifier stack, not an arbitrary general feature history. The ordinary internal feature tree, curve-constrained sketches, full topological lineage through arbitrary splits/merges, general cross-component closed-loop mate solving, manufacturing CAM toolpaths and complete associative drawings remain partially implemented or outstanding. The implementation retains v24 scene schema but extends its optional metadata, so older releases are not guaranteed to round-trip it.

Run the Windows gate from the extracted source root:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 123
```

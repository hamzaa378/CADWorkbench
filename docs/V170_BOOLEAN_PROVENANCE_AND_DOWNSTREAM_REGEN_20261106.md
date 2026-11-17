# CADWorkbench v1.70 engineering increment: Boolean topology provenance and downstream regeneration

**Status:** source-only development candidate. Windows MSVC, Qt6 and OCCT 8.0.1
build + CTest + physical UI acceptance have **not** been run. Preserve the last
user-confirmed Windows build as the release baseline.

## 1. Provenance-aware Boolean references

`src/core/BooleanReferenceLineage.cpp` uses the **current** OpenCascade
`BRepAlgoAPI_BooleanOperation` builder's `Modified`, `Generated` and
`IsDeleted` records to identify candidate descendant edges or planar faces.
Candidates must occur in the final output shape. Unique successors are
reanchored; split/merge results require a uniquely valid interior witness.
Missing successors, removed face witness regions, ambiguous splits, invalid
anchor parameters and unsupported feature types are rejected, without
substituting an approximately similar edge or face.

Supported follow-up modifiers include selected-edge fillet/chamfer and
planar face normal/rectangle pocket/boss, face-attached polygon
boss/pocket, open-face shell and draft. A selected edge needs a witnessed
v2 line/circular anchor; a selected face needs an interior planar witness.
Nonplanar face features and B-spline edge anchoring are not supported by
this provenance-rebase interface.

**Face-relative engineering placement:** when the Boolean changes a planar
face centroid but leaves its original trimmed witness and design plane,
`u/v` offsets are adjusted to keep a hole or boss at its original physical
in-plane location. Incompatible orientation or plane changes fail closed.

## 2. Associative downstream regeneration

`InterleavedHistoryStep::referenceSourceId` is an optional stable ID for an
anchored Shape modifier. To use it:

- Set `step.inputs={"boolean-stage-id"}` on the downstream Shape step.
- Set `step.referenceSourceId` to `@base` or the exact direct predecessor
  ID of that Boolean stage. For sketch-driven Boolean steps, it must be the
  *target-solid operand*, not the external sketch body's ID.
- Capture the selected edge/planar face anchor on that **source branch**,
  not on the resulting Boolean solid, and include the numeric anchor fields
  in the Shape modifier parameters.

While replaying the history, the target body records only the live kernel
builders needed by downstream nodes. It resolves the saved anchor against
the named input shape, proves its successor against that Boolean's actual
history, captures a current result anchor, and rebuilds the downstream
modifier with this **temporary** anchor. The persisted source anchor is not
rewritten during regeneration. Later sketch source edits, undo/redo,
Save/Reopen and cross-body dependency propagation preserve the source link.
A failed edit or a corrupted file is rejected before committing to the
open scene.

This version handles **one direct Boolean boundary** per referenced modifier.
It does not claim ancestry propagation through arbitrary chains of
Boolean/transform/draft operations, multiple split descendants without a
unique witness, arbitrary B-splines, or cross-document identity mapping.
There is no incremental per-step cache for mixed history; the whole mixed
DAG is replayed before downstream bodies regenerate.

The persisted record is `dag-interleaved-history-v3` for histories containing
`reference_source`; older `linear-interleaved-history-v1` and
`dag-interleaved-history-v2` records remain readable. The outer project
schema is unchanged (v24). New files cannot be opened with older binaries.

## 3. Authoring in the application

From **Edit → Mixed associative feature history (branches + sketches)**:

1. Create a live SketchFuse/SketchCut stage that consumes the frozen base,
   or a two-input Boolean that explicitly uses the frozen base.
2. Add `Fillet selected edge` or `Chamfer selected edge`.
3. Select the Boolean as **First parent** and choose an edge from the
   **Frozen-base edge** list, labeled with a center and length in mm.
4. Set the radius/distance and save. If a later sketch edit removes the
   target edge or makes its lineage ambiguous, the edit is rejected and
   the previous solid remains unchanged.

Other supported reference types and source branches are available through
the `SceneDocument::setInterleavedFeatureHistory` API; the compact UI only
supports witnessed selected edges sourced directly from `@base`. It does
not yet provide click-to-pick selections on a historic feature result.

## Verification ledger

- Python unittest source contracts and independent geometry checks:
  **206 passed**, including 5 new OCCT Boolean history cases. Not native C++.
- Portable `FeatureDependencyPlan.cpp` built and executed using GCC 14.2 and
  Clang 17 with ASan/UBSan: **347 assertions each**. This does not compile
  BooleanReferenceLineage.cpp, SceneDocument.cpp or the Qt GUI.
- Native C++ integration assertions were added to the existing
  `face_lineage_v170_tests` and `interleaved_history_v170_tests` executables:
  actual Modified/Generated ancestry, fillet after Cut, preserved hole
  location, removed face/edge witness, live sketch source edits,
  save/reopen, invalid branch rollback and tampered document rejection.
  These tests are **registered but not yet run** in the Windows suite.
- Exactly **130 registered CTests** remain. No target or release-gate count
  was removed.

On Windows PowerShell in the extracted project root:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Use a scratch document to edit sketches and Boolean-linked fillets/chamfers,
then save/reopen. Only promote after the native build, all CTests and a
manual Qt geometry/UI pass succeed.

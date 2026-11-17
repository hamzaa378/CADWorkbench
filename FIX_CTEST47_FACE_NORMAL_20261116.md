# CADWorkbench v1.70 — Windows native CTest #47 face-selection correction

## Reported failure

- Windows application and all test binaries compile successfully.
- Python/source tests: 722/722 passed before the fix.
- Native CTest #47 (`cadcore_associative_features_v159`) fails with
  `FAIL: Face normal did not point into the solid`.
- The other 129 native CTests pass.

## Root cause

`tests/associative_features_v159_tests.cpp` selected a planar box face
using a variable named `outward`, but reversed the analytic surface normal
for `TopAbs_FORWARD` instead of `TopAbs_REVERSED`. That formula actually
computed the *inward* normal. Thus the loop selected the negative-X face
while a subsequent assertion demanded a negative-X *inward* direction,
which belongs to the positive-X face.

The production implementation `SceneDocument::prepareHoleOnPlanarFace` was
not altered: its inward-direction logic is consistent. No assertions were
removed, skipped, or weakened. The test now chooses the physical +X face,
and still checks the cutter direction, material volume, regeneration, undo
and redo.

## Verification performed in the development environment

- Reproduced OpenCascade box face orientations independently in Python OCP.
- Ran the entire Python/source suite, including three new regression tests.
- Validated hash manifests and source package after extraction.
- Windows MSVC compilation and native CTest #47 still require execution
  on the user's Windows installation.

## Upgrade (from exact previous PersistentGeometry_MAJOR ZIP only)

Extract `CADWorkbench_v170_CTest47_FaceNormal_HOTFIX_20261116.zip`
to a new folder. Close CADWorkbench first. From PowerShell:

```powershell
$kit = "$env:USERPROFILE\Downloads\CADWorkbench_v170_CTest47_FaceNormal_HOTFIX_20261116"
$project = "$env:USERPROFILE\Downloads\CADWorkbench_PersistentGeometry_MAJOR_Test\CADWorkbench_v170_PersistentGeometry_MAJOR_PRELIMINARY_20261116"
powershell -NoProfile -ExecutionPolicy Bypass -File "$kit\APPLY_CTEST47_FACE_NORMAL_FIX.ps1" -ProjectRoot $project
```

Then run the included `RUN_CTEST47_RETEST.ps1 -ProjectRoot $project` to
rebuild the native target, run CTest #47, and finally run the full 130-test
and Python regression audit. This does not mark manual acceptance as passed.

Backups are created in a timestamped directory next to the project root.
If the source file has already been changed, the guarded upgrade refuses to
proceed rather than overwriting custom work.

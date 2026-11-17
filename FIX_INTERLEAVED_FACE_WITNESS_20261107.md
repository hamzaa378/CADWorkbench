# CADWorkbench v1.70 — interleaved face-machining regression fixture repair

**Status:** corrected source candidate. **Not Windows-native verified** in this environment. Preserve the previous Windows executable until the 130 native CTests and GUI smoke checks succeed.

## Failure reproduced from Windows log

`cadcore_v170_interleaved_history` aborts at `face machining produces a valid solid` when the test constructs a `Face normal pocket` and asks `rebuildShapeFeatureTree()` to record kernel provenance.

## Root cause in the test fixture

`capturePlanarFaceAnchor()` records a primary interior witness (`face_anchor_witness_{x,y,z}`) and, for the ordinary box top face, a secondary witness (`face_anchor_witness2_{x,y,z}`). The test deleted only the primary group and left the secondary group. The production validator deliberately rejects `(hasFaceWitness2 && !hasFaceWitness)` before attempting the Boolean. Thus the failure message was misleading: the test did not submit a valid face feature.

## Changes

- `tests/interleaved_history_v170_tests.cpp`: assert that the fixture has a secondary witness, verify the orphaned-secondary case is rejected, remove all three secondary witness fields to form a valid legacy anchor, verify feature validation succeeds, then run the original kernel-history replay checks. Include the actual `QString` error in the failure diagnostic if a later rebuild fails.
- `tests/v170_machining_provenance_contract.py`: two additional regression contracts ensure that future edits do not reintroduce the orphan and that production validation remains fail-closed.
- `tests/v170_machining_provenance_reference.py`: independent OpenCascade geometry check for the exact 12×12×10 mm box and radius-0.6, depth-2.0 pocket fixture.
- No production CAD geometry code, CMake targets, or existing CTest registrations were changed.

## Verification performed

- `python -m unittest discover -s tests -p '*.py'`: **342 tests passed** (including independent OpenCascade/Python reference geometry).
- ZIP source manifest and archive integrity checks: performed at packaging.
- Actual Windows MSVC compilation, CTest execution, UI test: **not available in this environment**. The preceding user-supplied Windows log had 129/130 tests passed before this fixture repair.

## Test on Windows

Extract the new ZIP to a **new** folder, open PowerShell in that folder, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

This rebuilds and runs all **130 native CTests**. For a quick focused rerun after the new build finishes:

```powershell
$ctest='C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $ctest --test-dir .\build -C Release -R '^cadcore_v170_interleaved_history$' --output-on-failure --verbose
```

Do not assume 130/130 passes until the output actually confirms it. If a different failure emerges after this fixture repair, the test now prints the underlying geometry validation error.

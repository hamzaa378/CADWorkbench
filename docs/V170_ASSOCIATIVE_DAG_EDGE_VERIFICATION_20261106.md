# v1.70 associative DAG / topology reference verification ledger

Source state: November 6, 2026. Native Windows build and UI acceptance pending.

| Verification | Result | Meaning |
|---|---|---|
| Full Python `unittest discover -s tests -p '*.py' -q` | PASS: 197 tests | Source contracts and independent reference calculations; these are not C++ Qt executions |
| GCC 14.2, C++20, `-Wall -Wextra -pedantic -fsanitize=address,undefined` | PASS: 347 portable feature DAG planner assertions | Compiled and executed *FeatureDependencyPlan.cpp*, not SceneDocument/ShapeFeatureTree |
| Clang, C++20, same sanitizer flags | PASS: 347 portable feature DAG planner assertions | Independent portable compiler check |
| OpenCascade Python binding reference | PASS: 3 new geometry cases | Projection on a split straight edge, splitting at witness, circle trimming, translation and ambiguity model |
| Registered native CTest entries | 130 distinct (retained) | Native CTest suite has **not been run** in this environment |
| Linux CMake configure | Expected Windows-only guard stopped configuration | Not a Windows build or meaningful MSVC validation |
| Windows MSVC/Qt6/OpenCascade application compile, full native tests | **Not run** | Required before installing as a new release |
| Visual Qt authoring / save-reopen acceptance | **Not run** | Required for interactive CAD use |

Native additions (registered in existing CTest executables):
`tests/interleaved_history_v170_tests.cpp` and `tests/face_lineage_v170_tests.cpp`.
UI coverage: `tests/v170_associative_dag_edge_preflight.py` asserts the Edit menu path,
source choices, parent controls, and atomic Save call; this is **static** coverage only.

Required PowerShell acceptance in the extracted project directory:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

If the build fails or any CTest fails, keep the last native verified baseline.
The source increment should not be designated a production release on the basis
of reference tests alone.

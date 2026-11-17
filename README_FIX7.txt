CADWorkbench v1.70 - interleaved-history selective-fillet ancestry correction

TARGET: existing CADWorkbench_v170_Consolidated_Native_Fix3_Complete_Source_20261106
with Fix4 + Fix5 + Fix6 overlays already applied.

REPLACE (extract into the project root, overwriting existing files):
  src/core/SceneDocument.cpp
  tests/interleaved_history_v170_tests.cpp
  SHA256SUMS.txt
  SOURCE_MANIFEST.sha256
  SOURCE_MANIFEST_SHA256.txt

ADDED CHECK:
  tests/v170_exact_modifier_priority_contract.py

Root-cause correction: after a fillet/chamfer that has kernel history, first try
tracking an exactly surviving source edge or face in the output. Only when that
strict identity lookup fails, consult actual recorded kernel descendants.
Never select an edge by old numeric index alone. Both paths require saved design
witness checks; consumed edges and ambiguous topology remain rejected.

The native regression now isolates a first-fillet/sketch issue from retargeting
and prints detailed error text if the three-stage transaction fails.

PowerShell from the project root, AFTER extraction:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_PREVIOUS_FAILURES.ps1 -VcpkgRoot C:\vcpkg -OnlyInterleavedHistory

If that passes, run:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg

Do not delete the build directory. Build scripts already use one job.
The previous Windows log builds CADWorkbench.exe successfully. This particular
source correction has not been compiled with MSVC/Qt/OpenCascade on Windows, so
native test success remains unverified.

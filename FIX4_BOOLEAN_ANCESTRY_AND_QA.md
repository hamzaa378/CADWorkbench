# CADWorkbench v1.70 — Boolean ancestry Fix 4

## Baseline and observed failure

This consolidated source tree is based on `CADWorkbench_v170_Consolidated_Native_Fix3_Complete_Source_20261106.zip`.
The user's six-test Windows retest passed five cases. Only
`cadcore_v170_interleaved_history` failed on the condition:
`disagreeing Boolean ancestry routes must reject tracked references`.

## Root cause and implementation

A Boolean diamond can merge two source branches into *the same final edge* even when one
branch has translated the original machining attachment point. The old resolver compared
re-captured descriptors after each path; those descriptors can converge and erase the
identity of the original physical design location.

The revised runtime now preserves the source edge/face witness independently of
regenerated topology descriptors. Along each ancestry route, supported rigid
transformations and selected pattern-instance transformations move that original witness
using the same transformations as the CAD kernel's feature builder. Every Boolean route
must identify the same final geometry **and** preserve the same transformed physical
witness (0.0001 mm agreement). Missing witnesses on multi-route merges are rejected.
The original source descriptor is never mutated during this comparison.

The selected pattern transform calculation is now shared by witness transport and
ordinary reference rebasing, preventing duplicated formulas from diverging. Errors
identify a conflicting merge and the disagreeing route. A failed candidate rebuild
retains the last committed body, as checked by native regression source.

## Additional verification improvements

- The interleaved history regression includes a failed-merge save/reopen check.
- The portable Boolean planner tests now check equal, displaced, non-finite, and
  tolerance-boundary witness comparisons.
- `RUN_PREVIOUS_FAILURES.ps1` accepts `-OnlyInterleavedHistory` to rebuild and run
  the single previously failing native test, while still verifying all source hashes.
- The full 130-test registration gate is not altered; the test count remains 130.

## Windows commands

From the extracted source root in PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_PREVIOUS_FAILURES.ps1 -VcpkgRoot C:\vcpkg -OnlyInterleavedHistory
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_PREVIOUS_FAILURES.ps1 -VcpkgRoot C:\vcpkg
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

## Verification boundary

Python source/independent geometry suites and portable C++ GCC/Clang checks pass in
this environment. This environment cannot compile the Windows-only Qt/OpenCascade
application. **Neither the single failing CTest nor the full 130 native CTests
has been executed against Fix 4 here.** The Windows logs from Fix 3 do not
constitute verification of Fix 4.

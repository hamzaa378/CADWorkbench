# Sprint 16 MSVC repair: Sprint 15 error logs and combined fix

**Candidate:** Sprint 16 Integrated Readiness, amended 2026-11-18.
**Scope:** targeted source compile repair + regression prevention; **NOT** a native Windows verification.

## Evidence and root cause

The two uploaded Sprint 15 PowerShell logs (`Pasted text(20261117-183844).txt` and `Pasted text(20261117-184225).txt`) are independent copies of the same early `cadcore` MSVC build failure, not separate bugs. The first root error was `src/core/SceneDocument.cpp` around line 1603:

- C2065/C2653: `detail`/`ContourPoint` unknown.
- C2065/C2653: `refold`/`Bend`/`Audit` unknown.
- The remaining C2923, C2976, C2641, C2146, C3861, C2737 and C2672 diagnostics are downstream type/parser errors after those declarations are absent.

The refold implementation was already present in header-only `src/core/RefoldGeometryAudit.h`, which includes `src/core/FoldedContourTopology.h`. `SheetMetal.cpp` included those headers, but `SceneDocument.cpp` invoked their types and functions without including them, so a clean MSVC compile fails. This was missed by local portable tests because they compiled narrower translation units.

## Fix merged into Sprint 16

`src/core/SceneDocument.cpp` now includes both `FoldedContourTopology.h` and `RefoldGeometryAudit.h` directly at the top. No refold formula, assembly/motion algorithm, scene schema or GUI semantics were changed by this patch.

`tools/preflight_major_build.py` checks the actual native source declarations against their required *direct* includes, catching this exact missing dependency before CMake/vcpkg configuration. Two new tests in `tests/test_sprint16_integrated_readiness.py` ensure the guard fails when either header is removed and that both public header contracts compile in an independent C++20 translation unit.

## Sprint 16 only: no Sprint 15 repair or rerun required

This ZIP is a complete, corrected Sprint 16 source tree. Do **not** use the earlier
Sprint 15 tree for testing, and do not apply any patch to it. Extract this ZIP
into a new directory and run `RUN_SPRINT16_WINDOWS_TARGETED_BUILD.ps1` there.
The historic Sprint 15 error log is used only to explain the fixed source dependency.

## Verification limits and next gate

Locally: direct-include guard positive and negative tests, public-header syntax compilation, Python/source regression, and portable C++ regression. The full Windows CMake/Qt/OpenCascade native build cannot run in this environment. The first restored error may have prevented the compiler from reaching further unrelated errors; **Sprint 16 native compilation and 130 CTests still require actual Windows execution**. The 60 manual engineering acceptance scenarios are unchanged.

If MSVC reveals another failure, collect the complete transcript including the *first* compiler error and do not conceal it behind downstream diagnostics.

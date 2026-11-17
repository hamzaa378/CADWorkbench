# CADWorkbench v1.70 — Acceptance gate execution (source candidate)

This source remains **preliminary**, not a production release.

Run from a fresh project directory on Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

The runner performs the source-integrity check, Windows MSVC Release build and all **130 CTests**, then scores `docs/V170_ACCEPTANCE_RESULTS.csv`. It deliberately returns a nonzero exit until the 85/85/65 acceptance rule has evidence. The spreadsheet starts with all 60 cases marked `NOT_RUN`: source implementation and portable tests **cannot** be substituted for scenario evidence.

The script creates a **mutable** scoring sheet at `build/V170_ACCEPTANCE_RESULTS.csv` on the first run. The original template in `docs/` is immutable and remains covered by the source-integrity manifest. The build folder is excluded from that manifest, so scored results survive normal repeated verification runs.

To record each real end-to-end test, change its `status` to `PASS` or `FAIL` and, for `PASS`, provide the path to a real test artifact in `evidence_path`, relative to project root or absolute. Review evidence yourself; the scorer checks that the artifact exists but cannot independently certify its semantic correctness. No partial-credit scenarios count as passed. All critical cases must pass, native tests must be present and non-skipped, and two workstreams must earn at least 17 of 20 scenarios each while the third earns at least 13 of 20.

Reports: `build/cadworkbench_release_ctest.xml` and `build/v170_acceptance_gate.json`. The original face-machining test #4 must pass as part of the full suite. Always preserve the last Windows-confirmed build before running this on new source.

This release gate does not establish full parity with Fusion or manufacturing/safety certification.

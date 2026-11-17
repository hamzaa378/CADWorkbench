# CADWorkbench v1.70 — Engineering Development Source (Sprint 11)

**Status: Windows automated verification passed on 17 November 2026; engineering acceptance is still pending.** This repository is a consolidated **source** checkpoint, not a certified CAD product or a binary release.

## Current verification

On the user's Windows/Visual Studio environment, Sprint 11 successfully built `build\Release\CADWorkbench.exe` with MSVC, and **all 130/130 native CTests passed**. The full **952/952 Python/source tests passed**, with the independent audit reporting `AUTOMATED_PASS`. The audit exit code `3` is intentional when manual engineering acceptance has not been recorded. **0/60 manual acceptance scenarios are formally verified**; therefore the engineering release gate is **BLOCKED**.

This repository intentionally does **not** include compiled Windows binaries, vcpkg-installed packages, Python virtual environments or personal machine audit logs. Retain your original Windows-verified Sprint 11 build and full audit log as rollback and verification evidence.

## Scope

C++20 / Qt 6 / OpenCascade 8, CMake and vcpkg. The project includes parametric geometry and feature history, topology-reference recovery, assembly mating and motion analysis, drawing and sheet-metal workflows, manufacturing handoff, and engineering inspection/reconciliation tools. Backend feature integration has priority; some functions are preliminary and require human validation before fabrication.

## Start here

- **[GitHub upload and first-clone instructions](GITHUB_UPLOAD_GUIDE.md)** — what to upload, how to push it, and how to verify the repository.
- **[Latest Windows verification and limitations](LATEST_VERIFIED_BASELINE.md)** — separates automated checks from real-world engineering acceptance.
- **[Sprint 11 continuation / full engineering handover](SPRINT11_CONTINUATION_20261117.md)** — current workflows and tool usage.
- **[60-case acceptance tracker](docs/V170_ACCEPTANCE_RESULTS.csv)** — presently `NOT_RUN`; use this for acceptance evidence.
- **[GitHub portable CI](.github/workflows/portable-regressions.yml)** — source, Python/OCP and portable C++; does *not* replace the Windows native gate.

## Windows full build and verification

Prerequisites: Windows, Visual Studio with MSVC C++ toolchain, CMake/CTest, Python 3, and [vcpkg](https://github.com/microsoft/vcpkg) installed at `C:\vcpkg` (or provide your own location). The project uses the checked-in `vcpkg.json` dependencies.

In **PowerShell**, from the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT11_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4 -FullAudit
```

The script runs the source preflight and focused native checks, then the full application build, all 130 native CTests, the Python/kernel suite and evidence classifier. Exit `3` indicates automated verification passed but recorded manual acceptance is outstanding. Inspect the generated reports before deciding release readiness.

## Local portable checks

```bash
python tools/preflight_major_build.py --root .
python tools/audit_full_source.py --root .
python tools/verify_130_tests.py
python -m pip install -r requirements-kernel-reference.txt
python -m unittest discover -s tests -p '*.py' -q
python tools/run_portable_regressions.py --compiler g++ --jobs 3
python tools/run_portable_regressions.py --compiler clang++ --jobs 3
```

These checks do **not** demonstrate that the Qt GUI, shop drawings or manufactured parts pass user acceptance.

## Change policy

Maintain the last Windows-verified baseline; use approximately **75% effort on completing/integrating existing features, 20% on reliability/performance, and 5% on genuinely missing capabilities**. Independent backend workstreams can progress concurrently, but merge only after combined regression checks. Never hide failing tests or silently change the historical 130 native test registrations. There is no automatic fabrication or standards-certification authorization.

## License

No open-source license has been selected or provided in this source archive. **A public repository alone does not grant third parties a license to modify or redistribute the code.** Add an appropriate `LICENSE` only after confirming ownership and the intended terms.

> **Sprint 17 Qt modal automation repair (2026-11-18):** `APPLY_SPRINT17_QT_DIALOG_FIX_TO_VERIFIED_SPRINT16.ps1` updates only the UI automation overlay to detect Qt6 dialogs through checked Win32 handles and allows secure keyboard fallback for form fields. Pillow is required for screenshots. No CAD native source or binary has been changed; the new UI driver still requires a Windows live rerun. See `SPRINT17_QT_DIALOG_FIX_20261118.md`.

# Sprint 17 add-on: guarded UI workflow automation and noninteractive background regression

**Windows-verified base:** Sprint 16 compiled successfully under MSVC and passed 130/130 native CTests and 1,050/1,050 Python/source tests in the user's Windows audit. Manual engineering acceptance is still pending. Sprint 17 adds Python workflow automation and launchers only; **Sprint 17's UIA workflow runs have not yet been executed/verified on Windows**. The source does not alter the verified Sprint 16 C++ engine or GUI.

Read **[SPRINT17_UI_AUTOMATION_GUIDE.md](SPRINT17_UI_AUTOMATION_GUIDE.md)** before running the automation. A normal Windows UIA test takes control of its current interactive session. To keep using the host laptop, run UI automation **inside a dedicated unlocked Windows VM**, or run only the desktop-free backend regressions with `RUN_SPRINT17_BACKGROUND_REGRESSION_WINDOWS.ps1 -Detach` on your host.

---

# CADWorkbench v1.70 — Sprint 16 Integrated Engineering Readiness (Preliminary Source)

**Status: Sprint 16 source candidate, local Python/GCC/Clang checks passed; Sprint 16 Windows MSVC build NOT YET verified.** This repository is a consolidated **source** checkpoint, not a certified CAD product or a binary release.

## Current verification

On the user's Windows/Visual Studio environment, the **preserved Sprint 11 baseline** successfully built `build\Release\CADWorkbench.exe` with MSVC, and **all 130/130 native CTests passed**. The full **952/952 Python/source tests passed**, with the independent audit reporting `AUTOMATED_PASS`. The audit exit code `3` is intentional when manual engineering acceptance has not been recorded. **0/60 manual acceptance scenarios are formally verified**; therefore the engineering release gate is **BLOCKED**.

Sprint 16 adds atomic coordinated multi-joint motion-track editing and synchronized retiming, and consolidates real OCCT geometry, strict saved-drawing regeneration, folded/refold audits, existing project findings and persisted assembly keyframe rate checks into one unissued engineering readiness report. Sprint 15 multi-joint mate editing, multi-sheet regeneration and refold validation are all retained. These Sprint 16 changes still require their own native Windows regression.

This repository intentionally does **not** include compiled Windows binaries, vcpkg-installed packages, Python virtual environments or personal machine audit logs. Retain your original Windows-verified Sprint 11 build and full audit log as rollback and verification evidence. Sprint 12 introduced cross-revision change impact, native hole/drawing comparisons and external evidence packets, but has not passed the Windows gate. Sprint 13 added source-linked flat-pattern verification and multi-part production checks. Sprint 14 connects evaluated parametric alternatives to revision-protected live commits, introduces atomic multi-sheet edge-witness review and a safe drawing-recovery CSV, and removes repeated dependent rebuilds for coordinated parameter edits. Sprint 12 through Sprint 16 Windows native gates remain pending unless separately verified by the user.

## Scope

C++20 / Qt 6 / OpenCascade 8, CMake and vcpkg. The project includes parametric geometry and feature history, topology-reference recovery, assembly mating and motion analysis, drawing and sheet-metal workflows, manufacturing handoff, and engineering inspection/reconciliation tools. Backend feature integration has priority; some functions are preliminary and require human validation before fabrication.

## Start here

- **[GitHub upload and first-clone instructions](GITHUB_UPLOAD_GUIDE.md)** — what to upload, how to push it, and how to verify the repository.
- **[Latest Windows verification and limitations](LATEST_VERIFIED_BASELINE.md)** — separates automated checks from real-world engineering acceptance.
- **[Sprint 16 complete continuation guide](SPRINT16_CONTINUATION_20261117.md)** — integrated native review, motion-rate audit, regression evidence and Windows checks.
- **[Sprint 15 complete continuation guide](SPRINT15_CONTINUATION_20261117.md)** — latest assembly/drawing/refold workflows, native tests and Windows instructions.
- **[Sprint 14 continuation guide](SPRINT14_CONTINUATION_20261117.md)** — latest parametric and drawing integration plus native regression scope.
- **[Sprint 13 handover](SPRINT13_CONTINUATION_20261117.md)** — latest native flat production verification, batch engineering and Windows commands.
- **[Sprint 12 major integration / continuation guide](SPRINT12_CONTINUATION_20261117.md)** — two-revision workflows and tests.
- **[Sprint 11 verified source baseline](SPRINT11_CONTINUATION_20261117.md)** — historic last Windows-verified implementation.
- **[60-case acceptance tracker](docs/V170_ACCEPTANCE_RESULTS.csv)** — presently `NOT_RUN`; use this for acceptance evidence.
- **[GitHub portable CI](.github/workflows/portable-regressions.yml)** — source, Python/OCP and portable C++; does *not* replace the Windows native gate.

## Windows full build and verification

Prerequisites: Windows, Visual Studio with MSVC C++ toolchain, CMake/CTest, Python 3, and [vcpkg](https://github.com/microsoft/vcpkg) installed at `C:\vcpkg` (or provide your own location). The project uses the checked-in `vcpkg.json` dependencies.

In **PowerShell**, from the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPRINT16_WINDOWS_TARGETED_BUILD.ps1 -VcpkgRoot 'C:\vcpkg' -Parallel 4 -FullAudit
```

The Sprint 16 script runs source preflight, builds the affected native targets and GUI, and runs four focused CTests before the full audit; with `-FullAudit`, it invokes the comprehensive 130-native-test and Python/kernel check. Its diagnostic classifier distinguishes automated verification from the 60 pending manual acceptance scenarios. Exit `3` can indicate automated verification passed while manual acceptance remains outstanding. Sprint 16 native verification is pending. Inspect the generated reports before deciding release readiness.

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

Maintain the last Windows-verified baseline; **this Sprint 15 release** allocates **70% to engineering workflow integration, 20% to reliability/performance, and 10% to missing capabilities**. Independent backend workstreams can progress concurrently, but merge only after combined regression checks. Never hide failing tests or silently change the historical 130 native test registrations. There is no automatic fabrication or standards-certification authorization.

## License

No open-source license has been selected or provided in this source archive. **A public repository alone does not grant third parties a license to modify or redistribute the code.** Add an appropriate `LICENSE` only after confirming ownership and the intended terms.

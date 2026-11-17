# CADWorkbench v1.70 — clean consolidated source (7 November 2026)

This is a **self-contained source distribution** rebuilt from Fix 3 with Fix 4,
Fix 5, the serial-build adjustment, Fix 7, and one new integrated fix.
Do **not** extract earlier overlays onto this tree. Start in a fresh folder.

## Important corrective change

The remaining reported native test was `cadcore_v170_interleaved_history`, at
`unaffected edge survives a prior selective fillet`.

The rebuilt history engine now makes a distinction between:

- **Persistent reference**: The original witnessed edge descriptor in the saved
  interleaved DAG. This is never replaced by an unverified index.
- **Current-stage execution**: After every tracked route has uniquely and
  successfully resolved the live edge on the current input shape, the engine
  temporarily uses that exact 1-based edge index to execute the selected
  fillet/chamfer. This avoids a redundant second anchor search after the first
  fillet changes the body geometry. The index is bounds-checked and is not
  persisted as a replacement for the saved witnessed feature.

The existing reject-if-consumed and reject-if-ambiguous routes remain in the
lineage resolver. The failure message in the native test now includes the
underlying `SceneDocument` error to diagnose any remaining issue.

This is a reasoned source change; the **native Windows test has NOT been rerun**
in the packaging environment. Do not treat this ZIP as an acceptance-passed
Windows release until the Windows command below succeeds.

## Windows build and tests (single command)

Prerequisites: Visual Studio with C++, CMake, and the configured `C:\vcpkg`
with Qt6 and OpenCascade dependencies.

Open PowerShell in the extracted folder, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

This verifies the source manifest, builds Release with **one MSBuild job** to
avoid competing vcpkg DLL copies, checks that **130 distinct native CTests**
are registered, and runs all of them. It produces:

`build\cadworkbench_release_ctest.xml`

If the interleaved-history test still fails, run the built test directly to
view the full source-level failure message:

```powershell
.\build\Release\interleaved_history_v170_tests.exe
ctest --test-dir build -C Release -R '^cadcore_v170_interleaved_history$' -V
```

The GUI binary will be at `build\Release\CADWorkbench.exe` if the build passes.

## Evidence and limitations

- Source/reference/independent geometry Python suite: **337 passed** here.
- GCC and Clang portable C++ suites with address and undefined-behavior
  sanitizers: **passed** here; see `scripts/verify_portable_consolidation.sh`.
- Independent Python OCCT geometry: verified that the first box edge is consumed,
  the separate opposite edge survives in exact topology, both fillets build,
  and the downstream boss remains valid.
- Native Qt/OpenCascade/MSVC rebuild and 130 Windows CTests: **NOT performed here**.
- Broader original 50-feature roadmap: **not fully implemented**. This package
  consolidates the current working source and corrects a specific native
  regression; it is not represented as final feature-complete v1.70.

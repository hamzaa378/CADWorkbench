# CADWorkbench v1.65 — verification report

**Baseline:** user-confirmed v1.64 Windows Release native tests 87/87 passing. v1.65 is a source candidate; MSVC/Qt/OpenCascade native compilation cannot be executed in this Linux environment.

**Local verification performed:**
- CMake syntax parsed through platform detection; the expected Windows-only guard stops Linux configuration before package discovery. This is **not** a successful Windows CMake configure.
- New v1.65 source preflight: 4/4 passed.
- Updated legacy version-sensitive preflights for v1.58, v1.61, v1.62, v1.63, and v1.64, plus v1.59/v1.60 contracts, passed in individual runs. Historical tests now check the new prefix cache rather than a removed append-only shortcut.
- Independent Python CadQuery/OpenCascade geometry smoke: 14/14 checks (solid intersection volumes, arbitrary rotation/reflection, linear and circular patterns) passed. It is independent reference evidence, **not** the native C++ feature-tree test.
- A new C++ regression test `cadcore_v165_incremental_feature_graph` exercises branch intersection volumes, cache reuse, failed edit rollback, undo/redo, save/reopen, arbitrary-axis rotations and patterns, and invalid parameters.
- Earlier C++ engine files are unchanged from v1.64; existing verification therefore remains applicable but is not automatically a new native pass.

**Not verified here:** MSVC build/link, Qt MOC, Windows GUI interaction, and the full 88/88 CTest suite. Run the Windows verification script and examine JUnit evidence before promoting v1.65 as the working baseline.

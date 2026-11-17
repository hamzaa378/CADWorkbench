# CADWorkbench v1.70 — MSVC/OCCT 8.0.1 compilation correction

The 2026-11-07 Windows Release log contained five MSVC compilation errors, all originating at `src/core/SceneDocument.cpp:3894`. `TopTools_IndexedMapOfShape` was used without its type declaration, so the variable `inputEdges` also failed to compile and caused four subsequent diagnostics.

The tracked-edge execution branch now uses `NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>`, consistent with the already-included `<NCollection_IndexedMap.hxx>` and `<TopTools_ShapeMapHasher.hxx>` and other indexed shape maps in the same translation unit. No geometry or feature-history algorithm was modified.

The regression `tests/v170_msvc_occt_map_regression.py` checks this source contract. Complete Python and portable C++ checks are available through `scripts/verify_portable_consolidation.sh`. **Neither provides native Windows MSVC compilation or runs Qt/OpenCascade native tests.**

## Windows commands

Extract this archive to a **new, empty directory** (do not overlay on the old source). From the extracted `CADWorkbench_v170_CleanBuild` folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

The script performs integrity verification, CMake/MSVC Release compilation, and the 130 CTest registrations. If the build succeeds but a test fails, run `ctest --test-dir .\build -C Release -R cadcore_v170_interleaved_history --output-on-failure -V` to see the interleaved-history failure in detail.

**Release status:** New source checks run locally. Native Windows compilation and the previously failing interleaved-history test still require the target Windows machine. Do not certify a release before those checks pass.

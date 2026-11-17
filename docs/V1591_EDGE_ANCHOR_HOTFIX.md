# CADWorkbench v1.59.1 — edge-anchor CTest hotfix (4 November 2026)

The Windows v1.59 Release build **compiled successfully**, including `CADWorkbench.exe` and test binaries. The user's actual CTest result was **76/77 PASS**: only `cadcore_associative_features_v159` failed with `Edge anchor did not resolve its original edge`.

**Fix in this candidate:** `src/core/ShapeFeatureTree.cpp` now uses `TopExp::MapShapes` and a `TopTools_IndexedMapOfShape` for both edge-anchor capture and resolution. The previous raw `TopExp_Explorer` enumerated each shared box edge twice (24 edge occurrences for 12 distinct edges) and mistakenly treated duplicate occurrences as ambiguous alternatives. A real ambiguity between distinct coincident edges is still rejected. Both operations now use the same **1-based unique-edge index**; callers must not pass a raw repeated explorer occurrence index.

**Added coverage:** the native regression exercises all twelve unique edges, invalid indices/lengths, and a truly ambiguous coincident-edge fixture; `tests/v159_source_preflight.py` guards the implementation against reverting to raw traversal, and `tests/edge_anchor_reference_smoke.py` independently demonstrates the OCCT behavior through Python/OCP.

**Status:** The corrected v1.59.1 ZIP has passed local source-contract and independent OCP reference checks, but the patched native binary has **not been compiled or run on Windows in this environment**. The expected native total remains **77 tests**, unchanged. Do not claim 77/77 until the Windows CTest run confirms it.

To verify from an extracted source directory in Windows PowerShell:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
PowerShell.exe -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1
```

For a targeted rerun after building:

```powershell
ctest --test-dir .\build -C Release --output-on-failure -R '^cadcore_associative_features_v159$'
```

The rest of the v1.59 source, including linked Boolean bodies, constraint editor, and template-based revolve/sweep/loft feature nodes, is preserved without functional changes.

---


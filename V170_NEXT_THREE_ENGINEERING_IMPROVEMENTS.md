# CADWorkbench v1.70 — Next three engineering improvements (source candidate)

This is a full source snapshot based on the user's `CADWorkbench_v170_Three_High_Impact_Engineering_Source_20261106.zip`. It does **not** replace the previous passing Windows executable.

## 1. Nested pattern occurrence selection (v5 history)

A tracked feature can now select one instance at each supported upstream pattern step using the pattern step's stable ID (`referencePatternOccurrences`). For example a feature might select occurrence 1 of `pattern-X`, then occurrence 2 of `pattern-Y`.

* Every nonzero selection must refer to a preceding registered pattern and be present on **every** accepted ancestry route. An omitted selection means instance zero (the seed).
* Each index is verified against the current pattern count on rebuild. Deleted instances cause the edit to fail without committing changes.
* `dag-interleaved-history-v5` serializes the map as `reference_occurrences`, retaining read compatibility with v1-v4 and the v4 `reference_occurrence` field.
* The feature editor provides per-stage instance controls and a scrollable form suitable for smaller displays.
* The map is bounded by 40 entries; the existing 40-stage overall limit remains.

Example saved step fragment (illustrative IDs, not a complete project):

```json
{"reference_source":"@base", "reference_occurrences":{"pattern-X":1,"pattern-Y":2}}
```

## 2. Exact surviving-subshape lineage through edge finishing

Tracked selected-edge and supported face operations may traverse **fillet/chamfer stages** when the *same OCCT topological subshape* is still present in the result. The resolver requires exact `IsSame` continuity and refreshes the witness on the actual output. It rejects replaced, consumed, or ambiguous shapes instead of guessing a geometrically similar replacement.

This is **not** general kernel history for fillets/chamfers, and **not** full split/merge naming through topology-changing modifiers. It intentionally does not recover new fillet rounds, new chamfer faces, or consumed original edges. The same limited rule applies when the intermediate fillet itself is ancestry-attached.

## 3. Linked loft/sweep design workflows in the mixed editor

The compact mixed feature editor now exposes editable `Sweep circle path` and `Loft rectangles` steps. These operators were already implemented in `ShapeFeatureTree.cpp`; now designers can insert and change their parameters in an externally sketch-linked feature history and regenerate the complete chain when the sketch source changes.

The editor uses a scrollable layout for the additional engineering parameters. Loft/sweep operations are **not** considered reference-preserving ancestry stages, so an unrelated downstream face/edge link attempting to cross them without true provenance must fail.

## Verification evidence in this environment

| Verification | Result |
| --- | --- |
| Python source contracts + independent OCCT geometry checks | **231 passed** (`python -m unittest discover -s tests -p '*.py' -q`) |
| GCC portable C++ with ASan/UBSan | **37 + 347 + 80 checks passed** |
| Clang portable C++ with ASan/UBSan | **37 + 347 checks passed** |
| Clang linked-body portable C++ (non-sanitized) | **80 checks passed** |
| Windows CTest registrations | **130 retained**, not executed here |
| Qt + native OCCT/MSVC application build | **Not verified** |
| Windows interactive GUI acceptance | **Not verified** |

Linux CMake is intentionally blocked by the project's Windows-only CMake guard. The additional native integration regression scenarios in `tests/interleaved_history_v170_tests.cpp` are **written but not yet run**; they include nested patterns, v5 round-trip and malformed records, unaffected/consumed fillet edge behavior, and linked loft/sweep regeneration.

### Windows acceptance gate

From the extracted root in PowerShell:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Do not promote this source candidate to a verified release until the native Windows build, all 130 CTests, and interactive validation succeed. If a native test fails, preserve the prior known-good binary and diagnose that exact failure first.

### Outstanding high-impact gaps

General kernel-backed lineage through fillet-generated faces, arbitrary B-spline rebuilds, full loft/sweep generated-face naming, and complex multi-branch pattern identity still require further native engineering and test coverage.

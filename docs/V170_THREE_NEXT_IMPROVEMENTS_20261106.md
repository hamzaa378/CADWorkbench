# CADWorkbench v1.70: Three Engineering Improvements (source candidate)

This increment is based on the 2026-11-06 Boolean-provenance development source.
It is **not yet MSVC-compiled or Windows-tested**.

## 1. Multi-stage Boolean ancestry, fail-closed

`BooleanLineagePath.h` plans a **unique** route from the reference source
(`@base` or any earlier feature stage) to the parent Boolean result. Every hop
must be a completed Boolean build retained in the **current** replay. `SceneDocument`
then calls `rebaseFeatureThroughBoolean` on each hop, retaining the kernel's
`Modified`, `Generated`, and `IsDeleted` provenance and recapturing the
selected face/edge anchor at every stage.

Supported: successive `SketchFuse`/`SketchCut` and two-parent Boolean joins
when a single ancestry route exists. Unsupported: tracking across arbitrary
intervening transforms or fillets, Boolean diamonds where both branches carry
the same source reference, inherently ambiguous split/merge successors,
complex B-spline remapping. These fail rather than silently selecting a new
face or edge. This is not a general topological-naming guarantee.

## 2. Intermediate-solid preview and anchored selection

`SceneDocument::previewInterleavedFeatureHistory` evaluates a staged feature
prefix from the frozen base in a local copy and exposes `@base` and each step's
result. It does not mutate the scene, checkpoint history, or replace the saved
shape. The mixed-history editor now enumerates captured straight/circular edge
witnesses and planar face witnesses from an **earlier selected stage** instead
of only from the frozen base.

## 3. Face machining and edit safety

The compact editor can now create `Face normal pocket` and `Face normal boss`
features, alongside selected-edge fillets and chamfers, with a tracked source
stage. Reordering a step is rejected when it would introduce forward feature
references or move geometry ahead of its own source. Removing a step is rejected
if another step explicitly depends on it. Implicit previous-result changes
require a user confirmation. The underlying document rebuild remains
transactional; failed Boolean or topology resolution never commits a candidate.

## Checks and how to verify

Portable ancestry planner (both compilers, with ASan+UBSan):

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Isrc tests/boolean_lineage_path_v170_tests.cpp -o planner-gcc
./planner-gcc
clang++ -std=c++20 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Isrc tests/boolean_lineage_path_v170_tests.cpp -o planner-clang
./planner-clang
```

Python static contracts and OpenCascade geometry smoke tests:

```sh
python -m unittest discover -s tests -p '*preflight.py' -v
python -m unittest discover -s tests -p '*kernel_reference.py' -v
```

Native Windows verification (unchanged registration count: **130 CTests**):

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Windows interactive acceptance: edit upstream sketches; inspect an anchored
face/edge after two Boolean stages; edit a middle source stage, undo/redo,
save/reopen, then edit again; confirm that ambiguous and deleted references
are rejected without committing corrupt geometry. Native test results are
necessary before adopting this as the user's passing baseline.

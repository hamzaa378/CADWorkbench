# CADWorkbench v1.70 — three engineering reference improvements (2026-11-06)

**Status: source candidate only.** Not compiled with MSVC/Qt/OpenCascade on Windows. The
last successful native executable remains the release baseline. This snapshot
extends the preceding Three Engineering Improvements candidate, rather than
replacing the user's last verified installation.

## 1. Trimmed NURBS/Bezier edges with measured local curvature

- `captureStableEdgeAnchor` now writes a **v3** fixed-field witness when the
  selected trimmed edge is a B-spline or Bezier curve. Earlier line/circle
  witnesses remain **v2**; legacy centroid/length anchors also remain readable.
- A v3 anchor records an interior 3D point, unit tangent and **local geometric
  curvature**, not just curve length or traversal index. The resolver verifies
  the curve family, interior trimmed-point projection, tangent agreement,
  curvature agreement and uniqueness. A witness exactly on a split boundary is
  deliberately rejected.
- The OpenCascade Boolean-history filter remains authoritative when following
  descendants through a Boolean. Geometric matching alone must never prove
  ancestry if the kernel reports no matching successor.

**Limits:** An edited NURBS control net that moves its witnessed point or changes
local curvature by more than the allowed tolerance requires reselection. The
implementation does not provide a universal persistent B-spline-face naming
system or identity across arbitrary nonrigid deformation.

## 2. Exact rigid stages in associative reference histories

- `SceneDocument` now registers the source of supported single-input
  **Translate, Rotate X/Y/Z and Rotate arbitrary axis** steps during a *current*
  replay. The ancestry planner can walk those stages interleaved with Boolean
  operations. Every hop must be unique and must have a recorded supported
  operation in that replay.
- `rebaseFeatureThroughRigid` transports a selected edge witness or planar
  face witness through the exact transform and resolves it uniquely in the
  produced solid. For face-normal and face-rectangle machining, the intended
  original design point is rotated too, then the feature's local `u` and `v`
  offsets are recomputed against the new face frame.
- Invalid, ambiguous, deleted, or non-reproducible references reject the entire
  staged edit before the document commits it. Existing v3 DAG schema and old
  linear histories are retained.

**Limits:** Uniform scaling, mirrors, patterns, fillets, general sweeps and
other topology-changing modifiers remain **untracked** in reference paths.
A single-input transform does not imply support for a general nonlinear
feature-history change. Boolean diamonds with more than one source route fail.

## 3. Curved-face shell-opening references through Booleans and rotations

- A `Open-face shell` step can carry the existing 14/15-field interior curved
  face witness for a **cylinder, sphere, cone or torus**. `findFace` resolves
  the witness in current geometry, not by saved traversal index.
- Boolean rebase follows only the OpenCascade-confirmed descendants of that
  source face, then checks unique witness containment on the retained support
  and captures a fresh anchor. Rigid rotation/translation similarly transports
  the curved witness and support frame.
- The main feature-tree editor captures a curved witness when the user selects
  a supported nonplanar shell opening. The compact mixed-history editor now
  exposes `Open-face shell` with thickness, intermediate planar/curved face
  selection and reference-stage preview. The older planar opening remains
  supported.

**Limits:** Shell generation itself remains subject to kernel offset geometry
constraints, thickness, self-intersections, and B-rep validity. General
B-spline surfaces and curved-face sketch machining are not included.

## Regressions and acceptance

- `tests/selected_edge_v160_tests.cpp`: v3 spline capture/trim/split-boundary,
  curvature tamper, rotated references, planar-face offset rotation, scale
  rejection. Existing CTest target retained.
- `tests/curved_face_lineage_v170_tests.cpp`: shell anchor validation, cut
  descendant and rigidly rotated curved-face references, corrupted anchors.
  Existing CTest target retained.
- `tests/interleaved_history_v170_tests.cpp`: Boolean → rigid → Boolean
  path and rollback for untracked scale. Existing CTest target retained.
- `tests/boolean_lineage_path_v170_tests.cpp`: pure C++ mixed-path ambiguity.
- `tests/v170_lineage_extensions_preflight.py`: static integration checks.
- `tests/v170_lineage_extensions_kernel_reference.py`: independent OpenCascade
  spline/curved geometry checks; **not** executable integration evidence.
- Registered Windows CTest count remains **130**, but newly added native
  assertions cannot be marked passed until the Windows application is built
  and all tests execute.

Portable checks (GCC and Clang, `-fsanitize=address,undefined`):

```sh
python -m unittest discover -s tests -p '*preflight.py' -q
python -m unittest discover -s tests -p '*kernel_reference.py' -q
```

Native Windows acceptance command (MSVC/vcpkg/Qt/OpenCascade dependencies required):

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

After tests pass, use the UI to author an upstream sketch Boolean, an intervening
rotation, a downstream selected-edge feature; edit/revert the source; save,
reopen, and edit again. Repeat with a cylindrical shell opening after a Boolean.
Confirm that deleting a source witness or introducing two possible descendants
fails without altering the previous model. Keep last Windows-verified release
until these acceptance checks have actually passed.

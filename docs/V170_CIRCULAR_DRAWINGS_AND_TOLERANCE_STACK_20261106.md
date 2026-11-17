# v1.70: bounded circular drawing regeneration and clearance tolerance stacks

**Status: development source, native Windows integration unverified.** This is a
narrow improvement to roadmap items **20, 42 and 50**, not completion of the
50-target v1.70 roadmap.

## 1. Linked diameter and hole-location dimensions after radius edits

`DrawingSheet.cpp` retains the existing strict line/circle/spline edge-anchor
resolver for ordinary edge lengths, machining features and unchanged geometry.
If that resolver fails for a circular drawing dimension, an **opt-in,
drawing-only** resolver may match an updated complete circle. It requires:

- A valid existing v2 full-circle witness (center-of-perimeter, interior point,
  tangent, curvature and source body center). Legacy index-only annotations
  are **not** upgraded automatically.
- The candidate remains a single *complete* circular edge with a plane and
  axis parallel to the original circle; same spatial center (or supported
  whole-body translation compensation), within a small geometric tolerance.
- Relative radius change no more than **20%**. The system looks at every
  candidate and rejects all matches when there are zero or multiple eligible
  circles. A changed circle with the same center but two close concentric
  successors is intentionally **ambiguous**.
- The drawing remeasures the *new* B-rep edge and uses the current body
  revision in its exported metadata. Existing dimensional tolerances are
  preserved as user-authored values, not inferred from CAD geometry.

**Not supported:** arbitrary hole relocation, changing a circle into an arc,
fillet-face reconstruction, perpendicular-axis movement, disambiguation of two
concentric circles, automatically accepting a change beyond 20%, or machine
inspection certification. If unsupported, drawing regeneration fails and
requires manual reselection. This does **not** change the persistent reference
contract for fillets, chamfers, pockets, or assembly mates.

## 2. Explicit three-part linear worst-case tolerance stack

The existing **Guided engineering studies** dialog includes the new
`three_part_clearance_stack` study, with seven editable inputs:

- Three nominal lengths `A`, `B`, `C` in millimetres, with the design
  equation `clearance = A + B − C`.
- Maximum **symmetric bounded** length deviations for each span, all in mm.
- Required minimum clearance, also in mm.

Calculations use the deterministic envelope:

```
nominal = A + B - C
uncertainty = delta_A + delta_B + delta_C
minimum = nominal - uncertainty
maximum = nominal + uncertainty
safety_margin = minimum - required_clearance
```

A negative safety margin displays an explicit not-guaranteed warning. The
calculation is **not** a statistical fit distribution or GD&T process
capability result; it excludes thermal changes, noncollinear dimensions,
angular errors, datum effects and covariance. This is preliminary engineering
screening, not manufacturing authorization.

## 3. Regression and acceptance

- Portable C++ `CircularEdgeLineage` tests include radius enlargement/shrinkage,
  axis reversal, moved planes/centers, ambiguous successor sets, invalid
  numbers and range boundaries.
- Independent OpenCascade Python tests exercise actual full/partial circle
  B-reps and verify that radius changes can retain a center/axis reference.
- Existing `cadcore_tests.cpp` drawing suite adds **unexecuted Windows-native**
  cases for a resized cylinder drawing, the new source revision, excessive
  radius change, and two ambiguous concentric successor circles.
- A portable guided engineering study regression checks nominal clearance,
  worst-case envelope, safety margin, negative deviation rejection and warnings.
- `scripts/verify_portable_consolidation.sh` now runs the new suites without
  registering an extra Windows CTest. All 130 existing CTest registrations
  remain intact.

The **mandatory** Windows release gate remains:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

In the Qt app, create a cylinder with a saved `DIAMETER` drawing dimension,
change only its radius within 20%, regenerate and verify the new measurement,
then test a missing circle, ambiguous concentric circles, out-of-range edit,
save/reopen, undo/redo, and SVG revision metadata. Run a three-part tolerance
study with an unsafe and a safe limit case. Until the MSVC build, all 130 native
CTests, and interactive application checks pass, keep the last verified
executable and do not label this increment a completed v1.70 release.

# CADWorkbench v1.70 — three high-impact engineering improvements (2026-11-06)

**Source-development candidate.** This changes the latest `Spline_Rigid_Curved_Shell` source package and retains its prior features. It has **not** been compiled or executed as a complete Qt/OpenCascade Windows application. Keep the most recent native-verified binary intact.

## 1. Freeform curved-face references (B-spline and Bezier)

- `captureCurvedFaceAnchor` now accepts trimmed `GeomAbs_BSplineSurface` and `GeomAbs_BezierSurface` as surface types **5 and 6**, alongside existing cylinders/spheres/cones/tori.
- Uses the original 14-number saved anchor footprint: the curved witness is an interior 3D point, center fields also contain that original point, axis fields hold the local unit normal, and radius records the square root of the face surface area as a tolerance scale. Old analytic anchors remain unchanged.
- Reattachment projects the witnessed point to each candidate surface, requires interior trimmed-face containment, normal agreement |dot| >= 0.98, a projection distance of at most `max(1e-5, min(0.25 mm, 1% of the saved area scale))`, and exactly one candidate.
- The kernel's Boolean ancestry remains authoritative when following a face through a Boolean. A geometric look-alike not recorded as a kernel descendant cannot be promoted to a reference.
- The compact editor can select spline curved faces **for shell-opening reference capture only**. Shell success itself depends on a valid closed solid and OCCT's thick-solid algorithm; standalone open faces are not shellable solids.

**Scope:** Small local deformations and supported splits/trims; not arbitrary high-deformation B-spline reconstruction, globally stable topology naming, or general spline-surface machining.

## 2. Multi-source Boolean merge references

- The ancestry planner now enumerates up to eight Boolean/rigid/pattern routes, each up to 40 graph stages, rejecting cycles, oversize graphs and nonexistent paths.
- At a Boolean diamond, every route must independently rebase the same selected face or edge through actual OpenCascade descendant records. Only numerically equivalent final anchor **and machining placement** descriptors are accepted. If any route disappears, is unsupported, yields a different successor, or is ambiguous, the entire rebuild fails transactionally.
- Existing `uniqueBooleanPath` remains for older callers; new mixed histories use `allTrackedPaths`.

**Scope:** Accepts truly convergent ancestry, not arbitrary face naming after topological merges. No fallback to a positional guess.

## 3. Parametric pattern occurrence references

- Tracking now passes through documented linear X/Y/Z/vector and circular Z/arbitrary-axis pattern operations.
- An explicit **zero-based occurrence index** follows the same kernel transform that generated the patterned copy. The seed occurrence is `0`; indices `1..count-1` select non-seed copies. Overlapping or otherwise non-unique referenced faces/edges fail closed.
- Compact mixed-history editor exposes linear X/Y/Z and circular Z pattern authoring, count and spacing/sweep controls, and a referenced-feature occurrence selector.
- New `dag-interleaved-history-v4` serialization records `reference_occurrence` only for nonzero selections, with strict type/range validation on load. Existing v1/v2/v3 project records are still accepted.
- A nonzero occurrence requires exactly one tracked pattern stage on each ancestry route; nested, non-seed pattern selection is deliberately rejected until a per-stage instance map exists. The saved occurrence must also remain below the live pattern count.

**Scope:** This is supported pattern-copy reference propagation, **not** blanket reference tracking across fillets, lofts, sweeps, or arbitrary surface deformation.

## Tests performed in the available environment

- 145 Python preflight/source checks **passed**.
- 32 independent OpenCascade geometry/reference checks **passed** (4 new cases).
- Portable ancestry planner **37 passed** with GCC and Clang under ASan/UBSan.
- Sparse feature dependency planner **347 passed** with GCC and Clang under ASan/UBSan.
- Linked-body DAG planner **80 passed** with GCC under ASan/UBSan (Clang check documented separately if available).
- Additional native C++ regression cases were written into existing suites: Bezier/B-spline reference capture and deformation, pattern occurrence selection/ambiguity, convergent Boolean diamonds, v4 save/reopen/tamper, and commit rollback.
- **Windows MSVC compilation, Qt UI interaction, and the 130 native CTests have not been run here.** They are required acceptance gates. Linux CMake intentionally stops because this application is Windows-only.

### Windows acceptance

From the extracted source root, in PowerShell:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Then manually: create a linked-sketch Boolean, branch and rejoin its results, fillet a surviving edge; create a linear pattern, select copy index 1, save and reopen, edit the source sketch; reduce the instance count below the selected index and verify the previous committed body remains unchanged. Check that overlapping copies and ambiguous spline faces fail rather than silently retargeting.

Do not replace the user's last Windows-verified baseline unless native tests and interactive checks pass.

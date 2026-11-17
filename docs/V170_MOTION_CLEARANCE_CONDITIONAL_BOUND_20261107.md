# CADWorkbench v1.70 — conditional motion-clearance lower bound

Development increment for **A16 — motion sweep collision/clearance verification**. This is not an assertion that the complete A16 workflow is accepted or that the 85/85/65 release gate has been met.

## Why the change is necessary

An adaptive interference scan checks only discrete samples. Even when every sample is clear and the scanner has finished its requested refinement, a narrow collision may occur between sample times. Existing scanning therefore remains explicitly non-certifying by default.

## Optional input

The assembly scan dialog now offers an **opt-in** `provenRelativeSpeedBoundMmPerSecond`. It must be an *independently established conservative bound* on the sum of the largest instantaneous surface-point speeds of the two moving solids over the full study interval, in mm/s. For rotating solids, the bounding surface-point speed must include angular speed multiplied by distance from the rotation axis; use the full shape and all moving links. The scan does **not** derive or validate this kinematic bound. A declaration of `0` requires that both surfaces are stationary, and no moving boundary points contribute to the relative speed bound.

For every pair of consecutively sampled times `a < b`, if `d(a)` and `d(b)` are nonnegative minimum solid separations and the independent speed bound is `V`, the implementation uses the conservative lower bound:

`d(t) >= min(d(a), d(b)) - V*(b-a)/2` for any `t` in `[a,b]`.

It follows from the Lipschitz bound on minimum distance and the fact that every point in the interval lies at most half its width from an endpoint. An interval is conditionally proven above the requested safety margin only if its lower bound exceeds that margin by an additional numerical guard of `1e-8 mm`, and neither endpoint has positive overlap volume. The full study gets a positive conditional result only if **every** interval is proven. Observed clearance changes exceeding `V*(b-a)` by more than `1e-7 mm` contradict the supplied bound and reject the scan without modifying the previous report.

Unproven intervals are never silently treated as safe. If no independent bound is supplied (`-1`), the scan continues in its previous sample-based mode and the conditional result is false.

## Limitations

- There is **no automatic kinematic speed-bound derivation** from the assembly solver or swept surfaces.
- Correctness is conditional on the physical accuracy of the user-provided upper bound and the CAD kernel's gap/overlap measurements.
- A bound that is consistent with measured samples may still be too small between samples; sample consistency does not establish validity.
- No proof is made for deformable bodies, approximated geometry, numerical tolerances larger than the guard, or collision-free manufacturing safety.
- The normal unresolved-refinement flag remains separate from the conditional bound status; a scan can finish numerical refinement while having unproven time intervals.

## Regression evidence

- `tests/adaptive_motion_scan_v170_tests.cpp`: 65 portable assertions, covering safe bound, unsafe interior dip, false speed bound, invalid inputs, atomic failure, and backward compatibility.
- `tests/cadcore_tests.cpp`: native source regression cases for motion scene rollback and an overlapping assembly incorrectly marked safe; these still require a Windows Release CTest run.
- `tests/v170_motion_clearance_bound_contract.py`: source wiring checks, **not** native runtime validation.
- Existing 130 CTests remain registered. Native Windows Release and interactive GUI verification are pending.

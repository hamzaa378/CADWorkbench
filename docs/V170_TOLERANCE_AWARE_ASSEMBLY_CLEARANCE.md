# v1.70 source increment: tolerance-aware assembly clearance (candidate)

## Why this change matters

A nominally collision-free assembly can fail at manufactured part extremes. The old **Inspect → Assembly fit audit** reported exact minimum separation, contact and shared B-rep volume for the current static pose. It could not distinguish a nominally clear gap from a gap that may disappear when two parts deviate from their CAD nominal surfaces. This increment keeps that report and adds a conservative check using user-specified upper deviation bounds.

## Implemented interface

- Open **Inspect → Assembly fit audit: all pairs, clearance and tolerance bounds...** with 2–32 selected solid bodies, or no selection for visible solids.
- In the new dialog enter the **minimum required functional gap in mm** and a separate **maximum outward geometric deviation for every body**. Both are nonnegative; zero is the default and means *nominal-only*, NOT zero real manufacturing error.
- The result preserves **nominal** clear / below-clearance / contact / interference classification and adds **tolerance-aware** guaranteed clearance / clearance at risk / separation not guaranteed / nominal interference.
- Text and CSV include the two body deviations, conservative minimum gap, signed margin and the state. A nominal overlap has no reported clearance-margin value in the CSV: the shared-volume check takes precedence, and this tool does not calculate penetration depth.
- CSV export uses the existing `QSaveFile` atomic-write path. Formula-looking body labels/IDs are escaped before spreadsheet import, while ordinary quotation marks/commas are correctly quoted.
- Previous `inspectAssemblyFit` calls remain source compatible; `inspectAssemblyFitWithTolerances` adds a parameter matching the ordered body-ID list. A failed scan leaves the old result untouched. Source geometry tokens are retained for freshness checks.

## Precise engineering assumptions

Let `d` be the exact CAD-kernel minimum distance in mm between *nonintersecting nominal solids*, `rA` and `rB` be nonnegative maximum outward deviations (in all spatial directions) from their nominal solid envelopes, and `c` be the **required clearance**. If the two real manufactured solids lie entirely inside their respective nominal solids expanded by `rA` and `rB`, their separation has the conservative bound:

`guaranteed_lower_gap = max(0, d - rA - rB)`

`signed_margin = d - rA - rB - c`

- **Guaranteed within the stated bounds**: the nominal bodies do not contact/intersect, the raw lower gap is positive, and the lower gap is at least `c`.
- **Clearance at risk**: bodies are nominally separate, guaranteed separation remains positive, but the lower gap is smaller than `c`.
- **Separation not guaranteed**: the nominal bodies touch, or the outward deviation sum could consume the entire nominal gap.
- **Nominal interference**: CAD computes a significant intersection volume; the sign of the margin is not presented as a physical penetration-depth estimate.

This is a *worst-case geometric envelope* check. The inputs are explicit assumptions, not inferred manufacturing capabilities or a full datum-based geometric tolerance system. It is not a proof of heat growth, structural deflection, motion clearance, statistical yield, fitting to ISO/ASME GD&T, or process capability. Kernel precision, the correctness of input tolerances and omitted parts still matter. Do not use these results alone for safety-critical release decisions.

## Worked example (nominal 8 mm gap; required gap 5 mm)

| Body A bound | Body B bound | Lower gap | Margin | Result |
|---:|---:|---:|---:|---|
| 1 mm | 1 mm | 6 mm | +1 mm | Guaranteed within bounds |
| 2 mm | 2 mm | 4 mm | -1 mm | Clearance at risk |
| 4 mm | 4 mm | 0 mm | -5 mm | Separation not guaranteed |

Nominal classification remains **Clear** in all three cases. The new check does not secretly change any part geometry or mutate the scene.

## Verification (correct evidence labels)

Performed in the source development container:

- GCC 14 and Clang 17 compiled and ran `tests/assembly_fit_policy_v170_tests.cpp` using `-std=c++20 -Wall -Wextra -Werror`. Includes existing 512 baseline cases, a new set of input/boundary tests and 2,000 monotonically increasing deviation tests. Also verify with sanitizers before promotion.
- Source preflight checks, including preservation of 130 native CTest definitions, pass; the separate `v170_tolerance_clearance_preflight.py` guards the newly added UI/API/CSV paths.
- CMake parsed on Linux and hit the intentional Windows-only guard. **This is not Qt/OCCT compilation.**

Pending on Windows: run `scripts/verify_release_windows.ps1 -ExpectedTests 130` with MSVC/Qt6/OCCT8 and inspect its JUnit report; manually test the dialog and CSV, including formula-looking user-defined names. Keep the original last confirmed passing build as rollback.

## Manual acceptance steps (native Windows)

1. Create two 20 × 20 × 10 mm boxes. Translate the second box +28 mm on X. Open Inspect → Assembly fit audit with both selected, required gap 5 mm, bounds 1 mm each. Expect **8 mm nominal**, **6 mm conservative minimum**, **+1 mm margin**, **guaranteed within bounds**.
2. Repeat with 2 mm each: nominal remains **Clear**, but minimum is **4 mm**, margin **-1 mm**, result **clearance at risk**.
3. Repeat with 4 mm each: expect **separation not guaranteed**. Test a contact pose and a 4 mm overlap pose; nominal contact and nominal interference must keep precedence.
4. Test 3+ selected bodies with different per-body deviations; check pair-specific values in CSV. Duplicated IDs, negative/nonfinite values at the C++ API and invalid output pointers must fail atomically.
5. Verify saved CSV in spreadsheet software, quoted commas/newlines and formula-starting names. Edit geometry and confirm a previous report is no longer considered current by `assemblyFitReportCurrent`.
6. Run the full 130 native CTests and exercise save/undo/redo, assembly interaction, and no-regression source rebuild on a separate copy before promoting the candidate.

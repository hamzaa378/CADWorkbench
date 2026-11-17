# Sheet-metal flat patterns

> Current source addendum (3 November 2026): **Inspect → Check selected folded sheet against shop limits...** checks a saved, current folded-body definition against a user-entered minimum inside-radius/thickness ratio and minimum straight flange length. The check is read-only and does not certify material, tooling, or standards compliance. Folded flat-pattern DXF also supports per-bend K factors and optional rectangular end reliefs; see `CONTINUE_HERE.md` for the current scope.

Choose **File → Export one-bend sheet-metal flat pattern SVG...** and enter two outside flange lengths, sheet width, bend rotation angle, material thickness, inside bend radius, and K-factor. The exported SVG contains the blank outline, bend line, rule metadata, and developed dimensions. SVG rule attributes (angle, thickness, radius, K-factor) use round-trip numeric precision rather than truncating very small valid values to zero; on-canvas notes use eight significant digits (including scientific notation for tiny values) so valid positive dimensions are not mislabeled as 0.000 mm.

The calculator uses:

```text
bend allowance = angleRadians × (insideRadius + KFactor × thickness)
outside setback = (insideRadius + thickness) × tan(angleRadians / 2)
bend deduction = 2 × outsideSetback − bendAllowance
flatLength = outsideFlangeA + outsideFlangeB − bendDeduction
```

The supported bend angle is 15–165 degrees, thickness and sheet dimensions must be positive, radius may be zero or positive, and K-factor must be between 0 and 1. The formula assumes outside flange dimensions. The SVG physical width and height are emitted at six-decimal millimeter precision, matching the viewBox, so accepted small positive dimensions are not rounded to zero. If the bend station would round onto either outer edge at that precision, SVG export refuses the input; the underlying calculation can still report its mathematical dimensions.

## Scope

## Folded sheet-metal body (v1.49)

Choose **Create → Folded sheet-metal body...** to add a solid directly to the active scene. Enter comma-separated straight neutral-surface flange lengths and signed bend angles (one more flange than bends), plus one inside radius per bend, sheet width, and thickness. Positive and negative angles fold in opposite directions. Each bend can use a different radius. The model and its input definition save with the scene. Select it and use **Edit → Edit folded sheet-metal body...** to change the inputs and rebuild the solid as one undoable operation. If a later direct modeling operation changes the solid, the saved input definition remains inspectable but editing is blocked to avoid replacing newer geometry. The inputs describe the folded mid-surface path; they are separate from the outside flange dimensions used by the flat-pattern calculator.

With one current folded body selected, use **File → Export selected folded body flat pattern DXF...** to develop its saved neutral-surface straight lengths and bend radii. The export asks for one K-factor, applies it to every bend, and writes the blank outline and bend lines in millimeters. It supports one through fifty bends and works directly from the folded body's saved definition. The DXF is a point-in-time export: rerun it after editing the body to get an updated blank. This first linked export does not include end reliefs; use the separate multi-bend DXF workflow when relief notches are needed.

The cross-section samples each circular neutral bend into short straight facets, offsets those samples by half the sheet thickness, and extrudes the closed section across the width. Folded-body radii must be positive; zero-radius corners collapse the inner faceted bend profile. The saved definition supports editing these initial flange and bend inputs, but this is not a general sheet-metal feature history and is not associative with the flat-pattern DXF. It does not automatically add bend reliefs, compensate flange dimensions, unfold an imported solid, or model exact analytic cylindrical bend faces. The flat-pattern calculator and DXF remain the source for developed blank dimensions, and production use requires checking the geometry against material and tooling data.

## Multi-bend DXF

Use **File → Export multi-bend sheet-metal flat pattern DXF...** for 2–50 bends. The comma-separated flange, angle, radius, and K-factor fields require a numeric entry between every pair of commas; missing values, including trailing commas, are rejected rather than silently shifting the bend-to-rule mapping. Enter outside flange lengths in sequence (one more flange than bends), plus per-bend angles, inside radii, and K-factors. Each bend uses its own bend allowance, setback, and deduction. The calculator rejects flange segments too short for their adjacent setbacks and computes ordered bend-line center positions in the developed blank. The DXF uses millimeter units and separates the blank boundary (`OUTLINE`) from bend center lines (`BEND`). Optional end-relief width and depth create rectangular notches at both ends of each bend station; leave both at zero to disable them. Reliefs are rejected when the dimensions are incomplete, too deep, overlap, or extend beyond the blank. When end reliefs are enabled, bend center lines are trimmed to the remaining blank between the notches; without reliefs, bend lines span the full sheet width. Reliefs are also rejected if their remaining center web becomes a zero-length bend line when rounded to the DXF's six-decimal millimeter coordinates. Likewise, a relief-to-relief or relief-to-blank-edge gap must remain nonzero at that precision; gaps that collapse below the DXF coordinate resolution of 0.000001 mm would otherwise create degenerate `OUTLINE` entities.

The DXF writer declares the AutoCAD 2000 format (`AC1015`), millimeter units, and a `CONTINUOUS` linetype table used by its outline and bend layers so readers do not have to infer those drawing-table records. Folded-body DXF output derives from the saved bend definition at export time, but the emitted file does not refresh automatically when the model changes. End reliefs are rectangular blank notches, not a full corner-relief or seam solver. The app does not model exact analytic bend faces, material-specific K-factor tables, springback, tool compensation, or manufacturing validation. Confirm production dimensions against the material, tooling, and shop process.

DXF export also checks that each bend center remains distinct from its neighbors and from the blank edges **after** six-decimal rounding. An otherwise valid mathematical pattern is rejected for DXF export when two stations collapse to the same coordinate; adjust the flange spacing rather than relying on unresolved DXF geometry. The numerical flat-pattern calculator remains available for those inputs.

## Optional shop-specific rule checks (multi-bend)

The multi-bend DXF dialog offers two **opt-in planning limits**: minimum inside bend
radius divided by sheet thickness, and minimum straight length (mm) on every
flange after subtracting its adjacent bend setbacks. Both default to **zero**
(disabled) so existing flat-pattern exports are unchanged. A failing check
reports the numbered bend or flange before opening the save dialog. These limits
must come from the user's material and tooling process; the application does
not supply a universal allowable ratio, certify manufacturability, or model
springback. The folded-body tool is a separate approximate solid workflow. The core `validateMultiBendShopLimits`
API performs the same checks independently of DXF serialization.

## Verification

The Windows Release `--sheet-metal-flat-pattern` regression covers relief trimming, DXF rounding gaps, coincident bend stations, SVG tangent clearances, round-trip rule metadata, small-dimension notes, and strict comma-separated parsing. The `--folded-sheet-metal` regression covers signed and per-bend-radius folds, bounds, input persistence, edit/rebuild, undo/redo, stale-definition protection, neutral-length development, one- and multi-bend DXF output, and invalid-input atomicity. C++ regression tests were run on Windows; no live UI test was performed.

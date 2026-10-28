# Sheet-metal flat patterns

Choose **File → Export one-bend sheet-metal flat pattern SVG...** and enter two outside flange lengths, sheet width, bend rotation angle, material thickness, inside bend radius, and K-factor. The exported SVG contains the blank outline, bend line, rule metadata, and developed dimensions.

The calculator uses:

```text
bend allowance = angleRadians × (insideRadius + KFactor × thickness)
outside setback = (insideRadius + thickness) × tan(angleRadians / 2)
bend deduction = 2 × outsideSetback − bendAllowance
flatLength = outsideFlangeA + outsideFlangeB − bendDeduction
```

The supported bend angle is 15–165 degrees, thickness and sheet dimensions must be positive, radius may be zero or positive, and K-factor must be between 0 and 1. The formula assumes outside flange dimensions.

## Scope

## Multi-bend DXF

Use **File → Export multi-bend sheet-metal flat pattern DXF...** for 2–50 bends. Enter outside flange lengths in sequence (one more flange than bends), plus per-bend angles, inside radii, and K-factors. Each bend uses its own bend allowance, setback, and deduction. The calculator rejects flange segments too short for their adjacent setbacks and computes ordered bend-line center positions in the developed blank. The DXF uses millimeter units and separates the blank boundary (`OUTLINE`) from bend center lines (`BEND`). Optional end-relief width and depth create rectangular notches at both ends of each bend station; leave both at zero to disable them. Reliefs are rejected when the dimensions are incomplete, too deep, overlap, or extend beyond the blank.

This is still a development calculator, not folded sheet-solid modeling. End reliefs are rectangular blank notches, not a full corner-relief or seam solver. The app does not model folded solids, material-specific K-factor tables, springback, tool compensation, or manufacturing validation. Confirm production dimensions against the material, tooling, and shop process.

## Verification

Windows Release regression: `--sheet-metal-flat-pattern`, 41 assertions covering one-bend SVG and multi-bend DXF workflows, independent bend rules, analytical allowance/deduction/line positions, relief entities and invalid relief dimensions, millimeter units, file output, and invalid counts, angles, rules, and short flanges. No live UI test was run for this work session.

# CADWorkbench v1.3: 30 new engineering components

Each command builds one validated B-rep component and commits it as one undoable body. The Windows Release suite exercised each default parameter set, topology validity, positive volume, invalid zero-dimension rejection and undo.

| # | Component | Parameters and behavior |
|---:|---|---|
| 1 | Stepped shaft | Two coaxial cylindrical sections with independent radii and lengths. |
| 2 | Keyway shaft | Cylindrical shaft with a milled rectangular keyseat; radius, length, key width and depth are checked. |
| 3 | Splined shaft | Extruded radial spline profile with configurable tooth count and height. |
| 4 | Hubbed pulley | Rim and integral hub with a through bore. |
| 5 | V-groove pulley | Pulley rim with a circumferential toroidal belt groove and bore. |
| 6 | Multi-groove pulley | Rim with two to eight circumferential grooves and a through bore. |
| 7 | Flanged bearing | Hollow bearing body fused to an annular mounting flange. |
| 8 | Ball bearing assembly | Separate inner/outer races and an evenly spaced ring of balls in one compound. |
| 9 | Flanged pipe | Hollow tube with an integral annular end flange. |
| 10 | Bolt-circle flange | Annular plate with configurable bolt-circle radius, hole size and 3–16 holes. |
| 11 | Square flange | Square plate with center bore and four inset mounting holes. |
| 12 | Blind flange | Solid circular plate with a configurable circular bolt pattern. |
| 13 | Hollow pipe elbow | Annular torus sweep with configurable centerline radius, wall and sweep angle. |
| 14 | Hollow pipe reducer | Outer and inner circular-section lofts form a tapered reducer with constant wall thickness. |
| 15 | Hollow pipe tee | Fused run and branch tubes with corresponding internal bores. |
| 16 | Hollow Y branch | Three hollow pipe legs meeting at a configurable branch angle. |
| 17 | Pipe cap | Hollow cup with a closed end and independent cap thickness. |
| 18 | Grooved pipe plug | Plug stem with circumferential relief grooves and an integral hexagonal head. |
| 19 | Angle bracket | L-profile plate with mounting holes through both legs. |
| 20 | Gusset bracket | L-profile bracket reinforced by a triangular gusset. |
| 21 | T bracket | Extruded T-section mounting bracket. |
| 22 | U channel | Extruded U-shaped open channel section. |
| 23 | C channel | Extruded C-shaped channel section. |
| 24 | I beam | Extruded I-section with web and flanges. |
| 25 | T beam | Extruded T-section with configurable web and flange dimensions. |
| 26 | Dovetail rail | Extruded trapezoidal rail section. |
| 27 | Spur gear | Approximate straight-tooth radial profile, face width and optional bore. |
| 28 | Rack gear | Rectangular rack base with repeated triangular teeth at configurable pitch. |
| 29 | Chain sprocket | Radial tooth profile with configurable tooth count and bore. |
| 30 | Fan impeller | Hub with repeated radial blades fused into one body. |

## Geometric limits

- Boolean joins and cuts use OCCT tolerances; unusually thin or self-intersecting parameter combinations are rejected rather than committed.
- Spur gears and sprockets use a straight-sided tooth approximation, not a manufacturing-grade involute or chain roller profile.
- The ball bearing is a compound assembly; its races and balls are separate solids inside one CAD object.
- Pipe elbow geometry is an annular torus segment, and the Y branch uses straight cylindrical legs; neither is an editable swept-path feature.
- Rack teeth are triangular and evenly spaced. Component commands create direct geometry, not editable parametric timelines.

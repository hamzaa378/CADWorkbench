# CADWorkbench v1.4 — 30 advanced components

Each Create command opens a dimension dialog and adds one validated, undoable B-rep body or assembly.

| # | New component | Geometry |
|---:|---|---|
| 1 | Hex bolt | Shank with fused hex head. Threading is not modeled. |
| 2 | Socket head screw | Shank, cylindrical head and hex socket pocket. Threading is not modeled. |
| 3 | Flat washer | Annular plate with configurable bore and thickness. |
| 4 | Tooth lock washer | Washer with a radial array of fused tabs. |
| 5 | Dowel pin | Cylindrical pin with tapered end relief. |
| 6 | Clevis pin | Headed pin with transverse retaining-hole feature. |
| 7 | Taper pin | Conical locating pin with independent end radii. |
| 8 | Shaft collar | Bored collar with radial clamp-screw relief. |
| 9 | Flange coupling | Two hubs, connecting flange and bolt-circle holes. |
| 10 | Jaw coupling | Two hubs with repeated interface jaws and through bore. Elastomer is omitted. |
| 11 | Pillow block | Bearing boss fused to a base with a shaft passage. |
| 12 | Split bearing housing | Housing block with shaft passage and mounting holes. It is not split into editable halves. |
| 13 | Linear guide rail | Drilled base and extruded trapezoidal rail profile. |
| 14 | Linear bearing block | Block with a transverse guide bore and mounting holes. |
| 15 | Ball screw support | Support block, raised bearing boss and screw passage. |
| 16 | Coil spring | Circular wire swept along a sampled B-spline helix. |
| 17 | Timing pulley | Radial polygon tooth approximation, face width and bore. |
| 18 | Timing belt loop | Extruded toothed annular profile approximation; not a flexible belt or manufacturing tooth profile. |
| 19 | Brake rotor | Annular disc, raised hat and bolt-circle holes. |
| 20 | Vented brake rotor | Two annular plates joined by evenly spaced vent posts. |
| 21 | Clevis bracket | U-shaped bracket with aligned pin passages. |
| 22 | Hinge barrel | Alternating leaf and hollow knuckle solids in one assembly object. |
| 23 | Pipe union | Hollow tube and integral union nut profile. |
| 24 | Valve body | Fused run and branch casting approximation with intersecting passages. |
| 25 | Check valve body | Hollow cylindrical housing with raised bonnet approximation. |
| 26 | Reducing nozzle | Hollow conical reducer with constant radial wall allowance. |
| 27 | Heat sink | Base plate with a repeated fin array. |
| 28 | Conveyor roller | Hollow drum with continuous axle. |
| 29 | Pump impeller | Hub, back plate and repeated radial blades. |
| 30 | Universal joint | Cross and paired yoke/shaft solids grouped as an assembly. |

## Verification

The Windows Release core test exercises all 30 default configurations for parameter validation, valid OCCT topology, positive volume, document insertion and undo. This does not claim every command has been manually created in the UI. See `QA_MATRIX.md` and `manual_qa_results.md` for the live Windows checks and remaining UI scope.

## Geometry limits

Fastener shafts are smooth cylinders; threads are not modeled. Several fit-and-function components use simplified direct geometry instead of manufacturing-ready surfaces. Assembly-like components may be compounds containing multiple solids. All commands create direct geometry without an editable parametric timeline.

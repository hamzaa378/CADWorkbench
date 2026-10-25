# CADWorkbench v1.2: 30 new CAD capabilities

All 30 entries below are implemented in the v1.2 source. Core geometry, validation, and transaction paths are covered by the Windows Release test executable (440 C++ assertions; CTest 1/1). UI availability is through Create, Modify, and Inspect menus/ribbon. Live behavior should be recorded in `local_verification.json` separately from core proof.

| # | Capability | Implementation and verification |
|---:|---|---|
| 1 | Regular prism solid | Configurable 3–64 sided prism; topology and analytic volume checked. |
| 2 | Capsule solid | Cylindrical shaft with fused spherical caps; topology, volume, undo checked. |
| 3 | Ellipsoid solid | Triaxial scaled sphere; axis bounds and volume checked within OCCT spline-transform tolerance. |
| 4 | Hollow tube solid | Through-bore cylinder with inner-radius validation; volume and invalid case checked. |
| 5 | Wedge solid | Rectangular wedge with independently set top width; volume and limits checked. |
| 6 | Rectangular pyramid frustum | Lofted base and top sections; volume and top-size limits checked. |
| 7 | Hex nut solid | Hexagonal blank with through bore; volume and inradius rejection checked. |
| 8 | Ellipse sketch extrusion | Elliptical profile extruded along Z; analytic volume and topology checked. |
| 9 | Slot sketch extrusion | Obround profile extruded along Z; analytic area-volume and topology checked. |
| 10 | Triangle sketch extrusion | Equilateral triangular profile; analytic volume checked. |
| 11 | Regular polygon sketch extrusion | Configurable 3–64 sides; integer validation and octagon volume checked. |
| 12 | Rounded rectangle extrusion | Four straight edges plus tangent quarter arcs; volume, invalid radius and topology checked. |
| 13 | Ring extrusion | Annular sketch with reversed inner wire; volume, topology and invalid diameters checked. |
| 14 | Symmetric extrusion | Extrusion centered on the sketch plane; bounds, volume and undo checked. |
| 15 | Tapered extrusion | Rectangle/circle loft with signed draft angle; collapse rejection and analytic volume checked. |
| 16 | Arbitrary-plane mirror | User-defined normal and signed offset; reflected bounds and undo checked. |
| 17 | Custom-axis rotation | Arbitrary origin and direction; volume preservation and undo checked. |
| 18 | Two-direction rectangular pattern | Independent counts/spacings along two axes; atomic undo/redo and solid validity checked. |
| 19 | Counterbore hole | Blind or through drill with cylindrical head recess; removed volume and undo checked. |
| 20 | Countersink hole | Conical head recess with included angle; removed volume and invalid head size checked. |
| 21 | Rectangular-section loft | Two independently sized/positioned sections; topology, integrated volume and undo checked. |
| 22 | Fillet all edges | Applies one radius to every edge atomically; invalid radius, volume change and undo checked. |
| 23 | Chamfer all edges | Applies one distance to every edge atomically; volume change and undo checked. |
| 24 | Oriented cylinder | Cylinder on a user-defined axis and origin; volume and 3D bounds checked. |
| 25 | Oriented cone | Frustum/cone on a user-defined axis; analytic volume and topology checked. |
| 26 | Partial torus | Sweep angle from 1–360 degrees; topology, volume and invalid angle checked. |
| 27 | Rectangular hole grid | Multi-row/column hole cut committed as one operation; failed grid rollback and volume/undo checked. |
| 28 | Precise inspection bounds | Body inspector and mass-properties dialog use optimal geometric bounds, reducing mesh/tolerance inflation. |
| 29 | X/Y/Z-axis revolve | Revolve result placed around selected world axis and origin; volume and bounds checked. |
| 30 | Planar face draft | Drafts a selected planar face against a neutral plane; invalid face, volume change and undo checked. |

## Geometric limits

- All operations create direct B-rep geometry; there is no editable parametric history tree.
- Tapered extrusion supports rectangle and circle profiles. Other profiles remain available for standard/symmetric extrusion.
- Ellipsoid surfaces use an OCCT general transform; regression tolerances allow up to 0.5% for its bounds and volume integration.
- Face draft is limited to planar, cylindrical or conical faces supported by the OCCT draft builder; the current dialog identifies faces by 1-based index.

# v1.61 — 50 preliminary engineering calculations

Each row is an **implemented calculation mode** of the engineering workbench, not an independently certified CAD/FEA feature. Inputs and their units are defined in `src/core/EngineeringDesignTools.cpp`.

| # | Group | Tool | Output | Stated assumption |
|---:|---|---|---|---|
| 1 | Sections & Mass | Rectangular area (`rectangle_area`) | mm^2 | Nominal, flat rectangle. |
| 2 | Sections & Mass | Circular area (`circle_area`) | mm^2 | Nominal full circle. |
| 3 | Sections & Mass | Annular area (`annulus_area`) | mm^2 | Outer diameter must exceed inner diameter. |
| 4 | Sections & Mass | Rectangle section second moment (`rectangle_inertia`) | mm^4 | Centroidal bending axis parallel to width. |
| 5 | Sections & Mass | Circular section second moment (`circle_inertia`) | mm^4 | Centroidal diametral axis. |
| 6 | Sections & Mass | Tube section second moment (`annulus_inertia`) | mm^4 | Centroidal diametral axis; outer > inner diameter. |
| 7 | Sections & Mass | Solid cylinder volume (`cylinder_volume`) | mm^3 | Cylindrical solid, nominal dimensions. |
| 8 | Sections & Mass | Hollow cylinder volume (`tube_volume`) | mm^3 | Constant wall, outer > inner diameter. |
| 9 | Sections & Mass | Rectangular plate mass (`plate_mass`) | kg | Homogeneous material; density in kg/m^3. |
| 10 | Sections & Mass | Solid round rod mass (`rod_mass`) | kg | Homogeneous material; density in kg/m^3. |
| 11 | Stress & Strength | Axial normal stress (`axial_stress`) | MPa | Uniform axial load, no stress concentration. |
| 12 | Stress & Strength | Elastic axial strain (`axial_strain`) | 1 | Linear elastic Hooke law; E supplied in GPa. |
| 13 | Stress & Strength | Elastic bar elongation (`axial_elongation`) | mm | Uniform prismatic bar, axial load, linear elasticity. |
| 14 | Stress & Strength | Elastic bending stress (`bending_stress`) | MPa | Straight beam, sigma = M*c/I, no concentration. |
| 15 | Stress & Strength | Solid shaft torsional shear (`solid_shaft_shear`) | MPa | Circular solid shaft, Saint-Venant torsion. |
| 16 | Stress & Strength | Hollow shaft torsional shear (`tube_shaft_shear`) | MPa | Circular hollow shaft; outer > inner diameter. |
| 17 | Stress & Strength | Thin cylinder hoop stress (`cylinder_hoop_stress`) | MPa | Thin-wall membrane estimate; t << diameter; pressure in MPa. |
| 18 | Stress & Strength | Thin cylinder axial stress (`cylinder_axial_stress`) | MPa | Closed-end thin-wall membrane estimate; t << diameter. |
| 19 | Stress & Strength | Pin bearing pressure (`pin_bearing_stress`) | MPa | Average projected bearing area d*t. |
| 20 | Stress & Strength | Nominal yield safety factor (`yield_safety_factor`) | 1 | Yield stress / equivalent working stress; no code factors. |
| 21 | Beams & Columns | Simply supported, center point deflection (`simply_center_deflection`) | mm | Elastic small-deflection, central point load; constant EI. |
| 22 | Beams & Columns | Simply supported, uniform load deflection (`simply_udl_deflection`) | mm | Elastic small-deflection; w in N/mm (NOT N/m). |
| 23 | Beams & Columns | Cantilever, tip load deflection (`cantilever_tip_deflection`) | mm | Elastic small-deflection with ideal fixed support. |
| 24 | Beams & Columns | Cantilever, uniform load deflection (`cantilever_udl_deflection`) | mm | Elastic small-deflection; w in N/mm. |
| 25 | Beams & Columns | Simply supported, center load maximum moment (`simply_center_moment`) | N·mm | One central transverse point load, two simple supports. |
| 26 | Beams & Columns | Simply supported, UDL maximum moment (`simply_udl_moment`) | N·mm | Uniform load over entire span; w in N/mm. |
| 27 | Beams & Columns | Cantilever, tip load root moment (`cantilever_end_moment`) | N·mm | One transverse load at free end, ideal fixed support. |
| 28 | Beams & Columns | Cantilever, tip load slope (`cantilever_tip_slope`) | rad | Small-angle, elastic Euler-Bernoulli theory. |
| 29 | Beams & Columns | Euler elastic buckling load (`euler_column_load`) | N | Slender elastic column; effective length factor K; not a design capacity. |
| 30 | Beams & Columns | Rectangle beam maximum transverse shear (`rect_beam_max_shear`) | MPa | 1.5 V/A; homogeneous rectangular section, elastic theory. |
| 31 | Fasteners & Fits | Metric thread tensile stress area (`iso_bolt_tensile_area`) | mm^2 | Approx. ISO basic thread: A_t = pi/4*(d - 0.9382*p)^2. |
| 32 | Fasteners & Fits | Bolt nominal tensile stress (`bolt_tensile_stress`) | MPa | Gross axial force divided by bolt tensile stress area. |
| 33 | Fasteners & Fits | Torque-preload estimate (`bolt_torque_estimate`) | N·m | Empirical T=K*F*d; K depends strongly on friction. |
| 34 | Fasteners & Fits | Direct shear per bolt (`bolt_group_load`) | N | Concentric shear shared equally; excludes moment and prying. |
| 35 | Fasteners & Fits | Bolt group direct shear stress (`bolt_group_shear_stress`) | MPa | Equal sharing across bolts and one shear plane each. |
| 36 | Fasteners & Fits | Equal-leg fillet weld throat (`fillet_weld_throat`) | mm | Theoretical effective throat 0.707*leg; not a code capacity. |
| 37 | Fasteners & Fits | Fillet weld nominal shear stress (`fillet_weld_nominal_shear`) | MPa | Single effective weld length, uniform load, no eccentricity. |
| 38 | Fasteners & Fits | Minimum diametral fit clearance (`minimum_clearance`) | mm | Hole minimum minus shaft maximum; negative means interference. |
| 39 | Fasteners & Fits | Maximum diametral fit clearance (`maximum_clearance`) | mm | Hole maximum minus shaft minimum. |
| 40 | Fasteners & Fits | Nominal press-fit interference (`diametral_interference`) | mm | Shaft diameter minus hole diameter; positive indicates interference. |
| 41 | Production & Thermal | Free linear thermal expansion (`thermal_expansion`) | mm | Uniform expansion; alpha in microstrain/K; signed temperature change. |
| 42 | Production & Thermal | Fully restrained thermal stress magnitude (`thermal_stress_magnitude`) | MPa | Uniaxial linear elasticity; ideal full restraint and no yielding. |
| 43 | Production & Thermal | Three-part worst-case tolerance (`tolerance_worst_case`) | mm | Sum of nonnegative absolute dimensional tolerances. |
| 44 | Production & Thermal | Three-part statistical RSS tolerance (`tolerance_rss`) | mm | Independent zero-centered contributors, same statistical definition. |
| 45 | Production & Thermal | Sheet-metal bend allowance (`sheet_bend_allowance`) | mm | Neutral-axis estimate: theta(rad)*(inside radius + K*thickness). |
| 46 | Production & Thermal | Sheet-metal bend deduction (`sheet_bend_deduction`) | mm | BD=2*(r+t)*tan(theta/2)-BA; theta is bend angle. |
| 47 | Production & Thermal | Drilling feed time (`drilling_time`) | min | Depth / (feed per revolution * spindle speed); no peck/retract allowance. |
| 48 | Production & Thermal | Turning feed time (`turning_time`) | min | Axial pass length / (feed per revolution * spindle speed). |
| 49 | Production & Thermal | Milling feed time (`milling_time`) | min | Straight path length / programmed feed; no approach/retract. |
| 50 | Production & Thermal | Cutting-speed spindle RPM (`spindle_speed`) | rev/min | Nominal surface speed conversion for circular tool/workpiece. |

All calculations reject nonfinite inputs and out-of-domain configurations. The optional selected-body context uses only the B-rep bounding box, volume and existing assigned density.

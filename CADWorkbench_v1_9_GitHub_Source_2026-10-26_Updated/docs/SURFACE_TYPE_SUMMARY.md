# Surface type summary

The Inspect ribbon command **Surface type summary** classifies the unique faces of one selected body as planar, cylindrical, conical, spherical, toroidal, or other. It reports each category's face count and summed area in square millimeters. The report is read-only and does not alter the model.

The focused regression passed 22 assertions. It verifies a 40 × 30 × 20 mm box (six planar faces, 5,200 mm²), a radius-5 mm by 12 mm cylinder (two planar caps and one cylindrical side, with exact expected areas), and sphere, cone, and torus classifications. Missing-body and null-output cases are rejected, and queries leave scene history unchanged. The Windows x64 Release app and tests built successfully; full CTest passed 34/34. The source audit passed all 136 checks and references all 109 public `SceneDocument` operations from regression tests.

Live Windows UI verification passed on a 40 × 30 × 20 mm box: the report showed 6 planar faces and 5,200.000 mm², with zero faces in every other category. A radius-10 mm, height-35 mm cylinder reported 2 planar faces / 628.319 mm² and 1 cylindrical face / 2,199.115 mm², with no other face types. Those areas agree with the analytic cap and lateral surface formulas.

The classifier reports analytic surface types; it does not infer manufacturing intent, feature history, or material properties. Freeform and unsupported analytic surfaces are grouped under Other.

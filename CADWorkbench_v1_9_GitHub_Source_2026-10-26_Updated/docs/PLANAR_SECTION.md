# Planar section contour measurement

The Inspect ribbon command **Planar section contour** intersects one selected body with an axis-aligned plane. Choose X, Y, or Z as the plane normal, then enter the plane offset in millimeters. The report gives the unique section-edge count and the sum of their lengths. A plane that misses the body is a successful measurement with zero edges and zero length. The command does not modify the model.

For a 40 × 30 × 20 mm box, a Z-normal plane at 10 mm produces four edges with a total contour length of 140 mm. The core regression also checks an X-normal plane at 20 mm (four edges, 100 mm), a nonintersecting plane, invalid axis/offset/body/output handling, and unchanged document history. The focused Windows test passed 15 assertions. The Windows x64 Release app and test targets built successfully, and the full CTest suite passed 34/34.

Live Windows UI verification used the built Release application: created and selected a 40 × 30 × 20 mm box, chose Z as the normal, and entered a 10 mm offset. The dialog reported **4 section edges** and **140.000 mm** total contour length, matching the expected rectangular perimeter. A second live check used X = 20 mm and reported **4 section edges** and **100.000 mm**, also matching the expected perimeter.

This first version measures straight and curved contour lengths on axis-aligned planes. It does not compute filled section area or create a persistent section object.

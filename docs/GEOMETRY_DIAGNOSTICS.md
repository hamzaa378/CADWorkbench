# Geometry health and topology

Select one body and choose **Inspect → Geometry health and topology** from the menu or Inspect ribbon. The read-only report checks the Open CASCADE shape validity and reports unique solid, shell, face, wire, edge, and vertex counts; volume; surface area; axis-aligned bounds; and center of mass.

The focused regression passed 20 assertions. It checked a 10 × 12 × 14 mm box, expected unique topology counts (1 solid, 1 shell, 6 faces, 6 wires, 12 edges, 8 vertices), volume (1,680 mm³), area (856 mm²), bounds, center (5, 6, 7) mm, invalid IDs/null results, and no document-history mutation. Windows Release build passed and full CTest passed 30/30.

Live Windows UI verification passed on a 40 × 30 × 20 mm box. The dialog reported a valid closed solid, 1 solid, 1 shell, 6 faces, 6 wires, 12 edges, 8 vertices, 24,000 mm³ volume, 5,200 mm² surface area, bounds X 0–40, Y 0–30, Z 0–20 mm, and center of mass (20, 15, 10) mm.

This is a geometric validity and summary report. It does not repair invalid geometry, identify a manufacturing cause, or certify a model for production.

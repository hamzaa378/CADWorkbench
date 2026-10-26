# Mass properties from material density

Select one body and choose **Inspect → Mass properties / measure body**. Enter a material density in kg/m³. CADWorkbench reports the model's volume in mm³, surface area in mm², center of mass in mm, and estimated mass in kg. The density is used for this calculation only; it is not saved as a material assignment on the body.

Mass is computed as `volume_mm3 × density_kg_per_m3 ÷ 1,000,000,000`, converting cubic millimeters to cubic meters. The focused core regression passed 14 assertions, including a 10 × 12 × 14 mm box at 7,850 kg/m³ (0.013188 kg), invalid-density and missing-body rejection, and no document-history mutation. The Windows Release build passed and full CTest passed 34/34.

Live Windows UI verification passed on a 40 × 30 × 20 mm box at 7,850 kg/m³. The report showed 24,000 mm³ volume, 5,200 mm² area, 0.188400 kg mass, and center (20, 15, 10) mm.

This is a density-based estimate from the modeled volume. It does not assign material data, account for porosity or varying density, or include mass from separate components or fasteners.

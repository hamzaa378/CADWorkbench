# Body material assignments

Use **Edit → Assign material...** to choose a preset for the selected body or multiple selected bodies: Aluminum 6061, carbon steel, stainless steel 304, brass, copper, ABS plastic, or Nylon 6. Choose **Custom...** to enter a material name and density in kg/m³. A custom material assigned to any body becomes a reusable choice for other bodies in the same document. A multi-body assignment is one undoable change. Choose **Unassigned** to clear it. The body properties panel shows the material and density. **Inspect → Mass properties / measure body** uses the assigned density as its starting value; the dialog still allows an override for a one-time calculation.

Assignments are stored in the native scene file and participate in undo and redo. Older scene files without material fields load as unassigned. The material model stores name and density only; it does not provide thermal/strength properties, manufacturing data, or rendered appearance mapping.

## Verification

- Windows Release application and tests build successfully.
- Focused CTest passed (1/1); direct regression passed 50 assertions covering preset/custom and multi-body assignment, normalization, undo/redo, mass from assigned density, save/load, clear, invalid values, material inheritance by duplicates, mirrors, and patterns, plus matching and mixed-material Boolean unions.
- The complete Windows Release CTest suite passed 39/39.
- The source audit passed 145 checks; all 114 public scene operations have regression references.
- Live UI passed for preset assignment, custom name/density display, and density-based mass calculation. Save/reload was covered by core regression, but was not exercised in the live UI pass.

- Live Windows UI: PASS. Assigned Aluminum 6061 to a 40 × 30 × 20 mm box; properties displayed 2,700 kg/m³, and mass inspection calculated 0.064800 kg for 24,000 mm³. Assigned a custom "UI Test Alloy" at 7,850 kg/m³; the body properties retained that value, and mass inspection calculated 0.188400 kg. This was a disposable Untitled document; the live UI pass did not exercise saving/reloading materials.


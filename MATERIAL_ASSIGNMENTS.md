# Body material assignments

Use **Edit → Assign material...** to choose a preset for the selected body or multiple selected bodies: Aluminum 6061, carbon steel, stainless steel 304, brass, copper, ABS plastic, or Nylon 6. Choose **Custom...** to enter a material name and density in kg/m³. A custom material assigned to any body becomes a reusable choice for other bodies in the same document. A multi-body assignment is one undoable change. Choose **Unassigned** to clear it. The body properties panel shows the material and density. **Inspect → Mass properties / measure body** uses the assigned density as its starting value; the dialog still allows an override for a one-time calculation.

Assignments are stored in the native scene file and participate in undo and redo. Older scene files without material fields load as unassigned. Material names are trimmed, limited to 80 characters, and may not contain CR/LF line breaks; both assignments and native scene loading enforce this rule so BOM/parts-list labels remain single-line. The material model stores name and density only; it does not provide thermal/strength properties, manufacturing data, or rendered appearance mapping.

## Optional engineering record and local library (core API)

A material can additionally carry **provenance** (a user-entered source or datasheet
reference, maximum 200 characters) and **Young's modulus in GPa** (0 = unknown;
valid entered range above zero through 2000 GPa). These fields are metadata, not
verified engineering values or analysis inputs. The core API
`setMaterialEngineering(bodyIds, provenance, youngsModulusGPa)` sets them in one
undoable operation after material name/density assignment. Changing or clearing
an existing material clears its engineering record, preventing stale properties.

Native `.cadscene` JSON writes an optional per-body `material_engineering` object
with `provenance` and `youngs_modulus_gpa` keys only for populated records.
Scenes without this object load with empty provenance and unknown modulus.
Malformed record types, line breaks, nonfinite values, and out-of-range modulus
are rejected before a scene is committed. The file schema identifier is
unchanged; this is an additive optional field, not a promise of compatibility
with unknown earlier external consumers.

`documentMaterialLibrary()` derives a **document-local** list of distinct assigned
records (including provenance and modulus). It has no separate database, editing
UI, external lookup, or independently persisted catalog entries. The BOM and parts
list CSVs append provenance and modulus columns; different engineering records
are not silently grouped. No solver, strength check, or photorealistic material
rendering uses these values.

## Verification

The results below are historical results from earlier releases, not a new verification of this patch. The embedded-line-break assignment and scene-load negative tests still need to run on Windows.

- Windows Release application and tests build successfully.
- Focused CTest passed (1/1); direct regression passed 50 assertions covering preset/custom and multi-body assignment, normalization, undo/redo, mass from assigned density, save/load, clear, invalid values, material inheritance by duplicates, mirrors, and patterns, plus matching and mixed-material Boolean unions.
- The complete Windows Release CTest suite passed 39/39.
- The source audit passed 145 checks; all 114 public scene operations have regression references.
- Live UI passed for preset assignment, custom name/density display, and density-based mass calculation. Save/reload was covered by core regression, but was not exercised in the live UI pass.

- Live Windows UI: PASS. Assigned Aluminum 6061 to a 40 × 30 × 20 mm box; properties displayed 2,700 kg/m³, and mass inspection calculated 0.064800 kg for 24,000 mm³. Assigned a custom "UI Test Alloy" at 7,850 kg/m³; the body properties retained that value, and mass inspection calculated 0.188400 kg. This was a disposable Untitled document; the live UI pass did not exercise saving/reloading materials.


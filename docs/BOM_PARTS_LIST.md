# BOM and parts list

CADWorkbench v1.26.0 adds a derived assembly-structure report alongside per-body procurement metadata and slash-separated assembly paths. Set each part to **Manufactured** or **Purchased**; purchased parts can include a supplier and supplier catalogue number. BOM and parts-list views and CSV files include these fields. Native scene schema v5 stores procurement fields. Older v1-v4 scenes still load; legacy parts default to **Manufactured**. Duplicate, pattern, and mirror operations carry the source part metadata, assembly path, procurement details, and material assignment.

In v1.29-v1.30, bodies can also be marked as reusable components and inserted as fixed XYZ occurrences. The grouped BOM reports the component-definition ID/name; the per-body parts list reports each occurrence ID and its coordinates. The native schema v7 stores these records together with optional drawing setup. See [`PERSISTENT_ASSEMBLIES.md`](PERSISTENT_ASSEMBLIES.md) for behavior and limits.

## Reports

- **BOM and parts list** opens a grouped BOM tab and a per-body parts list tab.
- The **Assembly structure** tab derives nested assembly nodes from paths such as `Powertrain/Drive`; part rows appear at their path, and each assembly level reports descendant quantity, total volume, and total mass.
- Bodies with the same part number, description, revision, material/density, and geometry signature are grouped into one BOM item. Bodies without a part number remain separate so unrelated unnumbered parts are never silently merged.
- The parts list always contains one row per body, including its persistent body ID.
- Assembly paths use slash-separated names such as `Vehicle/Front Axle`. Identical part definitions in different paths appear on separate BOM rows; copies within the same path still roll up into one quantity. Paths appear in the UI report and both CSV exports.
- BOM grouping keeps purchased records from different suppliers or catalogue numbers separate. The BOM data-quality check warns when a purchased part lacks a supplier or supplier part number. This is a record completeness check, not a purchasing, price, inventory, or ERP system.
- Procurement type, supplier, and supplier part number are edited with the part number, description, revision, and assembly path as one undoable metadata change.
- Both reports include volume. When material density is assigned, they also include unit and total mass. Without a density, the mass cells are blank rather than presented as zero mass.
- The File ribbon exports a quoted, escaped CSV BOM and CSV parts list. Exports use an atomic save and report path or write failures.

## Windows verification

- Release build passed for `CADWorkbench.exe` and `cadcore_tests.exe`.
- Focused `--bom-parts-standards` regression passed **69 assertions** covering metadata and path normalization, supplier validation, undo, duplicate inheritance, assembly- and supplier-aware BOM grouping, mass and volume, CSV quoting and failure handling, missing supplier findings, duplicate part-number conflicts, scope disclosures, schema-v5 roundtrip, and scene save/reopen.
- Full CTest passed **43/43** in the v1.19 Windows Release suite.
- Live UI assigned Aluminum 6061 to a 40 × 30 × 20 mm box. BOM showed unit mass 0.064800 kg; the exported CSV showed 24,000 mm³ and 0.0648 kg.
- Live duplicate produced a grouped BOM quantity of 2, total volume 48,000 mm³, and total mass 0.129600 kg. The parts list showed two individual body rows and retained metadata/material.
- Both BOM and parts-list export dialogs saved CSVs; their file contents were reopened and inspected.
- v1.22 live Windows UI entered `PUMP-BOX-001`, `Drive housing`, and `Powertrain/Drive`. Both BOM and per-body report tabs displayed the assembly path. Save As produced a schema-v4 scene whose saved JSON retained the path and part number.
- v1.23 live Windows UI opened that schema-v4 scene, changed the part to Purchased with supplier `ACME UI Supply` and supplier part number `UI-420`, and showed all three fields in both report tabs. Save As wrote `purchased_item_ui_test.cadscene`; its JSON identifies schema v5 and retains the path and purchasing metadata. The live BOM quality check returned 0 errors and 2 warnings for the intentionally unset revision and material.
- v1.26 live Windows UI opened a `Powertrain/Drive` part and displayed parent nodes `Powertrain` and `Drive` in the new tree tab. With one 40 × 30 × 20 mm body, both levels reported quantity 1 and 24,000 mm³. After duplicating the part, both levels and the leaf showed quantity 2 and 48,000 mm³. The test-only duplicate was discarded without saving.
- v1.26 Windows x64 Release build passed; focused BOM/parts/standards regression passed 69 assertions; full CTest passed 43/43; source audit passed 219 checks.

## Limits

The geometry signature compares volume, surface area, and solid/shell/face/edge/vertex counts. This is a practical consistency screen, not a proof of exact B-rep congruence. The assembly tree is derived from path labels and reports descendant part counts, volume, and mass; it does not store editable assembly entities or independent subassembly definitions. Supplier pricing, inventory, purchase orders, and ERP/MRP integration are not implemented.

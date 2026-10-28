# Standards and data checks

## Configurable drawing checklist (v1.31)

When checking an exported SVG, the reviewer can require any combination of drawing number, description, part number, revision, drawn-by, checked-by, issue date, material, X/Y/Z envelope dimensions, projection symbol, auxiliary view, and general notes. Missing selected requirements become findings. The checklist opens with the saved project defaults; **Save selected requirements as this project's defaults** stores the selection in the `.cadscene` document. Those settings persist across save/reopen and support undo/redo. An optional source check compares an SVG's body ID and geometry token with one selected body and reports current, stale, mismatched, or missing source identity.

The model-level ISO/ASME readiness profiles also inspect saved drawing-sheet settings against these project defaults. They report missing sheets, required title-block values, required conventions, and stale or missing source bodies. **File → Refresh saved drawing source link...** updates the stored source revision after the user reviews the changed model; exporting a new SVG/PDF is still required to update the artifact. The model-level checks inspect saved settings, not the full rendered SVG or every formal standard requirement.

Scene schema v13 stores the required title-block fields and annotation conventions. Schemas v1-v12 remain loadable and receive empty checklist defaults. Unsupported or duplicate keys and malformed policy records are rejected.

The checklist is a project content-review aid. It does not establish formal ISO/ASME conformity, inspect licensed tolerance tables, verify GD&T, or provide standards certification.

The Inspect menu and ribbon provide BOM data quality, ISO drawing readiness, ASME drawing readiness, **All available checks**, and a separate **Check exported drawing SVG** command. The model-level all-checks command runs the BOM, ISO, and ASME profiles, totals their errors and warnings, and shows findings under separate profile headings. The SVG command screens an exported sheet for structural content against the selected ISO/ASME profile. These are readiness screens, not standards certification; they do not claim to validate every clause.

## Current checks

- A model with no bodies is an error.
- Each body must diagnose as one valid, positive-volume solid.
- Part numbers must be 3–32 uppercase letters/digits/dots/underscores/hyphens after the editor normalizes case.
- Reusing a part number with inconsistent description, revision, material/density, or geometry signature is an error.
- Missing part number, description, revision, or material is reported as an actionable warning.
- Purchased parts are warned when supplier or supplier part number is missing; procurement identity is included when checking duplicate-part-number consistency.
- Saved sheets are checked for project-required fields/conventions and source freshness; missing requirements are warnings so the project can define its own release gate.
- The report states that it is a data/readiness check, not third-party standards certification.

## Exported drawing content screen

The Inspect command **Check exported drawing SVG** opens an existing SVG file and checks its XML structure, A4/A3 landscape metric page dimensions and matching viewBox, front/top/right view labels, an isometric or section label, a first-angle/third-angle heading, X/Y/Z envelope dimensions, and description/part-number/revision/material title-block data. Missing orthographic views or malformed SVG are errors; incomplete page or metadata fields are warnings. **All available checks** runs the ISO and ASME screens separately and combines their findings.

These are CADWorkbench content checks only. They do not verify exact ISO 7200 field placement, ISO 129-1 dimension presentation, ISO 22081 tolerance values, ASME Y14.100 drafting requirements, Y14.5 dimensioning/GD&T, datums, or feature-control frames. They do not validate drawing line weights, projection-symbol conformance, tolerances, fonts, or every standard clause.

## Standards scope and drawing dependency

The ISO profile identifies ISO 7200 title-block fields, ISO 129-1 dimension presentation, and ISO 22081 general geometrical/size specifications as drawing work still required. The ASME profile identifies Y14.100 drawing practice and Y14.5 dimensioning/GD&T as drawing work still required. Current drawing sheets persist placed views, notes, envelope dimensions, a linked source-scoped parts list, and BOM balloons, while SVG export remains a snapshot. The content screen checks sheet structure and visible labels, but does not inspect requirements at clause level. Feature-attached dimensions, a validated standardized projection symbol, tolerances, datums, and feature-control frames remain absent. Therefore the readiness profiles still do **not** claim standards compliance.

The checker can be extended to inspect drawing content against selected user-configured conventions. It must not invent or hard-code licensed tolerance tables; ISO general tolerance values and ASME interpretation require the relevant standard tables and project specification.

## v1.46 saved-sheet project-policy checks

The ISO/ASME model readiness reports apply the saved project policy to each body with a sheet. They flag missing saved sheets, required title-block values, X/Y/Z envelope dimensions, projection, auxiliary view, general notes, and stale/missing source bodies. These are project-defined release checks; the profile labels do not turn them into formal standard conformance tests.

The Windows BOM/standards regression passes **93 assertions**, including a missing-sheet warning, a complete configured sheet, stale-source detection, refresh, and scope language. The full suite passed 51/51 for v1.46; live UI was excluded by the user request. See [`QA_MATRIX.md`](QA_MATRIX.md).

## v1.24 title-block fields, live profile checks (2026-10-27)

The drawing export now collects a drawing number, drawn-by name, checked-by name, and issue date. The exported-SVG readiness screen recognizes the drawing number as title-block identity data; an empty drawing number falls back to the part number, and missing identity is reported. These fields are XML-escaped and length/control-character validated. Live Windows UI exported a sheet with entered author/checker/date values, then ran both ISO and ASME profiles against it. Each screen reported **0 errors and 1 warning** for the intentionally unset revision, and each detail panel stated that the result is structural/content screening rather than standards certification.

## v1.25 editable drawing revision (2026-10-27)

The title-block dialog now accepts a drawing-specific revision, defaulting to the selected part's revision. It is validated to 16 printable characters and XML-escaped. Live Windows UI exported revision `A`; the saved SVG parsed successfully, and ISO and ASME content screens each reported **0 errors and 0 warnings**. The profile details still state that standard clauses, tolerance tables, datums, and GD&T are not checked and that the result is not standards certification.

## v1.27 issue-date format check (2026-10-27)

Drawing export rejects nonempty issue dates that are not exactly `YYYY-MM-DD`. The exported-SVG screen reports `DRAWING_ISSUE_DATE_FORMAT` as a warning for malformed dates in externally supplied SVGs. The final v1.27 app exported revision `A` with date `2026-10-27`; both ISO and ASME screens returned 0 errors and 0 warnings. On a second SVG containing `27/10/2026`, both profiles reported 0 errors and 1 date-format warning, and their details still disclaimed certification. This is input-format validation only; it does not establish an ISO or ASME requirement or verify clause compliance. The core regression covers both export rejection and the warning code.

## Verification status

- Focused regression `--bom-parts-standards`: **PASS, 53 assertions**.
- Focused regression `--orthographic-drawing`: **PASS, 54 assertions**, including ISO/ASME SVG structure screens and projection convention layout checks.
- Full Windows x64 Release CTest: **PASS, 43/43** for v1.22.0.
- Live Windows UI: BOM data check, ISO readiness profile, and ASME readiness profile all ran with 0 current errors and 0 current warnings on the populated two-body test model. The expanded details were inspected and correctly disclose the unimplemented drawing-level checks.
- Model-level all available checks together: **PASS** in the v1.18 live UI. A cylinder with no part metadata reported 0 errors and 12 missing-metadata warnings across the three profiles. Expanded details showed distinct BOM, ISO-readiness, and ASME-readiness headings and state that exported drawings are not inspected by this model-level command.
- Exported drawing SVG screen: **PASS** in an earlier live UI run for both ISO and ASME; the final rebuilt v1.19 app could not be reopened for UI verification because Computer Use was stopped by physical Escape. The saved A4 cylinder sheet had 0 structural errors and three warnings for intentionally blank part number, revision, and material fields; both profiles displayed their non-certification scope notes.
- v1.22 live check: ASME first-angle sheet passed with 0 errors; the intentionally misplaced-symbol fixture was rejected with 1 error. The noted third-angle SVG passed the ISO screen with 0 errors and 2 expected metadata warnings.
- Actual ISO dimensional-tolerance checks and ASME drawing/GD&T checks: **not implemented or verified**; the initial SVG view export does not provide dimensioned drawing data.


## v1.21 live Windows UI verification (2026-10-27)

The exported-SVG command was run separately for ISO and ASME on the saved first-angle and third-angle v1.21 sheets. Each of the four runs returned **0 structural errors and 3 warnings** for the intentionally unset part number, revision, and material. The expanded results state that neither profile checks standard clauses, licensed tolerance tables, datums, or GD&T.

The model-level **All available checks** command was also run on the active cylinder. It returned **0 errors and 12 warnings**. Expanded details listed four missing part metadata fields under each BOM, ISO, and ASME profile, with separate scope notes. This verifies the UI workflows and expected reporting; it is not standards certification.

## v1.22 live general-note SVG screen (2026-10-27)

The v1.22 export workflow added two user-entered notes to a third-angle A4 sheet. The saved SVG was screened through both profiles: ISO and ASME each returned **0 structural errors and 2 warnings** for the intentionally unassigned revision and material. This verifies the new notes do not break basic sheet parsing; it does not verify ISO/ASME conformance.

## v1.23 live purchased-item BOM quality screen (2026-10-27)

The schema-v4 test scene was opened in the v1.23 UI. A part was changed to Purchased with supplier `ACME UI Supply` and supplier catalogue number `UI-420`; both report tabs displayed the values. The BOM data-quality screen returned **0 errors and 2 warnings** for the intentionally unset revision and material. Expanded details confirmed those findings; supplier data had no missing-field warnings.

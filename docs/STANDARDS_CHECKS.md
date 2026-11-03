# BOM and drawing readiness checks

CADWorkbench offers project readiness checks for BOM data and drawing-sheet content. The ISO and ASME profile names select the report scope and wording; they do not certify compliance with either standard.

## Configurable saved-sheet checklist

Use **Inspect → Run BOM / ISO / ASME checks...** to choose required title-block fields and annotation-presence conventions. The checklist can be saved as project defaults in the scene file.

Supported title-block fields are drawing number, description, part number, revision, drawn by, checked by, issue date, and material. Supported annotation conventions are:

- Overall X, Y, and Z envelope dimensions
- Projection symbol / first- or third-angle projection setting
- Auxiliary view
- General notes
- Section view
- Detail view
- Visible linked parts list with drawing source bodies
- BOM balloon annotations

The saved-sheet report emits a separate finding for each required convention that is absent. It also checks for missing required fields and stale or missing drawing source bodies. The source-revision warning covers geometry and BOM/material metadata.

## Boundaries

These are configurable project presence checks. They do not validate dimension placement, tolerances, datums, geometric dimensioning and tolerancing, clause-level ISO or ASME rules, or whether a selected view is appropriate for a design. SVG and PDF exports remain snapshots. Use the findings as a project checklist, not as an engineering certification.

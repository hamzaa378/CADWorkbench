# Five profile and transition tools

These direct-solid tools are available under **Create > Profiles** and in the matching ribbon group. Every result is inserted as one undoable body.

## Elliptical prism

Extrude an ellipse by semi-axis A, semi-axis B, and height.

## Elliptical tube

Create a hollow oval tube from outer and inner ellipse semi-axes and height. Each inner semi-axis must be smaller than its corresponding outer semi-axis.

## Circular loft

Loft between bottom and top circular sections with separate radii, height, and X/Y offset for the top section.

## Rectangular-to-circular transition

Loft from a centered rectangular section to a circular section. Set the rectangle width/depth, circle radius, height, and top-section X/Y offsets.

## Hollow loft reducer

Create a hollow transition from bottom and top outer/inner radii, height, and top-section X/Y offset. Each inner radius must be smaller than the matching outer radius.

## Verification (Windows x64 Release, 2026-10-25)

- Release build: **PASS**.
- Focused regression: **PASS, 30 assertions** across all five operations, including input rejection, valid topology/positive volume, insertion/name, and undo.
- Full CTest: **PASS, 10/10 tests**.
- Live menu/dialog workflow: **PASS**. Elliptical prism, elliptical tube, circular loft, rectangular-to-circular transition, and hollow loft reducer dialogs opened and created visible bodies using their default dimensions.

These tools create direct solids. They do not add editable feature-history nodes.

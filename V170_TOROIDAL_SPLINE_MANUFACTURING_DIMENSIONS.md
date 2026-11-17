# CADWorkbench v1.70 — three engineering improvements (2026-11-06)

This package is a **source-only development candidate**, based on `CADWorkbench_v170_GeneratedCylinder_BranchRecovery_LinkedDrawing_Source_20261106.zip`. No previous executable or user document is replaced.

## 1. Generated rounded-face identity: bounded toroidal fillet edits

When a face is selected directly from the output of its parent feature, the curved-face lineage resolver now has a narrow fallback for toroidal fillet faces whose **minor radius changes by at most 20%**. The major radius, axis, and center must still agree with the saved descriptor; the old trimmed-interior witness must project into exactly one candidate face. Large radius changes, missing regions, and ambiguous candidates fail. This is **not** arbitrary Blend/BSpline reconstruction or global fillet naming.

## 2. Two-location witnesses for new Bezier / B-spline face anchors

Newly recorded freeform curved-face references now capture **two independently checked trimmed-interior points** instead of one. A successor is accepted only when both witness regions can be projected onto the same uniquely identified face within the existing bounded deformation and normal constraints. This reduces mistaken attachment after some surface splits and near-duplicate surfaces. Single-point saved descriptors remain readable for compatibility; users may reselect a face to obtain the stronger format. Some legitimate splits will now be **rejected rather than guessed**. Rigid transform lineage transports both witness points.

## 3. Model-linked manufacturing drawing dimensions

The existing selected-edge drawing operation now supports three explicit callout kinds:

* **Edge length:** a uniquely witnessed current line, arc, or supported spline edge.
* **Diameter:** the diameter of a **complete circular edge**, in an orthographic view normal to the circle's plane.
* **Center distance:** distance between **two distinct, coplanar full circular edges with parallel axes** on the same body, also viewed normally.

Each callout can carry user-entered **upper (+)** and **lower (-)** dimensional tolerances, expressed in millimetres. The exporter re-resolves both geometric witnesses against the current geometry before measuring. Invalid, deleted, ambiguous, noncoplanar, or non-circular references fail. The UI checks complete circles and view orientation before saving. These dimension types and tolerance values round-trip in optional fields of the current drawing JSON scene format; older length-only drawings use default kind `LENGTH` and zero tolerances. The SVG preserves the dimension kind, computed nominal value, and supplied tolerances in metadata and rendered text. These annotations are **not GD&T**, do not establish manufacturing capability, and do not certify drawing-standard compliance. An external SVG is a snapshot until regenerated.

## Test evidence

* Python preflight and **independent OCCT geometry**: **273 passing tests**, including new toroidal, double-witness split, and circle-center fixtures. Independent OCP Python tests do not compile native C++.
* GCC portable graph/ancestry C++ with address and undefined-behavior sanitizers: **41 + 347 + 80 checks passed**.
* Clang ancestry and feature graph with sanitizers: **41 + 347 checks passed**. Clang linked-body DAG: **80 checks passed without sanitizers**; the sanitizer-enabled compilation exceeded the tool command limit.
* The existing **130 Windows CTest registrations are preserved**, not executed in this environment. New native tests were added to the already registered `cadcore_tests.cpp` and `curved_face_lineage_v170_tests.cpp` suites but have **not been compiled or executed**.
* Linux CMake stops at the project's **intentional Windows-only guard**. The updated Qt/OpenCascade application has **not been built or GUI-tested**.

Run on Windows with the correct MSVC, Qt, vcpkg, and OpenCascade versions:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Then manually verify: render a diameter and a two-hole-center drawing with user-entered tolerance; save/reopen; edit the source and regenerate; select a partial arc or wrong view to confirm rejection; modify a toroidal fillet radius and a split freeform surface. Preserve the previous passing executable until all native checks succeed.

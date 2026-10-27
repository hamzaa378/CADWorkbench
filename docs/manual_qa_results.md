# CADWorkbench v1.9 Windows QA results

## Automated checks

- Windows x64 Release build: PASS; `CADWorkbench` and `cadcore_tests` compiled.
- Windows CTest: PASS, 1/1 (`cadcore_geometry_and_persistence`).
- Direct core regression: PASS, 4,113 C++ assertions.
- Component catalog: all 405 default templates across the v1.3-v1.9 groups passed the core checks for invalid zero dimensions, valid topology, positive volume, insertion, and undo (30 Mechanical, 30 Advanced, 35 Specialized, 70 Industrial, 70 Extended, 70 Precision & Utility, 100 Production).
- Core API coverage audit: all 62 public `SceneDocument` operation names are called by the regression suite (including selected-edge fillet/chamfer and erase/clear/new-document paths).
- Source audit: PASS, 81 repository contract checks.

## Live Windows UI checks

The packaged Windows x64 executable launched as `CADWorkbench v1.9`. The Create menu and Create ribbon each exposed all 100 Production Components commands, and the Precision & Utility group remained present.

The following workflows were exercised in the packaged application:

- Create a Box; inspect its selected-body properties (24,000 mm^3; bounds X 0 to 40, Y 0 to 30, Z 0 to 20 mm; 6 faces; 24 edges).
- Save, open the saved `.cadscene`, and create a new document.
- Undo and redo body creation.
- Export STEP and STL files.
- Change to Top view; orbit with the mouse and zoom with the wheel.
- Select a body and inspect its Properties; rename it and set its color to `#e53935`.
- Toggle body visibility off and on.
- Duplicate a body with an offset and move a body by an entered offset.
- Rotate a body 90 degrees around the world X axis; verify its bounds changed and volume remained 24,000 mm^3. Use Fit All to recenter the model.
- Uniformly scale a body by 2; verify volume changed to 192,000 mm^3 and bounds doubled, then undo and verify the original values returned.
- Mirror a selected body and verify a second body appears.
- Create a linear pattern with four total instances and verify three copies appear.
- Split a selected body with the default X plane and verify the split body is added.
- Shell a selected box with 1 mm wall thickness; verify volume changed from 24,000 to 4,164 mm^3, with 11 faces and 48 edges.
- Open the through-all hole preview, apply it, verify the body volume changed, and undo to restore 24,000 mm^3.
- Create a 3-column by 2-row hole grid and verify the resulting body has six openings.
- Select an edge and apply fillet through its preview; verify volume changed from 24,000 to 23,993.562 mm^3 and undo restored it.
- Select an edge and apply chamfer through its preview; verify volume changed from 24,000 to 23,985 mm^3.
- Union two selected bodies that meet along a face; verify the body count dropped from six to five and an `Union` result appeared, then undo and verify six bodies returned.
- Apply a circular pattern with four total instances; verify three copies appeared and the body count rose from six to nine, then undo and verify six bodies returned.
- Open the Inspect > Mass properties command; it reported a 40 x 30 x 20 mm bounding size, 24,000 mm^3 volume, 5,200 mm^2 area, and center (20, 15, 10) mm.
- Confirm the Create menu displays the primitive/profile commands and all seven component groups.

The UI run was a workflow smoke test, not an individual click-through of every command or every menu action. It covered selected transform, Boolean, shell, split, hole, pattern, and edge-finishing paths through their dialogs/previews. All 405 component defaults were exercised through the core regression; the 100 newly added Production dialogs were not each manually opened in the UI. The broader W01-W24 manual release checklist and a clean virtual-machine deployment were not completed, so the release gate remains blocked pending those checks and reviewer sign-off.

## Runtime packaging note

Launching `CADWorkbench.exe` directly from the build folder fails to locate the Qt platform plugin because that folder does not include the packaged runtime layout. The executable from the Windows package launched successfully with its bundled runtime files.

## Feature-gap comparison

The benchmark has 30 stated capability areas: 6 implemented, 4 partial, and 20 not implemented. These figures describe the documented benchmark, not every capability in AutoCAD or Fusion 360. See `FEATURE_GAP_COMPARISON.md` for definitions and scope.

## Extended reviewer pass (2026-10-25)

- Rebuilt the Windows x64 Release configuration from the source tree with the Windows build script; both `CADWorkbench` and `cadcore_tests` compiled. CTest passed 1/1 (`cadcore_geometry_and_persistence`). This pass did not change source code.
- Re-ran the dependency-free source audit: 81/81 checks passed.
- Launched the freshly built executable with its staged Qt runtime. The application opened as `CADWorkbench v1.9` without the earlier Qt platform-plugin startup error.
- Created a lofted rectangle, revolved rectangle, extruded rectangle, stepped shaft, hex bolt, and Wing rib through their dialogs. The document browser showed each resulting body.
- Added a countersink hole to the extruded rectangle. Preview and apply succeeded; the reported volume changed from 24,000 to 23,586.095 mm^3, and the face/edge counts changed from 6/24 to 8/34.
- Intersected two selected bodies; an `Intersection` result appeared and the body count changed from six to five. Undo restored the six-body state. Subtract also completed, added a `Cut` result, and undo restored the original count.
- Top, Front, Right, and Isometric camera presets changed the view. Wireframe visibly switched display mode and the view returned to shaded mode. The XY reference grid toggled on and off visibly.

### Wing rib correction and focused verification (2026-10-25)

The reviewer finding is fixed. Wing rib now uses chord length, maximum airfoil depth, rib thickness, spar opening radius, and lightening hole radius. Its generated solid is an extruded NACA-style outline with one spar opening and two lightening holes. The user requested testing only the affected feature, so verification was limited to this command and its direct regression checks.

- Focused Windows regression: PASS, 14 Wing rib assertions covering parameter labels, default construction, valid single-solid topology, airfoil profile bounds, plate thickness, through-hole detail, undo, oversized-hole rejection, and rejection without document mutation.
- Rebuilt the `CADWorkbench` and `cadcore_tests` Release targets successfully.
- Live UI: PASS. The rebuilt app launched; the Wing rib dialog showed the five domain-specific fields; accepting defaults produced the airfoil plate with three openings; Ctrl+Z removed it and returned the document to zero bodies.

The broader component catalog and unrelated workflows were not retested in this focused fix pass.

The Windows build also emitted an unreachable-code warning in `SceneDocument.cpp` around line 131 and an OCCT 8.0 deprecation warning for `TColgp_Array1OfPnt` in `AdvancedComponents.cpp` around line 185. `windeployqt` reported missing `dxcompiler.dll`/`dxil.dll` (only relevant if Direct3D 12 is used) and an unset `VCINSTALLDIR`; deployment completed. These warnings were recorded and not changed.

The source audit and this review do not substitute for the W01-W24 release checklist, clean-VM deployment, or approver sign-off. The release gate remains blocked.

## Previously untested feature dialogs - live Windows UI (2026-10-25)

In the rebuilt Windows Release app, each of the following Create commands opened its parameter dialog, accepted the displayed defaults, and added the expected body to the model browser and viewport: variable-pitch spring, tapered spring, elliptical profile sweep, elliptical prism, elliptical tube, circular loft, rectangular-to-circular transition, and hollow loft reducer.

The selected Elliptical Prism was also used for Modify > Linear pattern along vector. The dialog exposed X/Y/Z direction, total instances, and spacing; the result added two pattern copies. Modify > Circular hole pattern was then configured for a four-instance, 8 mm bolt circle and 3 mm through-all holes centered at (0, 0) on the top face. The selected body's reported volume changed from 16,964.600 mm³ to 16,116.370 mm³, and faces/edges changed from 3/6 to 7/30.

Ten Ctrl+Z steps returned the disposable Untitled test document to zero bodies. The Create ribbon displayed all eight related commands, and the Modify ribbon displayed the vector pattern and circular hole pattern buttons. Ribbon buttons were not individually activated, and this pass did not open the 100 Production Component dialogs individually.

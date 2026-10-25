# CADWorkbench v1.9 validation matrix

## Automated Windows validation

| Check | Result | Evidence |
|---|---|---|
| Windows x64 Release build | PASS | `CADWorkbench` and `cadcore_tests` targets built |
| CTest | PASS (1/1) | `cadcore_geometry_and_persistence` |
| Direct core regression | PASS (4,113 assertions) | `cadcore_tests` output |
| Component template defaults | PASS (405/405) | Invalid dimensions, topology, positive volume, insertion, and undo per template |
| Public core API coverage | PASS | Every public `SceneDocument` operation appears in the regression suite |
| Source audit | PASS (81 checks) | `scripts/source_audit.py` |

Component catalog breakdown: 30 Mechanical, 30 Advanced, 35 Specialized, 70 Industrial, 70 Extended, 70 Precision & Utility, and 100 Production templates.

## Live Windows UI validation

The packaged Windows x64 application launched. The Create menu and Create ribbon each exposed all 100 Production Components; the Precision & Utility group remained available. Live workflows covered box creation and measured properties; save/open/new; undo/redo; STEP/STL export; Top view, orbit, zoom, and Fit All; selection/properties, rename, color, and visibility; duplicate, move, rotate, scale, and mirror; linear and circular patterns; split, shell, through-all hole, hole grid, selected-edge fillet/chamfer previews; two-body union; mass-properties measurement; and Create-menu component-group visibility. See `manual_qa_results.md` for observed values.

This was a workflow smoke test. The added operations were checked through the packaged Windows UI, including undo for scale, fillet, union, and circular pattern. It did not individually open every command dialog. The core regression covers the default geometry for all 405 component templates, including every Production template.

## Release gate still required

The W01-W24 manual release checklist and clean-VM deployment have not been completed. No approver or corresponding release log paths have been entered in `windows_verification.json`; that file is intentionally absent rather than fabricated. The release gate is therefore blocked pending genuine evidence and sign-off.

## Extended reviewer pass (2026-10-25)

| Check | Result | Evidence |
|---|---|---|
| Fresh Windows Release build | PASS | `scripts/build_windows.ps1`; application and native test targets compiled |
| Native CTest | PASS (1/1) | `cadcore_geometry_and_persistence` |
| Static source audit | PASS (81/81) | `scripts/source_audit.py` |
| Loft, revolve, extrusion, and component creation | PASS | Six generated bodies visible in the model browser |
| Countersink hole | PASS | Preview/apply succeeded; volume 24,000 to 23,586.095 mm^3; faces/edges 6/24 to 8/34 |
| Boolean intersection and subtract | PASS | Results added; undo restored original body count for each |
| Top/Front/Right/Isometric camera presets | PASS | Each preset changed the viewport orientation |
| Wireframe and XY reference grid toggles | PASS | Display mode and grid visibly switched on/off |
| Production Wing rib fidelity correction | PASS | Dedicated airfoil profile, spar opening and two lightening holes; see targeted verification below |

### Focused Wing rib correction

The Wing rib now exposes chord, airfoil depth, plate thickness, spar opening radius, and lightening hole radius. The generated geometry is one valid extruded airfoil solid with three through-openings. The focused Windows regression passed 14 assertions; the rebuilt UI showed the domain-specific dialog, generated the part, and undid it successfully. Other features were not retested in this focused pass.

No source code was changed in this pass. Build warnings and the component-fidelity issue are recorded in `manual_qa_results.md`. The extended UI sampling does not cover every command dialog, and W01-W24, clean-VM deployment, and approver sign-off remain open.

## Scope and comparison

The 100 Production commands generate illustrative dimensioned direct-solid templates using nine reusable geometry families. See `FEATURES_V1_9.md` for the catalog. The comparison benchmark defines 30 capability areas: 6 implemented, 3 partial, and 21 not implemented. Threads, seals, certified profiles, internal mechanisms, joints, load ratings, tolerances, and manufacturing details may be simplified or omitted.

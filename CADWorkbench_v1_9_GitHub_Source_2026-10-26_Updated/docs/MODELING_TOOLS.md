# CADWorkbench Modeling Tools

This release adds 30 dimension-driven direct-solid modeling tools. Each tool is available from **Create > Modeling Tools** and from the **Create** ribbon's **Modeling Tools** group. Choose a command, review or edit the millimeter dimensions, then select **OK** to create an undoable body.

These are direct solids. They do not create a parametric feature timeline, and the example geometries are not manufacturing-certified designs.

## Feature verification

**Automated core status: PASS for all 30 defaults.** On Windows x64 Release, `cadcore_modeling_tools` ran 210 assertions covering specification and input count, rejection of zero dimensions without changing the document, shape validity, positive volume, insertion, and undo. This validates each tool's default parameter set and its core construction path.

**Windows UI status:** the live application launched successfully. The Create ribbon and Create > Modeling Tools menu exposed all 30 commands. A Rectangular tube was created through the menu; the dimension dialog opened and the resulting hollow body appeared in the model browser and viewport. The UI workflow was sampled with this representative command; every individual command was not manually clicked in the UI.

| # | Tool | What it creates | Dimension inputs (mm unless stated) | Core | UI |
|---:|---|---|---|---|---|
| 1 | Rectangular tube | Hollow rectangular section | Outer width, outer height, wall, length | PASS | Listed |
| 2 | Square tube | Hollow square section | Outer side, wall, length | PASS | Listed |
| 3 | Z section beam | Extruded Z-shaped beam | Flange width, overall height, web, flange, length | PASS | Listed |
| 4 | Hat channel | Hat-shaped structural section | Width, height, wall, length | PASS | Listed |
| 5 | T-slot rail | Extruded rail profile with a top slot | Width, height, slot, wall, length | PASS | Listed |
| 6 | Slotted mounting plate | Plate with two elongated mounting slots | Width, height, thickness, slot length, slot width | PASS | Listed |
| 7 | Grid-hole plate | Plate with a regular array of holes | Width, height, thickness, hole radius, spacing | PASS | Listed |
| 8 | Bolt-circle flange | Round flange with center bore and bolt pattern | Outer radius, bore radius, thickness, bolt-circle radius, hole radius, hole count | PASS | Listed |
| 9 | Square flange | Square flange with center bore and four bolt holes | Width, bore radius, thickness, bolt-hole radius, hole inset | PASS | Listed |
| 10 | Lifting lug | Lug plate with a transverse pin hole | Width, thickness, height, pin-hole radius | PASS | Listed |
| 11 | Clevis bracket | Base with two pin-hole ears | Base width, base depth, base thickness, ear thickness, pin-hole radius | PASS | Listed |
| 12 | Hinge barrel | Alternating hollow knuckles and hinge leaves | Barrel radius, bore radius, length, knuckle count | PASS | Listed |
| 13 | Ribbed plate | Base plate with raised ribs | Width, height, base thickness, rib thickness, rib count | PASS | Listed |
| 14 | Triangular gusset | Triangular reinforcement with a base flange | Leg length, width, thickness | PASS | Listed |
| 15 | Elliptical tube | Hollow elliptical section | Outer X radius, outer Y radius, wall, length | PASS | Listed |
| 16 | Tapered tube | Hollow conical reducer | Bottom radius, top radius, wall, length | PASS | Listed |
| 17 | Pipe elbow | Hollow swept toroidal elbow | Centerline radius, outer radius, wall, sweep angle (degrees) | PASS | Listed |
| 18 | Pipe tee | Hollow run with a perpendicular branch | Outer radius, inner radius, run length, branch length | PASS | Listed |
| 19 | Pipe Y-branch | Hollow run with two angled branches | Outer radius, inner radius, branch length, branch angle (degrees) | PASS | Listed |
| 20 | Concentric reducer | Hollow centered tapered transition | Small-end radius, large-end radius, wall, length | PASS | Listed |
| 21 | Eccentric reducer | Hollow offset tapered transition | Small-end radius, large-end radius, wall, length, center offset | PASS | Listed |
| 22 | Blind flange | Round end flange with bolt pattern | Outer radius, thickness, bolt-hole radius, bolt-circle radius, hole count | PASS | Listed |
| 23 | Flanged pipe | Hollow pipe with end flanges | Outer radius, inner radius, body length, flange radius, flange thickness | PASS | Listed |
| 24 | V-groove pulley | Pulley blank with a circumferential groove | Outer radius, bore radius, width, groove radius | PASS | Listed |
| 25 | Timing pulley | Toothed pulley blank | Root radius, tip radius, width, tooth count, bore radius | PASS | Listed |
| 26 | Rack gear | Linear rack with repeated teeth | Length, body height, tooth height, tooth count, face width | PASS | Listed |
| 27 | Chain sprocket | Radially toothed sprocket blank | Root radius, tip radius, width, tooth count, bore radius | PASS | Listed |
| 28 | Fan impeller | Central hub with radial blades | Outer radius, hub radius, width, blade count, blade thickness | PASS | Listed |
| 29 | Ball-bearing assembly | Inner and outer races with spherical balls | Outer radius, inner radius, width, ball radius, ball count | PASS | Listed |
| 30 | Coil spring | Helical wire swept along a coil centerline | Mean radius, wire radius, pitch, turns | PASS | Listed |

“Listed” means the command is present in the live menu and ribbon. It does not mean all 30 dialogs and results have individually had hands-on UI verification. Full commands can be exercised from the app; the core test is the repeatable geometry check for each default specification.

## Repeat the focused tests on Windows

From the project root in PowerShell:

```powershell
$env:PATH = "$PWD\vcpkg_installed\x64-windows\bin;$env:PATH"
ctest --test-dir build -C Release -R "cadcore_(modeling_tools|wing_rib)" --output-on-failure
```

Or run the modeling-tool regression directly:

```powershell
$env:PATH = "$PWD\vcpkg_installed\x64-windows\bin;$env:PATH"
.\build\Release\cadcore_tests.exe --modeling-tools
```

Expected output: `PASS: 210 assertions across 30 modeling tools`.


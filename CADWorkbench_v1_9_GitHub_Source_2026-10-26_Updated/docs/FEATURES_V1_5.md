# CADWorkbench v1.5 — 35 specialized components

Each Create command opens a dimension dialog and adds a topology-checked, undoable B-rep body or assembly.

| # | New component | Geometry scope |
|---:|---|---|
| 1 | Helical gear | Spur-profile approximation; tooth helix is not modeled. |
| 2 | Bevel gear | Conical gear blank; individual bevel teeth are not modeled. |
| 3 | Worm wheel | Radial tooth approximation; mating worm contact geometry is not modeled. |
| 4 | Worm shaft | Axial raised rings approximate thread crests; no true helical thread is modeled. |
| 5 | Planet carrier | Plate and pin bosses for 3–6 planets. |
| 6 | Planetary gear set | Compound of ring, sun and planet gear approximations; no kinematic constraints. |
| 7 | Idler pulley | Bored roller with two side flanges. |
| 8 | Timing belt tensioner | Base block and bored idler wheel. |
| 9 | Needle roller bearing | Race ring and compound roller cylinders. |
| 10 | Tapered roller bearing | Race and roller approximation; rolling contact is not modeled. |
| 11 | Thrust bearing | Two ring races and axial roller cylinders. |
| 12 | Spherical roller bearing | Race and roller approximation; spherical raceways are omitted. |
| 13 | Hydraulic cylinder | Barrel, end caps and rod as a compound; seals and ports are omitted. |
| 14 | Pneumatic cylinder | Barrel, end caps and rod as a compound; seals and ports are omitted. |
| 15 | Linear actuator | Housing, screw and mount geometry; internal drive train is omitted. |
| 16 | Tie-rod cylinder | Barrel, caps, piston rod and four external tie rods. |
| 17 | Crankshaft | Main and offset journals with simple web blocks. |
| 18 | Connecting rod | Extruded link with bored ends; split cap details are omitted. |
| 19 | Piston | Cylindrical piston with pin bore and ring grooves. |
| 20 | Piston ring | Annular ring with a straight radial gap. |
| 21 | Cylinder liner | Bored liner with one end flange. |
| 22 | Cam follower | Stud, head and roller assembled as a compound. |
| 23 | Eccentric cam | Circular cam blank with an offset bore. |
| 24 | Chain link | Side plates, pins and rollers; articulation is not simulated. |
| 25 | Roller chain segment | Repeated approximate rollers and side plates; pins are not fully detailed. |
| 26 | Double-row sprocket | Two toothed radial-profile rows with a shared axis bore. |
| 27 | Flexible bellows coupling | Hollow coupling with repeated annular corrugation approximations. |
| 28 | Disc coupling | Two bored hubs, flexible disc and bolt features as an assembly. |
| 29 | Oldham coupling | Three bored hub/disc solids; sliding tongue details are omitted. |
| 30 | Y strainer | Intersecting hollow run and angled branch; screen is omitted. |
| 31 | Hose barb fitting | Hollow tube with repeated raised barb rings. |
| 32 | Pipe elbow | Intersecting hollow straight legs; bend is not a true swept elbow. |
| 33 | Pipe cross fitting | Intersecting hollow run and branch tubes. |
| 34 | Pipe expansion joint | Hollow bellows, corrugation rings and end flanges. |
| 35 | Axial cooling fan | Frame, hub and radial blade blocks; blade airfoil is simplified. |

## Modeling limits

These tools create direct geometry and do not add editable feature-history nodes or assembly constraints. The notes above call out simplified tooth, thread, rolling-element, contact, articulation and flow-path geometry. Use these generated forms as editable starting shapes, not manufacturing-certified parts.

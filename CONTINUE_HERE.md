> **CURRENT SOURCE AND VERIFICATION, 2026-11-17:** Start with [`README.md`](README.md) and [`LATEST_VERIFIED_BASELINE.md`](LATEST_VERIFIED_BASELINE.md). Sprint 11 builds on Windows, 130/130 native CTests and 952/952 Python/source checks passed. Manual engineering acceptance remains 0/60. The historical notes below have been superseded for current build status.

---

# Latest consolidated engineering development source (2026-11-06)

Read `docs/V170_SINGLE_CONSOLIDATED_DELIVERY_20261106.md` FIRST for exact implemented scope, test evidence, limitations and the mandatory Windows release gate. This is **not yet final v1.70**. No intermediate ZIPs were distributed in this development pass. The source adds adaptive assembly-motion inspection and rebuilt configuration/BOM comparison. Preserve the last Windows-verified binary.

---

# Current workstream: CADWorkbench v1.70 Consolidated RC5 (source-only candidate)

See `V170_CONSOLIDATED_ENGINEERING_RC5_20261106.md` and
`VERIFICATION_THIS_INCREMENT.md`. RC5 adds project-native editable
configuration presets and feature-linked inspection note audit/export.
**Do not promote to final release until native MSVC/Qt/OCCT build, all 130
CTests, and interactive workflow acceptance pass.** Continue unfinished
original 50-target roadmap; 85% accepted coverage is not yet established.

> **Consolidation RC2 (development source; native verification pending):** read `V170_CONSOLIDATED_ENGINEERING_RC2_20261106.md` and `VERIFICATION_THIS_INCREMENT.md`. Selective external-source rebuild planning, redundant-sketch-constraint diagnostics, and saveable/refreshable rigid mate networks have been added. Do not promote to final v1.70 without the Windows acceptance gate.

> **Consolidation RC1 (source candidate, not production-ready):** see `V170_CONSOLIDATED_ENGINEERING_RC1.md` for completed increments, remaining Priority 1/2 blockers and the verification gates.

## v1.70 current-build kernel-history increment (2026-11-06)

Three new source changes: retain OCCT Modified/Generated history through fillets/chamfers, cylinder cuts and additive loft/sweep fusions; allow unique witnessed direct attachments to intermediate (including loft-generated) geometry; and reject stale provenance whose builder shape differs from the current result. See [docs/V170_THREE_CURRENT_KERNEL_HISTORY_UPGRADES_20261106.md](docs/V170_THREE_CURRENT_KERNEL_HISTORY_UPGRADES_20261106.md). Python/OCP reference checks and portable C++ graph planners pass; native Windows build, full CTests and GUI validation remain **unverified**. This source is not a new verified executable.

# CADWorkbench v1.70 — mixed associative DAG and witnessed-edge source update

**Windows-native verification pending.** This new increment retains all 130 registered CTests and adds regression cases in existing suites. GCC/Clang portable graph-planner checks, Python preflight and independent OpenCascade geometry reference tests do **not** establish that the full Qt/OpenCascade application builds or operates correctly on Windows.

- **Generalized mixed history:** Explicit feature dependency inputs, two-input Boolean joins, coordinated external sketch rebuilding, strictly validated source references, transactional rollback, and versioned DAG save/reopen. A compact Edit-menu authoring dialog now exposes practical sketch/transform/Boolean DAG steps.
- **Persistent topology references:** Witnessed straight/circular edges survive supported splits, arc trimming and translations; ambiguous or deleted successors fail closed. Curved-face anchors now test both unchanged and translation-compensated witness positions to avoid unnecessary invalidation from changes to the solid center of mass.
- **Scope:** 1–40 supported mixed steps on eligible ordinary solids, with at least one external sketch. No unrestricted universal topological naming for B-splines/nonrigid edits; no complete visual DAG editor or generalized inter-part feature authoring.

See `docs/V170_ASSOCIATIVE_MIXED_DAG_AND_WITNESSED_EDGES_20261106.md` for exact API contract, examples, compatibility, checks and the mandatory Windows verification command.

---

# Latest v1.70 engineering handoff — conservative manufacturing-clearance analysis

The newest source ZIP adds per-solid deviation inputs to the static assembly fit audit, worst-case geometric separation bounds, risk classification, and spreadsheet-safe CSV labels. See `docs/V170_TOLERANCE_AWARE_ASSEMBLY_CLEARANCE.md`. It retains all 130 native CTest registrations and extends existing policy/integration tests; this updated candidate **has not been built or tested under Windows**. Keep the previously passing executable. Never claim that the 130 native tests passed until MSVC, OCCT, Qt and CTest actually run on the intended Windows environment.

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

---

# CADWorkbench v1.70 — interleaved modeling, curved-face lineage and articulated closed loops (130-test development candidate)

This candidate builds on the previous **127-test development source** and adds three architectural increments:

- **Interleaved history:** A new `SceneDocument` API supports 1–40 sequential operations combining ordinary single-input shape modifiers with external-sketch bosses and pockets, in alternating order. It requires at least one external-sketch step. Source changes replay the history and downstream linked bodies atomically. Undo/redo, save/reopen checks and corruption rejection are covered by native regressions. A graphical interleaved-history authoring editor and arbitrary branch interleaving are **not** complete.
- **Curved-face anchors:** Conical and toroidal trimmed surfaces join the previous planar/cylindrical/spherical witnessed-match operations, including translation compensation and ambiguous-match rejection. This is **not universal persistent topology naming** for arbitrary B-spline/split/merge edits.
- **Articulated loops:** A bounded multi-occurrence revolute/prismatic solver resolves fully constrained joint loops, validates limits, detects singular/underconstrained configurations, and atomically stages placement updates. Solved placements persist; **live joint-graph definitions and a full mechanism GUI remain unfinished**.

Native Windows verification is pending. The **130 registered CTests have not been executed in this environment**. The original 127-test source is preserved for rollback.

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

See `docs/V170_COMPLETION_ENGINEERING_INCREMENT.md` and `docs/V170_IMPLEMENTATION_STATUS.md` for exact scope, tests and remaining roadmap gaps.

---

# Previous v1.70 increment — mixed feature history, curved face lineage and rigid assembly loops (127-test candidate)

This source builds on the previous 123-test eight-stage source. It adds:

- **Mixed modeling history on one body:** existing external sketch bosses/pockets may feed a general feature tree of supported transformations, patterned operations, cuts, blends, and profile features. Source-sketch edits replay BOTH layers, with verified save/reopen, undo and rollback. External sketch stages currently precede general feature nodes; arbitrary interleaving is not yet supported.
- **Curved-surface witness lineage:** public cylinder/sphere capture/resolve APIs, supporting some unambiguous trimmed-face splits and translations. This is not universal face naming, and face-attached polygon modeling remains planar.
- **Multi-component rigid closed-loop mates:** an atomic 2–32 occurrence network solver checks every closure edge and persists the resulting placements. This does not yet solve partially constrained articulated loops or persist live mate equations.

Read `docs/V170_THREE_GAPS_ENGINEERING_INCREMENT.md` for precise scope. **127 native Windows CTests are registered, not yet executed in this environment.** Preserve the 123-test ZIP as rollback until the Release build and 127 CTests pass.

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 127
```

---

# Current v1.70 ordered stack development candidate

The external constrained-sketch in-place boss/pocket system now supports **up to eight ordered modifiers** per eligible target. Every distinct source enters the cross-body dependency DAG, with transactional regeneration, per-stage editing, removal from the end, strict save/reopen validation, and corrupted-scene rollback. The previous two-stage source-loading field-count defect is corrected. The full 50-feature v1.70 roadmap is **not complete**. See `docs/V170_ORDERED_INPLACE_SKETCH_STACK.md` for scope and limitations.

## v1.70 major development increment — two-stage in-place features and mate UI (119 native CTests registered)

This candidate adds **two ordered external constrained-sketch in-place modifiers** on one eligible body, with edit/remove operations, a two-source graph dependency, persistence and rollback. Assembly mate authoring now exposes point/axis/plane solver inputs, not just three points. See `docs/V170_TWO_STAGE_INPLACE_AND_ADVANCED_MATES.md`.

Windows MSVC build and 119 native CTests are **not yet verified**. The full 50-feature v1.70 roadmap remains incomplete. Preserve the previously passing build and a backup of `.cadscene` projects.

## v1.70 three-foundation increment (116 native CTests registered, Windows build pending)

New implementation: unambiguous trimmed-planar-face witness lineage across supported splits/merges, **in-place** external-sketch boss/pocket on an existing body ID with transactional cross-body regeneration, and a bounded six-DOF assembly mate solver (point, axis and plane constraints; UI provides three matched point pairs). See `docs/V170_THREE_FOUNDATIONS_INCREMENT.md` for exact usage, tests and limitations. This does not complete universal persistent naming, arbitrary feature-stack modifiers, or closed-loop assembly solving.

---

# Latest v1.70 increment — connected associative sketch bosses (111-test candidate)

New Create command builds a hidden live sketch tool and visible linked additive Fuse result together. The one-solid and measurable-added-volume checks reject detached or ineffective bosses on creation, downstream regeneration, and scene load. Undo/redo, stable dependency IDs and v24 persistence use the established linked-body records. See `docs/V170_ASSOCIATIVE_SKETCH_BOSS.md`. **Windows native build and 111 CTests are pending.** The 109-test source is the development parent, not a confirmed Windows acceptance baseline.

---

# v1.70 development increment — associative sketch pockets and source retargeting

This candidate extends the 106-test external-sketch-dependency source with an **atomic cross-body sketch-driven pocket** and **live sketch-source reassignment**. The pocket creates a hidden associative cutter plus a Boolean Cut result in a single undoable operation; editing the source sketch, cutter depth/offset or sketch source regenerates downstream linked solids. Read `docs/V170_ASSOCIATIVE_SKETCH_POCKET_AND_RETARGET.md` for exact capabilities and limitations.

**Windows Release build/109 native CTests pending.** Run `scripts/verify_release_windows.ps1 -ExpectedTests 109`. The original 106-test source was not reported as Windows-verified. Preserve your last known good source and scene backups until this candidate passes.

---

# Current development: external sketch dependency (106 CTests)

A second body can extrude the current saved sketch of a source body with its own depth and normal offset. Edits to the source rebuild the derived solid and its linked Boolean descendants transactionally, including save/reopen and undo/redo. New commands are under Create and Edit. Read `docs/V170_EXTERNAL_SKETCH_DEPENDENCY.md` for exact constraints and limitations. **Windows Release compilation and 106 native CTests are not yet verified for this candidate.** Run `scripts/verify_release_windows.ps1 -ExpectedTests 106`; retain the last passing baseline until verified.

---

**Latest v1.70 development increment:** Read-only assembly fit audit with pairwise exact distance/solid-overlap measurements, clearance threshold, contact classification, snapshot invalidation, and CSV export. **104 native CTests are registered, Windows acceptance pending**. See `docs/V170_ASSEMBLY_FIT_AUDIT.md`. Run `scripts/verify_release_windows.ps1 -ExpectedTests 104` after extraction. Keep a last passing Windows build.

**Latest v1.70 increment:** Atomic multi-result linked Boolean editing and read-only dependency preview. Windows release candidate with **101 native CTests**. Run `scripts/verify_release_windows.ps1 -ExpectedTests 101`; the previous 98-test candidate is the source baseline. See `docs/V170_ATOMIC_LINKED_BATCH.md`.

## Latest v1.70 candidate: Editable cross-body Boolean DAG

The complete source now supports editing existing associative Boolean results, retargeting to later-created independent bodies, topologically ordered transitive regeneration, transactional cycle/geometry rollback, and save/reopen of non-sequential dependencies. **98 registered native Windows CTests**, awaiting Windows Release acceptance. Run `scripts/verify_release_windows.ps1 -ExpectedTests 98`. Source and kernel checks are documented in `docs/V170_EDITABLE_CROSS_BODY_DEPENDENCY_DAG.md`.

---

## Latest v1.70 increment: body-relative face and edge references

This source adds translation-aware planar-face and unique-edge anchors while
preserving old project files. See `docs/V170_BODY_RELATIVE_GEOMETRY_ANCHORS.md`.
The previous user-confirmed Windows baseline is 94/94; this source still needs
its own native 95/95 run. No new modeling operations or registered CTests
were added; three existing test executables have stronger regression cases.

> **Current v1.70 face-attached sketch increment:** builds on the user-confirmed **v1.70 advanced-profile baseline (94/94 Windows CTests)**. Adds editable face-local polygon boss/pocket operations, geometric planar-face anchors, rollback guards and new native regression coverage. The new candidate has **95 registered native tests**, awaiting Windows MSVC acceptance. See `docs/V170_FACE_ATTACHED_POLYGON.md` and `docs/V170_IMPLEMENTATION_STATUS.md`. This is not the completed 50-feature roadmap.

> **Previous v1.70 advanced-profile candidate:** four editable 3D polygon solid features (sweep polyline, sweep spline, multi-section loft, arbitrary-axis revolve) passed **94/94 Windows tests** per user verification.

# CADWorkbench v1.67 — Editable polygon solid features

**User-confirmed baseline:** v1.66, 90/90 Windows Release native tests passed. **v1.67 candidate:** four new editable polygon-profile feature nodes (extrude, pocket, revolve and loft), text/sketch-canvas straight-line profile authoring, validation, persistence and two new native regression tests. **Expected total: 92 CTests; native Windows build pending.** Run `scripts/verify_release_windows.ps1 -ExpectedTests 92` before acceptance. Read `docs/V167_SKETCH_PROFILE_FEATURES.md` and `docs/V167_VERIFICATION.md`.

**Scope:** These operations use straight-edge single-outline profiles only; not a full sketch solver link or persistent topological naming. Retain the last passing v1.66 source as the rollback baseline.

---

# CADWorkbench v1.66 — sparse DAG-aware feature regeneration

This candidate is based on the **user-confirmed v1.65 Windows Release baseline (88/88 native CTests)**. It adds cache reuse for unchanged independent branches even when an earlier feature is edited or safe branch ordering changes. The feature-tree editor previews how many outputs are reused versus rebuilt; actual Save remains transactional. New regression tests cover the portable planner and native geometry, undo/redo, persistence, and failure rollback.

**Windows acceptance:** with `VCPKG_ROOT` set, run `powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 90`. The package has passed portable-source checks, but its full Windows Qt/OCCT C++ build and 90 CTests have **not** been run in this environment. Keep v1.65 until acceptance. See `docs/V166_SPARSE_DAG_REGENERATION.md` and `docs/V166_VERIFICATION.md`.

**Limit:** only stable-ID branches with unchanged upstream dependencies are reusable. Cross-body global dirty-graph rebuilding, persistent topological naming, and arbitrary-profile solid features are not claimed in v1.66.

---

## Historical release notes (statuses below refer to their original release dates)

# CADWorkbench v1.65 — selective feature regeneration and 3D transforms

This is a **Windows build candidate** derived byte-for-byte from the user's **v1.64 87/87 native-test-passing** baseline, plus the changes recorded in `docs/V165_INCREMENTAL_FEATURE_GRAPH.md`. The geometry tree now reuses an unchanged feature prefix across edits, adds branch intersection, arbitrary-axis rotation, arbitrary-plane mirror, vector linear pattern, and arbitrary-axis circular pattern. Original saved scene schema **v24** remains compatible; intermediate B-reps are transient and rebuilt on reopen.

**Acceptance gate:** `powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 88` with `VCPKG_ROOT` configured. This environment does not provide native MSVC/Qt/OpenCascade compilation. Keep verified v1.64 until 88/88 native tests pass.

---

# v1.64 build and test handoff

This is a full source candidate derived from the verified v1.63 archive. To accept v1.64, run `powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify_release_windows.ps1 -ExpectedTests 87` on Windows with `VCPKG_ROOT=C:\vcpkg`. The four new native tests are section properties, machining estimates, tolerance stacks and B-rep cylindrical inventory. Only a Windows MSVC Release build and 87/87 native CTests can verify the full GUI and OCCT integration. See `docs/V164_ENGINEERING_WORKSHOP.md` for use and limitations. Keep verified v1.63 until then.

# CADWorkbench v1.63 — centroidal inertia and assembly center of gravity (Windows build candidate)

**Verified starting point:** The user confirmed v1.62 built on Windows and passed **81/81 CTests**, with zero failures. That remains the last confirmed good baseline. **New in v1.63:** per-body principal inertia tensors and selected/all-visible rigid-assembly center-of-gravity and inertia reporting with text export. See [the v1.63 engineering notes](docs/V163_MASS_INERTIA_ENGINEERING.md). **Expected native tests: 83**; v1.63 Windows compilation and tests have **not** been confirmed yet. Do not replace the v1.62 baseline until this candidate passes. Run `scripts/verify_release_windows.ps1 -ExpectedTests 83` on Windows.

---

# CADWorkbench v1.62 — guided design and parametric studies (v1.70 foundation increment)


**New in v1.62 (not the full v1.70 roadmap):** unit-aware dimension expression editing, external configuration variants, a Qt-independent feature graph planner, ten guided engineering studies, and live design-review TXT export. The formula engine and engineering workflows have independent native C++ regression tests, but the full Qt/OCCT Windows build is **not verified here**. Read [v1.62 details](docs/V162_ENGINEERING_FOUNDATIONS.md), [v1.70 status by target](docs/V170_IMPLEMENTATION_STATUS.md), and run `scripts/verify_release_windows.ps1` on Windows (81 expected native tests).

**Last Windows-confirmed baseline:** v1.60 native Release build **78/78 CTests passed** (user-supplied log dated 4 November 2026). This candidate is derived from that exact successful source ZIP.

**New in v1.61:** An integrated preliminary engineering calculation workbench with **50 working mathematical tools** across geometry/sections, stress/strength, beam theory, bolts/fits, and production/thermal topics. Tools have explicit units, model-bounding-dimension context, clear assumptions, invalid-input detection, and copy/export to a text report. Read [v1.61 engineering tool details](docs/V161_ENGINEERING_WORKBENCH.md).

**Important distinction:** 50 calculation modes are **not** 50 major new parametric CAD features. The latter remain a tracked [50-item v1.70 roadmap](docs/V170_FIFTY_MAJOR_FEATURE_ROADMAP.md). We are advancing that roadmap through verified increments, not shipping untested modeling functionality.

**Verification:** The standalone C++20 engineering engine has compiled under Linux g++ with strict warnings and the focused executable passed **554 checks**. A complete Windows MSVC build, new Qt dialog smoke test and **79 CTests** have **NOT been run here**. Treat this as a candidate until those pass.

For Windows: `PowerShell.exe -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1` with `VCPKG_ROOT` configured to a bootstrapped vcpkg installation.

---

# CADWorkbench v1.60 — selected-edge features and safer linked-body edits

**Baseline:** The user confirms that the v1.59.1 Windows Release build passed all **77/77 native CTests**. The v1.60 source preserves that baseline and adds **one** new regression test (expected total **78**).

**New:** Editable selected-edge fillet/chamfer feature nodes with optional stable edge anchors, topology-unique all-edge enumeration, a clearer feature-tree editor, and transactional propagation of direct all-edge and selected-edge edits into downstream Boolean bodies. See [v1.60 details and limitations](docs/V160_SELECTED_EDGE_AND_LINKED_EDIT.md).

**Status:** Supplemental source checks and independent single-edge OpenCascade reference geometry checks passed. The **v1.60 Windows Release build and 78 native CTests have not been executed here**. They must pass before calling this candidate verified. Run from the extracted source folder:

```powershell
$env:VCPKG_ROOT='C:\vcpkg'
PowerShell.exe -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1
```

---

## Historical v1.59.1 notes (below)

# CADWorkbench v1.59.1 — edge-anchor CTest hotfix (4 November 2026)

The Windows v1.59 Release build **compiled successfully**, including `CADWorkbench.exe` and test binaries. The user's actual CTest result was **76/77 PASS**: only `cadcore_associative_features_v159` failed with `Edge anchor did not resolve its original edge`.

**Fix in this candidate:** `src/core/ShapeFeatureTree.cpp` now uses `TopExp::MapShapes` and a `TopTools_IndexedMapOfShape` for both edge-anchor capture and resolution. The previous raw `TopExp_Explorer` enumerated each shared box edge twice (24 edge occurrences for 12 distinct edges) and mistakenly treated duplicate occurrences as ambiguous alternatives. A real ambiguity between distinct coincident edges is still rejected. Both operations now use the same **1-based unique-edge index**; callers must not pass a raw repeated explorer occurrence index.

**Added coverage:** the native regression exercises all twelve unique edges, invalid indices/lengths, and a truly ambiguous coincident-edge fixture; `tests/v159_source_preflight.py` guards the implementation against reverting to raw traversal, and `tests/edge_anchor_reference_smoke.py` independently demonstrates the OCCT behavior through Python/OCP.

**Status:** The corrected v1.59.1 ZIP has passed local source-contract and independent OCP reference checks, but the patched native binary has **not been compiled or run on Windows in this environment**. The expected native total remains **77 tests**, unchanged. Do not claim 77/77 until the Windows CTest run confirms it.

To verify from an extracted source directory in Windows PowerShell:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
PowerShell.exe -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1
```

For a targeted rerun after building:

```powershell
ctest --test-dir .\build -C Release --output-on-failure -R '^cadcore_associative_features_v159$'
```

The rest of the v1.59 source, including linked Boolean bodies, constraint editor, and template-based revolve/sweep/loft feature nodes, is preserved without functional changes.

---

# CADWorkbench v1.59 combined source handoff — 4 November 2026

**This is an unverified Windows Release build candidate, not a 77/77 test pass.** It includes v1.58 source and corrected `cadcore_tests.cpp` plane test fixtures, with v1.59 linked Boolean bodies, geometric face/edge hints, template-based additive revolve/sweep/loft feature nodes, and click-to-constrain sketch tools. See [docs/V159_ASSOCIATIVITY_AND_FORMS.md](docs/V159_ASSOCIATIVITY_AND_FORMS.md) for all new capability limits, Windows build steps and acceptance tests. This snapshot preserves the previous features without resetting the old Git base. **Expected 77 native tests**; the last user-confirmed full native pass remains v1.56 (74 tests). A v1.58 build compiled cadcore and CADWorkbench.exe but the original regression test target failed because of incorrect enum/struct fixture types.

---

# CADWorkbench v1.58 combined repair + feature expansion — 4 November 2026

**Start here:** The attached v1.57 Windows build failed on the duplicate name `cad::SketchPlane` (datum-plane struct versus legacy extrusion enum). This release renames the legacy enum `ExtrusionSketchPlane` throughout the application **and original C++ regression tests**, while preserving the arbitrary datum-plane struct. Read `docs/V158_BUILD_FIX_AND_FEATURES.md` and `README.md` for changes, caveats, and Windows acceptance steps. Previous user-confirmed Windows baseline: v1.56 **74/74 passed**; this source is a **candidate**, not yet Windows compiled or UI tested. Expected CTest registrations: **76**. Do not assume v1.57 or v1.58 passed a native build.

**New features:** two-click rectangles, three-click rounded slots, strictly convex polygon offset, hovered-edge alignment/straightening, 3D mirrors, linear and circular feature patterns, X/Y cylindrical cuts, rectangular face pockets/bosses, open-face shell, Z draft, limited incremental feature append, search-filtered feature browser, scrollable properties inspector, and dry-run feature graph validation. Preserves prior modeling functionality and file persistence. All newly introduced operations are restricted by their documented geometry checks and limitations.

**Next engineering focus:** stable face/edge naming, cross-body associativity, full sketch-constraint editing, graph-node revolve/sweep/loft, and measured selective regeneration. Do not reset the existing source working tree.

---

# Historical v1.57 development handoff — 4 November 2026

**Starting point:** User-confirmed v1.56 x64 Windows build and **74/74 CTest PASS**, plus screenshots of the working sketch canvas. This folder is a subsequent **v1.57 source candidate**, whose new native C++ tests and live Qt UI have not been built or run in the Linux artifact environment. All preceding source changes have been preserved. The new CTest target brings expected registration to **75**.

**Added here:** `src/core/SketchProfile.*` with custom datum-plane extrusion, three-point arcs and cubic Bézier outline edges; persistent arc/spline records in `SketchSolver.*`; per-body explicit feature-node graph sources and branch Booleans in `ShapeFeatureTree.cpp`; planar face-index pocket/boss; transactional edit-sketch-and-replay-downstream modifiers in `SceneDocument.cpp`; versioned v24 scene saving with legacy v23 support; UI datum-plane/curve tools, face-sketch creation, graph-source selection and shorter ribbon labels; `tests/dependency_sketch_v157_tests.cpp` and `tests/dependency_sketch_v157_contract.py`.

**Full scope, instructions, compatibility and limitations:** `docs/DEPENDENCY_SKETCH_FEATURES_V157.md`. Do not report 75/75 until Windows actually builds and passes. The feature graph currently works **within one body**. A selected face seeds an independent sketch plane rather than a persistent linked face; face indices are not stable topological names.

Run on Windows with a bootstrapped vcpkg:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\build_windows.ps1
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
```

Keep this complete snapshot; do not reset to an older base commit or omit uncommitted historical features. Older stage statements in the following historical handoff may describe capabilities as absent which have since been implemented.

---

# CADWorkbench source handoff

This is the current **v1.55.0 source working tree** on 3 November 2026 (India time). The source ZIP is a snapshot of file contents, including files that were staged, unstaged, or untracked in the local Git checkout. It deliberately omits `.git`, compiled binaries, dependency installs, and caches. The checkout base commit was `b050654e5ff948302ee10357d39e2c6554bb9c93`; the working-tree changes have **not** been made into a new commit.

## New v1.56 development candidate — 4 November 2026 (not Windows-verified)

The incoming v1.55 development source was reported **73/73 CTest PASS** on November 4, 2026. This new candidate adds a persistent XY sketch canvas (`src/ui/SketchCanvasDialog.*`), a shape-agnostic ordered B-rep modifier stack (`src/core/ShapeFeatureTree.*`), scene schema **v23**, UI feature browser/editor, stale-stack audit findings, and `tests/shape_feature_tree_tests.cpp`. The new test raises the expected configured count to **74**. See `docs/INTERACTIVE_SKETCH_AND_GENERIC_FEATURES_V156.md` for exact scope, known limitations, and manual acceptance tests. The editor and new native tests have **not** been compiled or GUI-tested in this environment. Preserve the incoming 73-tested source, and verify all new results with Windows Release/CTest.

## Post-snapshot source increment (2026-11-04; not Windows-verified)

This working directory now includes a new project-wide health audit, a `Ctrl+K` command search, native audit CTest source, and a structured feature roadmap. See `docs/DEVELOPMENT_INCREMENT_NOTES.md` and `docs/ROADMAP_TO_PROFESSIONAL_CAD.md`. The 72/72 Windows test result below **predates** these additions and must not be treated as proof that they compile or work in the live UI. Preserve both the original v1.55 functionality and this new development increment.

## Start here in another chat

1. Extract the ZIP and point the chat at the extracted `CADWorkbench_v1_55_Source` folder, or work in the existing checkout if it is still available.
2. Read this file, `CMakeLists.txt`, `README.md`, and the source files for the area being changed. Treat older feature tables and QA notes as historical where they disagree with this handoff.
3. Preserve the current source. Do not reset to the base Git commit: that would discard the features summarized below.
4. On Windows, set `VCPKG_ROOT` to a bootstrapped vcpkg checkout and run `./scripts/build_windows.ps1` in PowerShell. It configures, builds, and runs CTest. A C++20 MSVC toolchain, Visual Studio CMake tools, Qt 6, and OCCT 8 through `vcpkg.json` are required.
5. If using the existing local build, its directory is `../build_v148_combined`. The ZIP does not include that directory, so a fresh extraction must build dependencies and compile again.

Suggested message for the next chat: **“Continue CADWorkbench from this source snapshot. Read CONTINUE_HERE.md, inspect the code for the area you choose, implement a substantial next CAD improvement, and verify it with a Windows Release build and affected regression tests. Preserve all current source changes.”**

## Verified state of this snapshot

- Windows x64 Release `ALL_BUILD`: **passed**.
- CTest: **72/72 passed**, most recently on 3 November 2026. The suite includes the long geometry/persistence case and focused sketch, surface, mesh, document-revision, assembly, drawing, parametric, BOM, and sheet-metal cases.
- Supplemental drawing and parameter source checks: **15/15 passed**. The separate CadQuery/OCP reference smoke test could not run in the available Python installations because `cadquery` is not installed; it is independent of the native C++ suite.
- The current interactive application UI was **not** live-tested in the last coding rounds. Menu commands compiled; GUI behavior needs separate verification before claiming it was observed live.
- Native source, CMake setup, vcpkg manifest, PowerShell scripts, C++ tests, supplemental Python source tests, and current documentation are in the ZIP. Runtime Qt/OCCT DLLs and an installer are **not** part of this source ZIP.

## Important implemented additions beyond older v1.55 notes

| Area | Current implementation | Main files |
|---|---|---|
| Sketches | General numeric 2D point/segment/circle solver, radius/equal-radius/concentric/point-on-circle/line-circle tangency, point symmetry about a line, external/internal circle tangency, finite point-on-segment, segment length, signed point-to-line/circle offsets, and segment angle from X. Constraint persistence and editable constrained polygon extrusions with circular cutouts are supported. Failed solves report the largest residual by constraint index. | `src/core/SketchSolver.*`, `src/core/SceneDocument.*`, `tests/sketch_solver_tests.cpp` |
| Parametric history | Editable rectangle extrusion, through/blind hole, X/Y linear hole patterns including blind holes, named dimensions and links, all-edge chamfer/fillet, optional-step suppression, and transactional batch suppression with one undo step. It remains a bounded chain. | `src/core/SceneDocument.*`, `src/ui/MainWindow.cpp`, `tests/cadcore_tests.cpp` |
| Assemblies | Reusable occurrences support XYZ placement and roll/pitch/yaw orientation, saved rigid/revolute/prismatic joints, motion tracks, BOM-linked definition synchronization, sampled pairwise clearance/overlap and all-pairs overlap scans. Scans include saved keyframe times and preserve the assembly pose. Pairwise CSV includes closest-point coordinates; all-pairs contacts also export as CSV. | `src/core/SceneDocument.*`, `src/core/PartsData.cpp`, `src/ui/MainWindow.cpp`, `tests/cadcore_tests.cpp` |
| Drawings | Persistent source-linked sheets, selected-edge dimensions guarded by geometry revision, SVG/PDF export, and editable DXF export with separate geometry/hidden/center/dimension/detail layers and clipped detail linework. Exports remain snapshots. | `src/core/DrawingSheet.*`, `src/core/DrawingDxf.cpp`, `src/ui/MainWindow.cpp` |
| Sheet metal | Editable faceted folded bodies, per-bend K factors, optional rectangular bend-end reliefs in folded flat-pattern DXF, bend-table CSV with signed directions/radii/allowances/deductions/stations, and project-supplied shop checks for folded-body radius-to-thickness ratio and straight flange length. | `src/core/SheetMetal.*`, `src/ui/MainWindow.cpp`, `tests/cadcore_tests.cpp` |
| Other areas | STEP assembly exchange, BOM/parts lists and configurable drawing checklists, persistent surface bodies, mesh repair/intake, and named offline document revisions are present. A checkpoint can be compared with the current saved scene; the UI shows a scrollable comparison report that can be saved as text. | `src/core/SceneDocument.*`, `src/core/SurfaceModeling.*`, `src/core/MeshRepair.*`, `src/core/DocumentRevisionHistory.*` |

## Boundaries and next work

- The sketch solver covers points, segments, and circles; it does not yet provide drawn arcs, splines, or a full sketch-canvas editor. The parametric timeline is not a general dependency graph and cannot arbitrarily reorder or insert operations.
- Assembly solving is bounded; closed-loop constraint solving, mating workflows, and continuous collision/contact studies remain open. Motion scans evaluate requested uniform positions and in-range saved keyframes, with at most 256 combined positions; collisions between them may still be missed. All-pairs scans cover at most 20 solid bodies.
- Saved drawing records are linked to model identities and revisions, but exported SVG/PDF/DXF files do not update themselves. More edge/face dimension types, arbitrary sections, and automatic sheet refresh remain open.
- Folded bodies are faceted; flat-pattern export is a developed snapshot. Exact analytic bends, flat-to-folded associativity, springback, tooling compensation, and formal manufacturing validation remain open.
- The ISO/ASME project checks are configurable completeness checks, not a claim of formal standards compliance. CAM, FEA, and cloud workflows are not included.
- `docs/README_HISTORY.md`, `docs/QA_MATRIX.md`, `docs/local_verification.json`, and some older feature tables contain stage-specific statements such as “blind holes cannot be patterned” or “folded DXF has no relief.” Those statements predate this working tree. Use the root README, code, and fresh tests as the authority for current behavior.

## Rebuild and publish

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\build_windows.ps1
```

For a manual regression run after a successful build:

```powershell
ctest --test-dir .\build -C Release --output-on-failure --parallel 8
```

The source ZIP is ready to **extract and upload as repository files**. GitHub does not expose or compile source contained only inside a ZIP. The repository has no project license file yet; choose one before presenting the code as an open-source release. The existing `scripts/package_release_windows.ps1` is for a separate portable app and installer workflow and requires an installed Windows Qt/OCCT runtime.

### Latest v1.70 increment — Boolean provenance with downstream regenerate

- See `docs/V170_BOOLEAN_PROVENANCE_AND_DOWNSTREAM_REGEN_20261106.md`.
- Candidate adds the `referenceSourceId` direct Boolean ancestor link,
  temporary OCCT kernel ancestry mapping, and planar design-offset correction.
- New UI entries: selected-edge fillet/chamfer from the frozen base after a
  sketch Boolean or two-input Boolean containing that base.
- Run `scripts/verify_release_windows.ps1 -ExpectedTests 130` with the
  documented Windows toolchain. Do not promote until native acceptance passes.

### Current source candidate: three engineering improvements (2026-11-06)

- `docs/V170_THREE_NEXT_IMPROVEMENTS_20261106.md` documents unique multi-hop
  Boolean lineage, intermediate geometry preview/selection, and face machining
  with history edit dependency safeguards.
- The verified baseline is still the prior Windows executable: **do not**
  describe this candidate as Windows-compiled or native-test-passing.
- Portable planner: `tests/boolean_lineage_path_v170_tests.cpp`. Native
  integration cases extended in `tests/interleaved_history_v170_tests.cpp`.
- Required gate: `scripts/verify_release_windows.ps1 -ExpectedTests 130`.


## 2026-11-06: curved shell, spline edges, and rigid ancestry candidate

See [docs/V170_SPLINE_RIGID_CURVED_SHELL_INCREMENT_20261106.md](docs/V170_SPLINE_RIGID_CURVED_SHELL_INCREMENT_20261106.md). This source extends NURBS/Bezier edge witness anchors (v3), tracks exact rigid reference hops in mixed histories, and introduces persistent curved shell-opening references and compact editor selection. **All 130 Windows-native CTests remain unverified for this increment; keep the last verified executable.**


### 2026-11-06 — Three high-impact engineering improvements

Current increment (2026-11-06): bounded spline/Bezier surface reference matching, all-route Boolean merge identity checks, and explicit pattern-copy selection with versioned v4 serialization. See `docs/V170_THREE_HIGH_IMPACT_SURFACE_MERGE_PATTERN_20261106.md`. The last verified Windows build remains the safe baseline.


### Latest source candidate — generated face witnesses, cap motion, drawing revisions (2026-11-06)

See `docs/V170_GENERATED_FACE_SPLIT_DRAWING_INCREMENT_20261106.md`. Added a secondary trimmed-interior planar-face witness, bounded normal-only generated-cap relocation for directly attached machining features, one-click regeneration of saved SVG sheets, and metadata-revision checking in drawing snapshots. Python/source/OCCT: 259 passed. GCC and Clang portable planner tests: 464 each with sanitizers. **Windows native build, CTest 130/130, and GUI remain unverified.**


## 2026-11-06: Three additional engineering improvements

See `docs/V170_GENERATED_CYLINDER_BRANCHES_LINKED_DRAWING_20261106.md` and
`VERIFICATION_THIS_INCREMENT.md`. Added bounded coaxial cylindrical-face
recovery for direct generated-face references, 32-path bounded Boolean ancestry
planning, and witnessed selected-edge dimensions that recalculate on saved SVG
regeneration. Portable/independent checks pass; native Windows build remains
pending. Keep the last Windows-verified release as the baseline.

## Latest source-only increment — 2026-11-06: rounded faces, freeform witness pairs, drawing tolerances

Read `V170_TOROIDAL_SPLINE_MANUFACTURING_DIMENSIONS.md`. New functions/fields: `findDirectStageFilletTorus`, `curved_witness2_{x,y,z}`, and linked-dimension `kind`, `secondEdgeAnchor`, `upperToleranceMm`, `lowerToleranceMm`. The portable regression tests and independent OCP cases pass, but native Qt/MSVC compilation and all 130 registered CTests have **not** been run. Keep the last passing Windows executable.

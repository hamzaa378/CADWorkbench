# CADWorkbench v1.70 — checkpoint 2 of 3 (PRELIMINARY)

Date: 2026-11-08. Based on `CADWorkbench_v170_Checkpoint1of3_DetailedAssociativeFlat_PRELIMINARY_20261108.zip`. **Development source, not a Windows-verified release.** Preserve the last Windows-tested baseline separately.

## Functional changes

### 1. Persistent Boolean edge witnesses (parametric modeling)

- `rebaseFeatureThroughBoolean` for selected-edge fillets/chamfers now carries forward the *original physical design point*, curve kind, tangent and curvature after each kernel-confirmed Boolean descendant. It still captures the descendant's current edge index, body reference and geometry descriptor for the current rebuilt shape.
- Every hop still resolves the old witness on the current output and checks actual `Modified`/`Generated`/unchanged OCCT provenance; points removed or positioned on split boundaries are rejected. There is no nearest-edge fallback.
- `face_lineage_v170_tests.cpp` adds two successive true Boolean edge trims with no witness drift, followed by resolving the twice-trimmed successor. The independent Python OCCT test confirms the proposed 3-D cuts are valid; it does **not** validate the C++ rebase function.
- **Not solved:** arbitrary topology naming across sweeping surface replacements, all generated faces, changing curve types, and multiple equally valid descendants. This is a bounded history-preserving improvement, not a universal persistent naming system.

### 2. Live articulated joint-definition editing (assembly engineering)

- New Assembly menu action: **Edit articulated joint axis, limits and reference offset...**.
- Edit an existing driven revolute/prismatic joint's normalized axis, zero-reference **translation**, driving coordinate and explicit lower/upper limits. Joint endpoints, kind and zero-frame orientation are deliberately not edited by this control.
- A read-only preview stages the entire saved joint graph through the same kinematic solver and OCCT placement rebuild. For up to 12 occurrences it also reports nominal pairwise static fit, with a user-specified clearance threshold. It never certifies swept-motion clearance.
- Commit is one undoable transaction. Invalid joints/axes, inconsistent loop closure and unrepresentable B-reps fail without modifying existing parts, saved joints or the caller report. Unchanged definitions do not create a revision.
- Existing multi-drive changes now preserve meaningful explicit limits and reject out-of-range requests; legacy saved drives with a locked `min=max` keep their prior unconstrained-coordinate editing behavior. Edited joints and limits retain schema-24 save/reopen persistence.
- **Not solved:** complete GUI graph reparenting/retyping, arbitrary frame rotations, mechanism dynamics, general singularities and robust all-path motion collision certification.

### 3. Add/remove flanges without orphaning detailed linked blanks (drawings/sheet metal)

- Added `editFoldedSheetMetalWithDevelopments`: a structural change can provide explicitly revised per-bend K-factor lists for the selected source's linked flat blank(s). It stages and validates the folded solid and regenerates all linked neutral blanks in dependency order before a single undo checkpoint.
- The Folded Sheet Metal editor now detects a change in bend count, displays a confirmation dialog with one editable K-factor list per associated blank, and suggests `0.33` for newly added bends. Users must review and accept these factors; old factors are not silently remapped when the topology changes.
- Invalid length, factor values, unrelated blank IDs or failed OCCT relief geometry reject the complete edit. Source IDs, linked blank IDs, dependency provenance, relief dimensions, and unchanged-schema persistence are retained.
- The original `editFoldedSheetMetal` API is still supported for unchanged bend counts and legacy callers. A structural change with custom linked K-factors but no explicit new mapping deliberately fails closed.
- **Not solved:** true 3-D unfold of arbitrary material-dependent bends, shaped relief families beyond rectangular, hem/corner treatment, springback, automated tooling constraints or certified flat-pattern manufacturability.

## Tests and verification

- **Python/source checks:** 440/440 passed. This includes source-contract checks and independent Python OCCT reference geometry. These are *not* native CADWorkbench integration tests.
- **Portable GCC + ASan/UBSan:** 14/14 C++ targets passed.
- **Portable Clang + ASan/UBSan:** 14/14 C++ targets passed.
- **Independent OCCT two-cut geometry:** valid solid volumes approximately 1000, 988 and 976 mm³. Does not compile or execute C++ topology rebasing.
- **CMake-native CTest registrations:** 130, unchanged. New Qt/OCCT test cases are appended to existing test binaries.
- **MSVC Release compilation, GUI tests and 130 Windows CTests:** not run on this machine. Original face-machining failure (`cadcore_v170_interleaved_history`) remains awaiting a Windows retest. Do not promote to final without these tests.

### Required Windows verification

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

Test **both** the targeted `cadcore_v170_face_lineage`, `cadcore_v170_articulated_scene_transaction`, `cadcore_geometry_and_persistence` scenarios and the full 130-test suite; inspect GUI behavior for the new editing actions. Any failure keeps the preliminary snapshot unapproved.

## Provisional functional estimates (not acceptance scores)

| Workstream | Checkpoint 1 estimate | Checkpoint 2 estimate | Required final |
|---|---:|---:|---:|
| Parametric modeling | 69% | **72%** | 85% |
| Assembly engineering | 74% | **78%** | 85% |
| Drawings / sheet metal | 61% | **64%** | 65% |

Percentage increments (+3, +4, +3 points) reflect relative scope of the integrated functionality, **not formally passed acceptance scenarios**. The final gate remains any TWO workstreams >=85% and the THIRD >=65%, with mandatory critical tests and Windows build/CTest evidence. Do not assume checkpoint 3 will automatically achieve this gate; it must be earned by working workflows and tests.

## Checkpoint 3 priorities

1. Parametric: generalize reference ancestry for more split/merge/curved edge-face cases with stable dependent feature regeneration, and complete GUI fallback for missing references.
2. Assembly: enable editing joint frame rotations and graph connections with deterministic closed-loop conflict diagnostics and motion/clearance acceptance scenarios.
3. Sheet/drawings: reach the minimum 65% via tested flange/relief editing with save/reopen, undo/redo, and associative drawing updates.
4. Close the existing face-machining regression and obtain the full Windows-native evidence before final release.

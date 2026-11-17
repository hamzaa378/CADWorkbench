# CADWorkbench v1.70: Native CTest remediation candidate

## Evidence and baseline

The developer-provided Windows MSVC build log on 2026-11-06 shows that `CADWorkbench.exe` compiled and linked. The full native run completed **124/130** tests successfully, with these six failures:

- `cadcore_v170_assembly_mate_document` — consistent rigid-loop test fixture
- `cadcore_v170_curved_face_lineage` — displaced, unrelated Bezier surface accepted
- `cadcore_v170_mixed_feature_history` — valid combined sketch-stack/shape-stack project refused on load
- `cadcore_v170_inplace_sketch_persistence` — changed saved modifier parameters accepted
- `cadcore_v170_inplace_two_stage_corruption` — changed second modifier accepted
- `cadcore_v170_interleaved_history` — ambiguous diamond test needs differing reference routes

## Actual changes

1. **SceneDocument.cpp** permits reloading a legitimate in-place sketch modifier followed by a shape-feature tree. These are built in that order; a project containing both must be legal. Earlier rejection of the combination was inconsistent with save and edit behavior.
2. A **per-in-place-feature SHA-256 consistency checksum** is written on new project saves and checked before processing any link parameters. It covers both the first and all later modifier parameters. Legacy files without this optional checksum are still parsed, with the existing geometric replay and bounds checks. **This is corruption detection, not a cryptographically authenticated signature.** The previous tests modify new saved files without recomputing the checksum, and should now reject their mutations.
3. **ShapeFeatureTree.cpp** no longer infers arbitrary rigid movement of a freeform surface simply from whole-body center-of-mass displacement. Supported nearby deformation at the saved witnesses still works. Explicitly recorded rigid feature steps retain their own reference-transform path.
4. **Assembly mate test** now uses identical world points for an already satisfied closure link. The former fixture used 18 and 35 as mismatched observations and was geometrically inconsistent. It also explicitly verifies that an inconsistent additional edge is rejected atomically.
5. **Interleaved history test** now constructs two *different* traced geometries for the branch disagreement case. Convergent routes that genuinely identify the same successor should remain legal; contradictory routes must still fail with the geometry preserved.
6. **RUN_PREVIOUS_FAILURES.ps1** builds the full application and runs exactly the six prior native tests before the full 130-case suite.

## Verification status

The Python/source/reference regression suite passes **321** tests. The portable C++ suites passed under GCC and Clang. These tests do not compile the edited Qt/OpenCascade modules. **The above six native CTests and native application have NOT been rerun on Windows after the changes.** Retain the last stable executable until all Windows gates pass.

## Run on Windows

From the extracted project root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_PREVIOUS_FAILURES.ps1 -VcpkgRoot C:\vcpkg
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

## Original 50-target roadmap

The roadmap is **not fully accepted**. These changes primarily address integration and regression of partial requirements #1, #2, #11, #23, #24, and #50. They do not implement production-ready CAM, complete exploded-view authoring, or certify all partially implemented targets. The status ledger in `docs/V170_IMPLEMENTATION_STATUS.md` remains authoritative until each acceptance criterion is demonstrated.

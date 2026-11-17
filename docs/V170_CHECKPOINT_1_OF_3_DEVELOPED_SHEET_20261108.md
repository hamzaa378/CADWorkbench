# CADWorkbench v1.70 — three-checkpoint program, checkpoint 1/3 (PRELIMINARY)

Date: 2026-11-08. Derived from `CADWorkbench_v170_DeepTopology_MultiDrive_LinkedFlat_PRELIMINARY_20261108.zip` and retains its sources and outstanding Windows regression requirements. **This is not a verified final release.**

## What changed (functional, not a report)

1. Associated flat blanks can now store independent **per-bend neutral-axis K-factors** and **rectangular bend-end relief width/depth**. The previous empty-K/unrelieved format is still supported as a uniform K=0.33 rectangular development.
2. The 3-D blank uses the **same DXF-validated bend-line stations** as the exported manufacturing flat pattern. It subtracts two through-thickness reliefs at each bend with OCCT Booleans, validates the result and confirms its volume against the nominal developed area.
3. A new **Edit linked flat blank K-factors and relief...** action edits these settings in place while preserving the derived part ID. A rejected edit leaves the solid untouched; a no-op does not create a revision. Source flange edits regenerate the derived shape in the same linked-body transaction.
4. Save/reopen uses the current schema 24. Old `neutral-flat-blank-v1` link records remain loadable; nondefault settings use `developed-flat-blank-v2` with strict JSON validation. Save/reopen compares the actual derived solid with the regenerated expected shape.
5. Native C++ cases were added to the existing `cadcore_geometry_and_persistence` target for relief volume, invalid edits, undo/redo, source edits, save/reopen and provenance. No new CTest targets were registered.

**Engineering limits:** Rectangular bend-end relief only; no corner or bend-overlap treatment, tear-drop relief, hemming, material-specific K-factor library, validated manufacturing springback, collision-free refolding or production qualification. A detailed blank does **not** certify that the corresponding physical folded part can be formed. `relief.depth < sheet width/2` and non-overlapping stations are enforced. The DXF still needs manufacturing review.

## Verification performed on this checkpoint

- `python -m unittest discover -s tests -p '*.py' -q`: **429 passed**, including 3 additional independent Python OCC geometry reference cases and six new source-integration tests. **These are not native CADWorkbench tests.**
- Portable C++ (GCC ASan/UBSan): **14/14 targets pass**.
- Portable C++ (Clang ASan/UBSan): **14/14 targets pass**.
- CMake source declares **130** existing native CTest registrations; Windows discovery/compilation was **not run**.
- New native Qt/OCCT code and save/reopen assertions: **NOT EXECUTED** in this environment.
- Original `cadcore_v170_interleaved_history` / face-machining failure: **Windows retest pending**.

## Provisional functional coverage, NOT acceptance scores

| Workstream | Prior estimate | This snapshot | Target |
|---|---:|---:|---:|
| Parametric modeling | 69% | 69% | 85% |
| Assembly engineering | 74% | 74% | 85% |
| Drawings & sheet metal | 57% | 61% | 65% |

The increase applies only to the drawings/sheet-metal category and is a planning estimate. No new credit has been assigned to modeling or assembly. Only independently demonstrated end-to-end acceptance scenarios count towards final 85/85/65 verification.

## Remaining proposed milestones (aspirational, not assured)

- **Checkpoint 2/3 target:** Parametric feature-history/topology rebind work, articulated assembly reference selection and recovery, finish associative detailed sheet-metal edits. Planning targets ~77% modeling, ~80% assembly, >=65% drawings; formal scores may be lower.
- **Checkpoint 3/3 target:** Resolve highest-impact unverified topology and mate problems, complete native Windows/GUI acceptance matrix, and target 85% in modeling and assembly with 65% in drawings/sheet metal. If the acceptance evidence falls short, produce another explicitly preliminary checkpoint rather than mislabel it as a final release.

### Mandatory Windows checks before promotion

From a **new extraction**:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg
```

All **130/130** native tests must pass including the original face-machining regression. Then exercise interactive editing of a linked flat blank, edit a source flange, save/reopen, and compare the developed part with exported DXF. Preserve the last tested build as rollback baseline until successful verification.

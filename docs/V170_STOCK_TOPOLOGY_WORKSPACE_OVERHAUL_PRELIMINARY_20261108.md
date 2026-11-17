# CADWorkbench v1.70 — three-workstream functional expansion (PRELIMINARY)

## Source base and compatibility

Based on `CADWorkbench_v170_MajorWorkflowExpansion_ParametricAlternatives_MotionEnvelope_MultiPartMfg_PRELIMINARY_20261108.zip` without disabling any previous native test or changing the persisted document schema. The two previously observed Windows native failures are **not fixed or suppressed** here: `cadcore_v170_assembly_mate_document` (clearance-report assertion) and `cadcore_folded_sheet_metal` (segmentation fault). The last user-supplied Windows build passed 128/130 CTests on the preceding baseline. This modified archive has **NOT** been rebuilt under Windows/MSVC and retains preliminary status.

## Parametric modeling: per-body B-rep impact review

The detached `evaluateParametricDesignAlternatives` evaluator now records actual old/new volumes, face counts, and edge counts for each regenerated solid (including dependent bodies). The design-alternative GUI presents the per-body deltas and highlights changed topological counts before a single undoable commit. **Limit:** this is a topology *risk indicator*, not proof that every face/edge identity survived a split/merge; identical face/edge counts are not sufficient to establish stable correspondence. Existing witness-based regeneration and linked-body rebuilds remain unchanged.

## Assembly engineering: potential workspace overlap analysis

`buildLiveArticulatedEnvelopeCsv` still independently solves each sampled articulated pose and measures OCCT-generated moving-body B-rep bounding boxes. It now computes, for each moving body, the number of other **moving bodies** whose cumulative axis-aligned sampled workspace boxes touch/overlap, the closest body ID, and the minimum AABB gap. This supports planning collision investigations but does **not** establish an actual same-time collision or continuous swept-volume clearance. The original motion sample and nominal-fit policy remain available. Output is committed only after successful computation, with no scene mutations.

## Sheet metal: stock sheet planning in associative multi-part manufacturing jobs

Optionally specify stock width/height, spacing, and 90-degree rotation allowance in the existing batch manufacturing GUI. The exporter reads the folded sources' current neutral developments, uses deterministic rectangular shelf packing (2–24 blanks), and writes:

- `stock-layout.csv`: sheet index, stable blank ID, X/Y placement, oriented blank envelope and rotation.
- `stock-layout.svg`: independently readable multi-sheet rectangle placement visualization.
- `batch-manifest.json`: stock options, total rectangular blank area, estimated stock utilization, number of sheets and SHA-256 hashes for the added files.

Original per-part DXF/SVG/bend schedules and provenance manifests are retained. Stock planning happens **before** staging any files, and the batch still publishes all-or-nothing through a temporary sibling directory. The rectangular envelope scheme is **not** contour nesting, grain-direction assurance, or toolpath generation. If a blank cannot fit an empty sheet, the entire export rejects without publishing any output.

## Verification performed in this environment

- Python/source/independent-kernel checks: 497 passing (before final portable stress-test extension); rerun `python -m unittest discover -s tests -p '*.py' -q` for current count.
- Portable C++ suite: 17 targets under GCC (no sanitizers), 17 under Clang (ASan/UBSan). New stock layout target included.
- Stock layout: 350 deterministic multi-blank stress arrangements; rectangle containment, spacing and non-overlap verified under Clang sanitizers.
- No Windows MSVC/Qt/OpenCascade native build, CADWorkbench GUI runtime or complete CTest suite executed here.

Run `RUN_V170_NEW_WORKFLOWS.ps1 -VcpkgRoot C:\vcpkg` in Windows Developer PowerShell to build and exercise focused existing workflow suites. The full `RUN_V170_RELEASE_GATE.ps1` must eventually pass all native CTests and record real end-to-end acceptance results. Do not increase formal acceptance percentages on source checks alone.

## Next engineering tasks

Generalize persistent face/edge lineage through arbitrary topology splits and merges, implement stronger mechanism loop and exact-motion audits, and advance manufacturing-quality sheet-metal annotations and nonrectangular nesting. Resume the two deferred native failures before final release, not as part of this feature-focused checkpoint.

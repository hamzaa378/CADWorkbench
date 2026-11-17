# CADWorkbench v1.70 | Three engineering improvements | 2026-11-06

This is an incremental **source candidate**, not a Windows-verified executable.
It is based on `CADWorkbench_v170_GeneratedFace_SplitMerge_Associative_Drawing_Source_20261106.zip`.

## 1. Bounded recovery of a generated cylindrical face

A feature with an explicit **direct intermediate-stage** curved-face reference can
now recover a cylindrical face whose radius changes by no more than **20%**.
Recovery is only permitted when the cylinder stays coaxial (axis and support
centerline agree), the old radial design point projects to the interior of
exactly one trimmed successor, and its anchor has the complete original
14-field curved-face descriptor. The output receives a fresh current-face
anchor. It explicitly rejects translated/misaligned centerlines, larger
changes, removed regions, and multiple candidate faces. This is **not**
general topological naming for arbitrary fillet-generated B-spline faces or
non-coaxial fillet-radius changes.

## 2. Wider but bounded Boolean dependency recovery

`lineage::allTrackedPaths()` now supports up to 32 distinct ancestry paths
across at most 40 stages (up from 8 paths). It deduplicates identical parent
edges, rejects cyclic or overflowing graphs, and does not write into the caller's
output on error. The actual scene rebuild still independently retargets the
face/edge witness **along every supported route** and requires all routes to
agree on final identity and placement; no arbitrary best-match branch selection.
`uniqueBooleanPath()` remains available and unchanged for single-route callers.

## 3. Persistent model-linked drawing edge dimensions

New selected-edge drawing annotations capture a versioned point/tangent/curvature
witness from OpenCascade, using the unique TopExp edge index. The UI translates
its explorer-selection index to this unique index before capture. The persisted
v24 scene format uses an **optional** `edge_anchor` dictionary of 16 validated
numeric fields, so older scene documents remain readable without a breaking
schema revision. On SVG regeneration, the renderer resolves each witnessed
edge on the current source body, remeasures it, and writes the **current**
source geometry token and edge index to the exported SVG. Missing, boundary,
ambiguous, or malformed witness matches fail the export rather than silently
measuring a different edge. Legacy index-only annotations remain restricted to
their original geometry revision. Only line, circular, BSpline and Bezier
edges with witnessed interior geometry are offered for new saved dimensions.

**Important limitation:** An existing SVG/PDF/DXF file does not update in
place when the model changes. Use **Regenerate saved drawing SVG** to issue a
new snapshot. Overall X/Y/Z dimensions remain recomputed from the current
projected envelope; this increment specifically improves selected-edge length
dimensions. Changing an edge's type or trimming away its witness requires
explicit reselection.

## QA and release gate

- 268 Python tests passed (source contracts and independent OCCT references).
- GCC / Clang with ASan and UBSan: ancestry **41**, feature graph **347**,
  linked-body DAG **80** checks each, totaling **468 per compiler**.
- CMake on Linux reaches the expected Windows-only guard. No native Windows
  Qt/MSVC/OpenCascade compilation has been performed for the new integration.
- **130** existing CTest registrations retained. Additional native scenarios
  were added to registered tests, but are **not** counted as run.

Windows check from the extracted project root:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
.\scripts\verify_release_windows.ps1 -ExpectedTests 130
```

Native acceptance should include save/reopen of a witnessed edge dimension,
regeneration after a retained edge moves, rejection when its witness is deleted,
coaxial-cylinder bounded edits and ambiguous matches, and a fan-in history
whose routes agree. Verify the interactive Qt editing and drawing export
before replacing the last passing release.

# CADWorkbench v1.70 — Three-workstream continued implementation (PRELIMINARY, 2026-11-08)

This candidate continues the **CADWorkbench_v170_ThreeWorkstreams_BatchHistory_StrictMotion_LinkedFlat_PRELIMINARY_20261108.zip** baseline. It is not a validated Windows binary or a final v1.70 release.

## What changed

### 1. Parametric modeling

The mixed associative-history **Batch edit driving parameters** path now validates every staged intermediate B-rep rather than only the final one. The dialog shows a per-stage geometry/volume preview and requires a second confirmation before proposed values are staged. Existing witness/reference metadata guards and the final single-transaction Save behavior are retained. Cancel or invalid intermediate geometry leaves original staged parameters and the saved model unchanged. This does not implement universal persistent topological naming.

### 2. Assembly engineering

The **Edit articulated drives together** dialog has optional 1–4x sub-interval sampling. The underlying read-only trajectory API has a backwards-compatible default (`refinementsPerInterval=1`); users can request up to 48 sampled intervals (12 base intervals x 4) for a denser path check. Each evaluated pose still runs the full kinematic/solid preflight and optional strict-fit rejection. Invalid sample multipliers return failure without overwriting the caller's report. **Discrete sampling cannot certify collision-free behavior between poses**, regardless of resolution.

### 3. Drawings and sheet metal

Saved associative-flat SVG/DXF export now uses a shared geometry-equivalence validator that compares not only flat length and bend stations, but also developed width, every bend allowance and bend deduction. Invalid, nonfinite, or mismatched measurements block export before a file is selected; source and existing files remain unchanged. The check verifies agreement between two representations derived from the same development formulas; it does not independently certify real-world bend allowance or relief manufacturability.

## Verification evidence generated in this environment

- Python/source + independent OpenCascade reference checks: **467/467 passed**.
- Portable GCC C++ targets: **15/15 passed** (non-sanitized).
- Portable Clang C++ targets: **15/15 passed** (ASan/UBSan).
- Existing registered Windows native CTests: **130**, not run here.
- New native Qt/OpenCascade regression cases added for refined articulated sampling and complete developed-shape dimensional agreement, **not compiled/executed here**.
- Source-integrity SHA-256 manifest and ZIP CRC are verified during packaging.
- Formal 60-scenario acceptance evidence **not scored**; prior critical face-machining test also still needs Windows retesting.

## Provisional implementation estimates (NOT acceptance scores)

| Workstream | Previous | Current (provisional) | Final target |
|---|---:|---:|---:|
| Parametric modeling | 74% | ~75% | 85% |
| Assembly engineering | 81% | ~82% | 85% |
| Drawings & sheet metal | 66% | ~67% | 65% |

Even though drawings/sheet-metal is estimated beyond its minimum target, continue its development as requested. These figures do not satisfy the release gate without end-to-end evidence.

## Windows native test/release gate

Extract into a new folder. Run from PowerShell in the extracted source root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

The complete 130-test suite including `cadcore_v170_interleaved_history` and the 60-scenario acceptance gate must pass before final release; preserve your last passing Windows baseline until then.

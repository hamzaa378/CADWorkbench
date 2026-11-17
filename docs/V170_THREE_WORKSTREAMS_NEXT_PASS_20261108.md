# CADWorkbench v1.70 — three-workstream development continuation (PRELIMINARY)

This source is a continuation of CADWorkbench_v170_CompletionPush_ArticulatedRebind_AcceptanceGate_PRELIMINARY_20261108.zip.
It is **not** an MSVC-tested release and does not satisfy the 85%/85%/65% acceptance gate.
Preserve the previously working baseline and extract this ZIP to a separate directory.

## Implementation in this pass

**Parametric modeling:** The Mixed Associative History editor now offers `Batch edit driving parameters`. It displays editable persisted scalar inputs across multiple shape-history steps, but locks witness/anchor, face/edge and reference metadata, as well as feature IDs and graph dependencies. Edited stages run through the existing detached OCCT interleaved-history rebuild and final B-rep validity check. Only after successful validation are the edited values staged; pressing Save commits a single history rebuild. Numeric-input errors and invalid B-reps leave existing staged and saved geometry untouched. The existing advanced single-step parameter editor now shares stronger topology-key protection.

**Assembly engineering:** The Edit Multiple Articulated Drives editor adds a strict fit policy (enabled by default). The existing per-pose fit report now optionally returns a structured unsafe flag for overlap, below-required clearance, or uninspected >12-body mechanisms. When strict policy is enabled the final pose and **each sampled pose** must pass before the user can commit; cancellation or failed sampled preflight does not change the model. The option can be disabled for intentional design interference. This is still discrete static-pose assessment and does NOT prove continuous collision freedom. Legacy preview APIs remain compatible through default parameters.

**Drawings / sheet metal:** Select an associative flat-blank body and use File > Export saved associative flat blank (SVG/DXF). It takes the persisted parent folded definition, per-bend K-factors and relief, validates matching bend stations between the existing SVG and DXF generators, shows an SVG preview, and uses QSaveFile for atomic write of the selected format. It never asks for replacement geometry inputs and does not mutate the model. This is neutral-axis development, NOT manufacturing-certified unfolding.

## Verification actually performed in this container

- Python/source/independent OpenCascade-reference suite: 464/464 tests PASS (including 3 new source integration cases).
- GCC portable 15/15 C++ targets PASS (without sanitizers).
- Clang portable 15/15 C++ targets PASS (ASan + UBSan enabled).
- GCC portable 14/15 C++ targets PASS in a single sanitized run before the execution limit; 15th articulated target rerun separately under GCC ASan + UBSan: 27 checks PASS.
- Added C++ native regression cases for the new strict-sampled fit policy, including atomic failure, untouched report output, and high-clearance rejection. They are NOT compiled/run here.
- Windows MSVC+Qt+OpenCascade compilation and all 130 registered CTests: **NOT RUN**. In particular, `cadcore_v170_interleaved_history` remains to be confirmed after the prior fixture correction.
- End-to-end 60-scenario acceptance gate: **NOT SCORED**. No passing source-contract test is counted as an end-to-end acceptance scenario.

## Implementation estimates — NOT acceptance scores

| Workstream | Previous provisional estimate | New provisional estimate | Final release requirement |
|---|---:|---:|---:|
| Parametric modeling | ~72% | ~74% | 85% |
| Assembly engineering | ~80% | ~81% | 85% |
| Drawings and sheet metal | ~64% | ~66% | 65% |

The new figures are planning estimates of implemented source functionality only, not demonstrated acceptance completion. Even if the drawings/sheet target is reached in an estimate, development and verification continue for that workstream.

## Required Windows command

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V170_RELEASE_GATE.ps1 -VcpkgRoot C:\vcpkg
```

This rebuilds and runs the native CTests, then evaluates the recorded acceptance evidence. Review `build/cadworkbench_release_ctest.xml`, `build/v170_acceptance_gate.json`, and the acceptance CSV. A failed build or absent acceptance evidence is a blocking result. Do not promote to final release without the 85/85/65 rule, critical scenarios and all native tests passing.

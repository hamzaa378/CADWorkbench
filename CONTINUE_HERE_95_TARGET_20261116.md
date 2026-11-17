# Continuation: CADWorkbench v1.70 toward 95% in P/A/D

Baseline: last **Windows-tested** PersistentGeometry WinCTest47 fix (130/130 native, 725/725 source/reference). Current **PRELIMINARY** development source builds on RealWorldEdgeFinishing (new Windows build pending). Never overwrite that passing baseline.

Current increment: coupon-calibrated per-bend K-factor input integrated with existing associative flat-body editing; pure C++20 core; portable tests; target 95% evidence tool and detailed prioritized engineering program.

Read `REAL_LIFE_95_TARGET_PROGRAM_20261116.md` first. It includes blockers for P/A/D and conditional engineering estimates. Run `python tools/verify_95_acceptance.py --evidence-root .` from project root; exit code 3 is expected with 60 NOT_RUN scenarios. Native Windows CTests unchanged at 130 registrations, not yet run for this increment.

Next engineering step: pursue persistent reference rebind through Boolean splits/fillets, geometry-linked mate reattachment, and automatic drawing dimension rebind, using native editable/saved example projects. Keep the project format and undo/rollback invariants.

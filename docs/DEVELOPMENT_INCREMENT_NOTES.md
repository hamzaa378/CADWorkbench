# Post-v1.55 source increment (2026-11-04)

The original ZIP was the **uncommitted v1.55.0 working-tree snapshot** and had 72/72 Windows Release CTest cases passing as of 2026-11-03. These modified files are a separate development increment. Its additions have *not* yet passed a Windows Release build.

## Added

- `src/core/DocumentAudit.h` and `.cpp`: read-only multi-area document preflight API; enumerated severities, stable codes, suggested actions, plain-text/CSV report generation, no document mutation.
- `src/ui/MainWindow.cpp/.h`: project preflight dialogue under **Inspect** and **Inspect ribbon**, with filtering and export; menu **View > Find command…** and ribbon search with `Ctrl+K`.
- `tests/document_audit_tests.cpp`: native C++ audit regression executable registered in CMake as `cadcore_document_audit`.
- `docs/ROADMAP_TO_PROFESSIONAL_CAD.md`: architecture and acceptance gates across modeling, assembly, drawing, fabrication, usability, and later optional domains.
- `scripts/package_source_snapshot.ps1`: exclude an old `SOURCE_MANIFEST.sha256` from the copied source-file set before creating the new manifest entry, preventing duplicate ZIP manifest entries.

## Retained

No existing geometry/scene serialization implementation, tool action, C++ test, or historical project reference was removed. `CMakeLists.txt` remains at project version 1.55.0 until the next native build and release acceptance.

## Verification limitations

The current work environment is Linux and the project explicitly requires Windows, MSVC, Qt6 and OpenCASCADE 8 through vcpkg. Running `cmake -S . -B ...` stops at the intended Windows-only platform guard. This does *not* test C++ compilation. A source wiring/manifest smoke test is run separately and is not a replacement for Windows native CTest or interactive Qt UI testing.

## Developer continuation

1. Run the Windows build/test script and address any compile or runtime regressions.
2. Manually exercise the command search and project-audit GUI in a sample assembly. Check that issuing the audit does not change the undo stack or drawing source tokens.
3. Proceed with roadmap gates G1/G2 (interactive sketch canvas and generic feature DAG) before extending complicated high-level workflows.

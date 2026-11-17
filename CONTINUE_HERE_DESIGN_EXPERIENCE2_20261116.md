# CONTINUE HERE — Design Experience 2 / Backend

Read `DESIGN_EXPERIENCE2_BACKEND_INCREMENT_20261116.md` first.

Base archive: `CADWorkbench_v170_DesignExperience_MAJOR_PRELIMINARY_20261116_GitHub_Source.zip`.
Native last passing baseline: CTest47 corrected PersistentGeometry (130/130 tests, 725/725 Python).
New source regression target: 761 Python tests, 21 portable C++ executables; 130 native test registrations retained.
Windows native build and 60 manual cases pending.

Core: `src/core/SceneDocument.{h,cpp}` (atomic batch rename); `src/core/DesignBrowserQuery.h` (portable browser query).
UI: `src/ui/MainWindow.{h,cpp}` (batch rename preview, scoped browser filter, nested assembly navigator, clickable project audit).
Tests: `tests/cadcore_tests.cpp`, `tests/design_browser_query_portable_tests.cpp`, `tests/v170_model_productivity_backend_contract.py`.

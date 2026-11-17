# Continue here — CADWorkbench Design Experience

1. This preliminary source is based on the **95Target ShopCalibration** snapshot;
   it is not the last native Windows-verified build.
2. Major change: selection contextual workflow, original tool glyphs, design
   timeline, browser context actions, atomic multi-part visibility, recent files,
   navigation strip and saved workspace layout.
3. Read `DESIGN_EXPERIENCE_MAJOR_INCREMENT_20261116.md` before building.
4. Native CTest inventory stays at 130. Current local result: 754 Python/source
   and 20 portable C++ suites pass. Windows test is still required.
5. The native tests for visibility were added to existing `cadcore_tests.cpp`.
6. Keep last 130/130 native-passing CTest47 hotfix baseline untouched.
7. 95% per workstream = at least 19/20 **passed manual scenarios**, including
   critical checks. All manual scenarios are still NOT_RUN.
8. Do not publish a production binary or claim GUI acceptance until Windows
   MSVC + Qt/OpenCascade build and manual scenario checks are completed.

# v1.70 continued engineering — evidence

- New coordinated optional-feature history, synchronized assembly AABB review,
  free-rectangle stock packing.
- Python/source/reference checks: 500/500 passing.
- GCC portable C++ checks: 18/18 targets passing, non-sanitized.
- Clang portable C++ checks: 18/18 targets passing, ASan/UBSan.
- Native Windows Qt6/OpenCascade/MSVC build, focused CTest and full CTest:
  **not run here**, require user Windows run.
- No edits to the two previously failing CTests. No native release approval.

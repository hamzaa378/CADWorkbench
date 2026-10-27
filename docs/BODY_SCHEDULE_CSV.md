# Body schedule CSV export

Use **File → Export body schedule CSV...** or the **File** ribbon's **Export** group to save one row per solid body. The schedule includes the body ID and name, visibility, volume, surface area, axis-aligned X/Y/Z extents, and center of mass. Numeric fields use a period decimal separator; names and IDs are CSV-escaped. The export is written transactionally so a failed write does not leave a partial file.

## Verification

- Windows x64 Release `CADWorkbench.exe` and `cadcore_tests.exe` rebuilt successfully.
- Full CTest passed 38/38, including `cadcore_body_schedule_csv`; its focused test passed 14 assertions for header and metrics, CSV quoting, hidden-body state, row count, empty document/path, and invalid output destination.
- Source audit passed 141 checks; regression references cover all 113 public core operations.
- Live Windows UI confirmed **File → Export body schedule CSV...** is present and opened its CSV save dialog on a test box. The dialog's save workflow and resulting CSV were not exercised through the UI; file contents and error cases are covered by the focused core regression.

This is a body schedule, not a full bill of materials: it does not group instances into quantities, store part numbers or material assignments, or generate drawing sheets.

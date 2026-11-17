# v1.67 verification record

**Starting baseline:** User-confirmed v1.66 Windows build 90/90 passing.

## Executed here

- Linux GCC C++20 portable polygon validator: 88 checks passed with `-Wall -Wextra -Wpedantic -Werror`.
- Linux Clang C++20 portable polygon validator: 88 checks passed with the same strict diagnostics.
- Clang AddressSanitizer/UndefinedBehaviorSanitizer: 88 checks passed.
- Python source preflight and existing static contracts: 18 separate suites, 86 unittest assertions passed. The legacy `kernel_contract_smoke.py` passes additional reference checks that are not included in the count of 86.
- Python/OpenCascade independent reference on box fixture: polygon prism union, polygon pocket subtraction, polygon revolve, and two-polygon loft each produced valid nonempty B-reps and changed volume in the expected direction.
- CMake parsing reached the deliberate non-Windows platform guard without syntax errors.
- Distribution ZIP checked with CRC and per-file SHA-256 checksums. Incremental patch checked for exact reconstruction against the v1.66 ZIP.

## Not executed here

- MSVC Release compilation, Qt UI integration and actual OpenCascade C++ execution.
- The two new C++ tests and all 90 prior native tests (total 92 expected).
- Windows interactive GUI review.

Never interpret the portable/OCP checks as native Windows acceptance.

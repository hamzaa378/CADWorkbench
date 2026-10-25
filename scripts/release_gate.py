"""Conservative release gate. Lack of external Windows results means BLOCKED.

This is not a tamper-proof attestation. It simply prevents accidental declaration
of release readiness when required QA evidence has not been entered.
"""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
verification = root / 'docs' / 'windows_verification.json'
if not verification.exists():
    print('BLOCKED: Windows build, native CTest, and manual QA evidence not supplied.')
    print('Use docs/windows_verification.example.json as a starting point after actual Windows tests.')
    sys.exit(2)
try:
    info = json.loads(verification.read_text(encoding='utf-8'))
except (ValueError, OSError):
    print('BLOCKED: Cannot read Windows verification evidence.')
    sys.exit(2)
requirements = {
    'release_build': 'PASS',
    'ctest': 'PASS',
    'manual_QA_W01_to_W24': 'PASS',
    'clean_VM_deployment': 'PASS',
}
failures = [key for key,value in requirements.items() if info.get(key) != value]
failures += [key for key in ['approver','build_log_path','qa_log_path'] if not info.get(key)]
if failures:
    print('BLOCKED: Missing/failed checks:', ', '.join(failures))
    sys.exit(2)
print('EVIDENCE PRESENT: Confirm evidence is authentic and review before publishing.')

"""Guard independent Windows Python test dependencies and source-decoding setup."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class WindowsPythonDependencies(unittest.TestCase):
    def test_complete_reference_requirements(self):
        requirements = (ROOT / "requirements-kernel-reference.txt").read_text(encoding="utf-8")
        self.assertIn("cadquery>=2.6,<3", requirements)
        self.assertIn("pygments>=2.17,<3", requirements)

    def test_audit_probes_all_required_modules_and_utf8(self):
        script = (ROOT / "RUN_V170_COMPREHENSIVE_AUDIT.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("import OCP; import cadquery; import pygments", script)
        self.assertIn("$env:PYTHONUTF8='1'", script)
        self.assertIn("-r (Join-Path $root 'requirements-kernel-reference.txt')", script)

if __name__ == "__main__":
    unittest.main()

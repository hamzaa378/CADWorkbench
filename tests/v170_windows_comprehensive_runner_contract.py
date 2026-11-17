"""Source contracts for Windows 130-CTest and Python reference audit."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

class WindowsComprehensiveAuditContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runner=(ROOT/"RUN_V170_COMPREHENSIVE_AUDIT.ps1").read_text(encoding="utf-8-sig")
        cls.metal=(ROOT/"src/core/SceneDocument.cpp").read_text(encoding="utf-8")

    def test_native_warnings_do_not_abort_build_logging(self):
        self.assertIn("Start-Transcript", self.runner)
        self.assertIn("Stop-Transcript", self.runner)
        self.assertNotIn("-SkipTests 2>&1", self.runner)
        self.assertIn("$buildExit=$LASTEXITCODE", self.runner)

    def test_130_tests_and_unmodified_acceptance_are_retained(self):
        self.assertIn("--output-junit $report", self.runner)
        self.assertIn("--parallel $Parallel", self.runner)
        self.assertIn("v170_full_test_audit.py", self.runner)
        self.assertIn("V170_ACCEPTANCE_RESULTS.csv", self.runner)

    def test_windows_python_environment_is_isolated_and_explicit(self):
        self.assertIn("$SetupPythonEnv", self.runner)
        self.assertIn(".venv-cadquery/Scripts/python.exe", self.runner)
        self.assertIn("$env:PYTHONUTF8='1'", self.runner)
        self.assertIn("import OCP; import cadquery", self.runner)

    def test_unsafe_iterator_distance_removed(self):
        self.assertNotIn("std::distance(parts_.begin(),found)", self.metal)

if __name__ == "__main__":
    unittest.main()

from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
def load(path): return (ROOT/path).read_text(encoding="utf-8")

class ToleranceClearancePreflight(unittest.TestCase):
    def test_policy_and_compatibility(self):
        h=load('src/core/AssemblyFitPolicy.h')
        a=load('src/core/AssemblyFitAudit.cpp')
        self.assertIn('assessTolerance(',h)
        self.assertIn('std::isfinite(rawLowerBound)',h)
        self.assertIn('ToleranceStatus::NoGuaranteedSeparation',h)
        self.assertIn('return inspectAssemblyFitWithTolerances(document,bodyIds',a)
        self.assertIn('*result=std::move(staged);',a)
        self.assertIn('document.measureMinimumDistance(',a)
        self.assertIn('document.measureInterference(',a)
    def test_ui_and_report(self):
        ui=load('src/ui/MainWindow.cpp')
        a=load('src/core/AssemblyFitAudit.cpp')
        self.assertIn('QTableWidget(static_cast<int>(ids.size()),2,&settings)',ui)
        self.assertIn('maximum outward geometric deviation',load('docs/V170_TOLERANCE_AWARE_ASSEMBLY_CLEARANCE.md').lower())
        self.assertIn('inspectAssemblyFitWithTolerances(document_',ui)
        self.assertIn('Conservative minimum gap (mm)',a)
        self.assertIn('signedClearanceMarginMm',a)
        self.assertIn('escaped.prepend(QChar(0x27))',a)
        self.assertIn('QSaveFile destination(filename)',ui)
    def test_test_preservation(self):
        cm=load('CMakeLists.txt')
        names=re.findall(r'add_test\s*\(\s*NAME\s+([a-zA-Z0-9_]+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        self.assertIn('ExpectedTests = 130',load('scripts/verify_release_windows.ps1'))
        self.assertIn('2000;++i',load('tests/assembly_fit_policy_v170_tests.cpp'))
        self.assertIn('inspectAssemblyFitWithTolerances(scene',load('tests/assembly_fit_audit_v170_tests.cpp'))
        self.assertTrue((ROOT/'src/core/ArticulatedAssemblyNetwork.cpp').is_file())
        self.assertTrue((ROOT/'tests/interleaved_history_v170_tests.cpp').is_file())

if __name__=='__main__': unittest.main()

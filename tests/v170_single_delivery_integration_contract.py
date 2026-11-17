"""Static integration contracts only. Native Windows CTest is still required."""
import pathlib
import re
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]

class V170SingleDeliveryContracts(unittest.TestCase):
    def test_adaptive_assembly_integration(self):
        header=(ROOT/'src/core/SceneDocument.h').read_text(encoding="utf-8")
        code=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        for literal in ('scanAssemblyMotionAdaptive(', 'bool* unresolvedIntervals',
                        'QVector<MotionInterferenceSample>* samples'):
            self.assertIn(literal,header)
        self.assertIn('cad::motion::scanAdaptive(initial,settings,measure,&result,&reason)',code)
        self.assertIn('*samples=std::move(candidate)',code)
        self.assertIn('*unresolvedIntervals=result.budgetExhausted',code)
        self.assertIn('document_.scanAssemblyMotionAdaptive(',ui)
        self.assertIn('not certified continuous collision clearance',ui)
    def test_configuration_bom_is_rebuilt_and_atomic(self):
        header=(ROOT/'src/core/SceneDocument.h').read_text(encoding="utf-8")
        code=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        for literal in ('compareDesignConfigurations(', 'buildConfigurationComparisonCsv(',
                        'exportConfigurationComparisonCsv(', 'ConfigurationBomComparison'):
            self.assertIn(literal,header)
        for literal in ('trial.applyDesignConfiguration(partId,variant.name,&issue)',
                        'trial.billOfMaterials(&detail)', 'trial.diagnoseBody(partId,&body,&detail)',
                        'QSaveFile file(path);', 'drawingSourceDataFingerprint(assembly)'):
            self.assertIn(literal,code)
        self.assertIn('*output=std::move(prepared)',code)
    def test_ui_and_native_regression_evidence(self):
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        tests=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding="utf-8")
        self.assertIn('Compare rebuilt BOM and mass...',ui)
        self.assertIn('Read-only comparison of',ui)
        self.assertIn('doc.scanAssemblyMotionAdaptive(',tests)
        self.assertIn('document.compareDesignConfigurations(',tests)
        self.assertIn('document.exportConfigurationComparisonCsv(',tests)
    def test_release_gate_retained_and_portable_engine_registered(self):
        cm=(ROOT/'CMakeLists.txt').read_text(encoding="utf-8")
        script=(ROOT/'scripts/verify_release_windows.ps1').read_text(encoding="utf-8")
        portable=(ROOT/'scripts/verify_portable_consolidation.sh').read_text(encoding="utf-8")
        self.assertEqual(len(re.findall(r'add_test\(NAME ',cm)),130)
        self.assertIn('ExpectedTests = 130',script)
        self.assertIn('src/core/AdaptiveMotionScan.cpp',cm)
        self.assertIn('compile_run adaptive_motion',portable)

if __name__=='__main__':
    unittest.main()

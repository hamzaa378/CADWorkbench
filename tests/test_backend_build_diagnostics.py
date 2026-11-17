import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from diagnose_build import diagnose

class BuildDiagnosticTests(unittest.TestCase):
    def test_real_compiler_failure(self):
        output=diagnose('MainWindow.cpp(1,1): error C1128: number of sections exceeded object file format limit: compile with /bigobj\nNative tests NOT RUN: build did not succeed\nRan 781 tests in 25.0s\nOK\n')
        self.assertEqual(output['status'],'INCOMPLETE_OR_FAILED')
        self.assertIn('MSVC_C1128',[i['code'] for i in output['issues']])
        self.assertIn('NATIVE_NOT_RUN',[i['code'] for i in output['issues']])
    def test_redeclaration_and_linker(self):
        o=diagnose('x.cpp(5,4): error C2374: duplicate\nLINK : fatal error LNK1104: cannot open file\n')
        codes=[i['code'] for i in o['issues']]
        self.assertIn('MSVC_REDECLARATION',codes)
        self.assertIn('LINK_FAILURE',codes)
    def test_warnings_not_errors(self):
        o=diagnose('warning C4996: deprecated\nwarning C4456: shadow\n')
        self.assertNotIn('MSVC_OTHER',[i['code'] for i in o['issues']])
    def test_synthetic_fixture_does_not_pass(self):
        o=diagnose('Native 130/130 passed; failed=0; missing=0; gate=False\nRan 781 tests in 25.0s\nOK')
        self.assertFalse(o['native_ctest_complete'])
        self.assertEqual(o['status'],'INCOMPLETE_OR_FAILED')
    def test_explicit_complete_audit(self):
        o=diagnose('100% tests passed, 0 tests failed out of 130\nRan 781 tests in 22.1s\nOK\nAutomated verification PASSED\n')
        self.assertEqual(o['status'],'AUTOMATED_PASS')
        self.assertFalse(o['issues'])
    def test_failed_audit_does_not_override_tests(self):
        o=diagnose('100% tests passed, 0 tests failed out of 130\nRan 781 tests in 22.1s\nOK\nAutomated verification FAILED')
        self.assertEqual(o['status'],'INCOMPLETE_OR_FAILED')
    def test_native_failed_129(self):
        o=diagnose('99% tests passed, 1 tests failed out of 130\nThe following tests FAILED:\n')
        self.assertIn('NATIVE_TEST_FAILURE',[i['code'] for i in o['issues']])
    def test_grouped_edge_native_failure_has_actionable_diagnosis(self):
        o=diagnose('44/130 Test #45: cadcore_universal_feature_tree ***Failed\n'
                   'FAIL at check 70: Witnessed unequal-size two-edge blend did not rebuild')
        self.assertIn('GROUPED_EDGE_BUILD_FAILURE',[i['code'] for i in o['issues']])
        self.assertEqual(o['status'],'INCOMPLETE_OR_FAILED')

    def test_grouped_edge_new_diagnostic_reports_kernel_detail(self):
        o=diagnose('Witnessed unequal-size group failed [Fillet selected edges]: Incomplete edge anchor.')
        self.assertIn('GROUPED_EDGE_BUILD_FAILURE',[i['code'] for i in o['issues']])
        self.assertFalse(o['native_ctest_complete'])

    def test_unrelated_native_failure_not_misclassified_as_edge_group(self):
        o=diagnose('99% tests passed, 1 tests failed out of 130\nTest #47 ***Failed')
        self.assertNotIn('GROUPED_EDGE_BUILD_FAILURE',[i['code'] for i in o['issues']])
        self.assertEqual(o['status'],'INCOMPLETE_OR_FAILED')

    def test_python_failed(self):
        o=diagnose('Ran 781 tests in 22.1s\nFAILED (failures=1)')
        self.assertIn('PYTHON_FAILED',[i['code'] for i in o['issues']])
    def test_python_threshold(self):
        o=diagnose('100% tests passed, 0 tests failed out of 130\nRan 770 tests in 22.1s\nOK\nAutomated verification PASSED')
        self.assertFalse(o['python_inventory_complete'])
    def test_ctest_44_success_format(self):
        log = ('Test project C:/build\n'
               '130/130 Test #105: cadcore_geometry_and_persistence ..... Passed 47.88 sec\n'
               '100% tests passed out of 130\n'
               'Total Test time (real) = 47.92 sec\n'
               'Ran 900 tests in 29.260s\nOK\n'
               'AUTOMATED VERIFICATION PASSED: 130 native CTests and Python/source suite.\n'
               'MANUAL ACCEPTANCE PENDING: release remains BLOCKED.\n'
               'Full audit process exit code: 3\n')
        result = diagnose(log, expected_python=900)
        self.assertEqual(result['status'], 'AUTOMATED_PASS')
        self.assertTrue(result['native_ctest_complete'])
        self.assertTrue(result['python_inventory_complete'])

    def test_ctest_44_wrong_count_is_incomplete(self):
        o=diagnose('100% tests passed out of 129\nRan 900 tests in 29.0s\nOK\nAutomated verification PASSED',expected_python=900)
        self.assertEqual(o['status'], 'INCOMPLETE_OR_FAILED')
        self.assertIn('NATIVE_EVIDENCE_MISSING',[x['code'] for x in o['issues']])

    def test_ctest_44_missing_python_is_incomplete(self):
        o=diagnose('100% tests passed out of 130\nRan 899 tests in 29.0s\nOK\nAutomated verification PASSED',expected_python=900)
        self.assertEqual(o['status'], 'INCOMPLETE_OR_FAILED')
        self.assertIn('PYTHON_EVIDENCE_MISSING',[x['code'] for x in o['issues']])

    def test_ctest_44_without_audit_success_is_incomplete(self):
        o=diagnose('100% tests passed out of 130\nRan 900 tests in 29.0s\nOK',expected_python=900)
        self.assertEqual(o['status'], 'INCOMPLETE_OR_FAILED')

    def test_ctest_44_explicit_failure_takes_priority(self):
        o=diagnose('100% tests passed out of 130\nRan 900 tests in 29.0s\nOK\nAutomated verification FAILED',expected_python=900)
        self.assertEqual(o['status'], 'INCOMPLETE_OR_FAILED')

    def test_ctest_44_python_fixture_count_is_not_native_ctest(self):
        o=diagnose('Native 130/130 passed; failed=0; missing=0; gate=False\nRan 900 tests in 29.0s\nOK\nAutomated verification PASSED',expected_python=900)
        self.assertEqual(o['status'], 'INCOMPLETE_OR_FAILED')

    def test_full_audit_explicit(self):
        o=diagnose('100% tests passed, 0 tests failed out of 130\nRan 800 tests in 30.0s\nOK')
        self.assertEqual(o['status'],'INCOMPLETE_OR_FAILED')

if __name__=='__main__':unittest.main()

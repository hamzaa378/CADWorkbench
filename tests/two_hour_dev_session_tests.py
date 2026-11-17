"""Focused regression tests for the enforced coding-session integrity workflow.

These use temporary fixture projects. They do not impersonate real Windows CTest
execution or certify the current source as a Windows build.
"""
import importlib.util
import json
from pathlib import Path
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('cad_dev_session',ROOT/'tools/dev_session.py')
dev=importlib.util.module_from_spec(spec)
spec.loader.exec_module(dev)


class TwoHourDevSessionTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        (self.root/'src/core').mkdir(parents=True)
        (self.root/'src/ui').mkdir(parents=True)
        (self.root/'src/core/Body.cpp').write_text('int body() { return 1; }\n')
        (self.root/'src/ui/MainWindow.cpp').write_text('int main_ui() { return 1; }\n')
        (self.root/'.gitattributes').write_text('* text=auto\n')
        self.refresh()

    def refresh(self):
        text=dev.manifest_text(dev.scanner(self.root))
        for name in ('SHA256SUMS.txt','SOURCE_MANIFEST_SHA256.txt'):
            (self.root/name).write_text(text)

    def test_base_integrity_and_session_start(self):
        dev.start(self.root,'test session')
        state=dev.session(self.root)
        self.assertEqual(state['label'],'test session')
        self.assertEqual(len(state['baseline']),3)
        with self.assertRaisesRegex(ValueError,'already exists'):
            dev.start(self.root,'duplicate')

    def test_back_end_changes_stage_atomically(self):
        dev.start(self.root,'tests')
        (self.root/'src/core/Body.cpp').write_text('int body() { return 2; }\n')
        (self.root/'src/core/Extra.h').write_text('int extra();\n')
        dev.stage(self.root)
        state=dev.verify(self.root)
        self.assertEqual(state['staged']['delta']['modified'],['src/core/Body.cpp'])
        self.assertEqual(state['staged']['delta']['added'],['src/core/Extra.h'])
        self.assertEqual(dev.load_manifest(self.root),dev.scanner(self.root))

    def test_frontend_freeze_blocks_stage(self):
        dev.start(self.root,'tests')
        (self.root/'src/ui/MainWindow.cpp').write_text('int main_ui() { return 2; }\n')
        with self.assertRaisesRegex(ValueError,'Frontend freeze'):
            dev.stage(self.root)
        self.assertEqual(dev.load_manifest(self.root)['src/ui/MainWindow.cpp'],dev.session(self.root)['baseline']['src/ui/MainWindow.cpp'])

    def test_staged_changes_invalidate_test_snapshot(self):
        dev.start(self.root,'tests')
        dev.stage(self.root)
        (self.root/'src/core/Body.cpp').write_text('int body() { return 99; }\n')
        with self.assertRaisesRegex(ValueError,'Source manifest mismatch'):
            dev.verify(self.root)
        dev.stage(self.root)
        dev.verify(self.root)

    def test_external_manifest_edits_block_stage(self):
        dev.start(self.root,'tests')
        (self.root/'SHA256SUMS.txt').write_text('untrusted\n')
        with self.assertRaisesRegex(ValueError,'externally modified'):
            dev.stage(self.root)

    def test_fail_closed_without_native_evidence(self):
        dev.start(self.root,'tests')
        dev.stage(self.root)
        with self.assertRaisesRegex(ValueError,'Missing or stale'):
            dev.finish(self.root,3)
        self.assertTrue(dev.state_path(self.root).is_file())

    def test_reject_failed_audit_code(self):
        dev.start(self.root,'tests')
        dev.stage(self.root)
        with self.assertRaisesRegex(ValueError,'did not succeed'):
            dev.finish(self.root,1)

    def make_audit(self, native=True, count=771, has_success=True):
        path=self.root/'build/v170_comprehensive_audit'
        path.mkdir(parents=True,exist_ok=True)
        (path/'v170_full_audit.json').write_text(json.dumps({
            'native_complete':native,'problems':[],
            'release_gate_evidence_satisfied':False}))
        (path/'ctest_output.log').write_text('100% tests passed, 0 tests failed out of 130\n' if has_success else '99% tests passed, 1 tests failed out of 130\n')
        (path/'python_source_tests.log').write_text(f'Ran {count} tests in 9.0s\n\nOK\n')

    def test_finish_needs_all_130_and_full_python_inventory(self):
        dev.start(self.root,'tests')
        dev.stage(self.root)
        self.make_audit(count=770)
        with self.assertRaisesRegex(ValueError,'minimum 771'):
            dev.finish(self.root,3)
        self.make_audit(native=False)
        with self.assertRaisesRegex(ValueError,'130 native'):
            dev.finish(self.root,3)
        self.make_audit(has_success=False)
        with self.assertRaisesRegex(ValueError,'130/130 CTest'):
            dev.finish(self.root,3)

    def test_passed_fixture_records_automated_only(self):
        dev.start(self.root,'tests')
        (self.root/'src/core/Body.cpp').write_text('int body() { return 2; }\n')
        dev.stage(self.root)
        self.make_audit()
        dev.finish(self.root,3)
        report=(self.root/'build/dev_sessions/SESSION_REPORT.md').read_text()
        self.assertIn('manual acceptance: PENDING / NOT CLAIMED', report)
        self.assertFalse(dev.state_path(self.root).exists())
        with self.assertRaisesRegex(ValueError,'No active session'):
            dev.verify(self.root)

    def test_preflight_detects_contextgrid_redeclaration(self):
        spec2=importlib.util.spec_from_file_location('cad_preflight',ROOT/'tools/preflight_major_build.py')
        pre=importlib.util.module_from_spec(spec2)
        spec2.loader.exec_module(pre)
        project=ROOT
        source=(project/'src/ui/MainWindow.cpp').read_text()
        self.assertEqual(source.count('auto* contextGrid='),1)
        self.assertNotIn('auto* contextGrid=qobject_cast',source)
        self.assertFalse(any('contextGrid' in e for e in pre.check(project)),
                         'Shipped source should never contain the native build redeclaration')

if __name__=='__main__':unittest.main()

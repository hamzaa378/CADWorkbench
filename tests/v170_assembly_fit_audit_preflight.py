from pathlib import Path
import re
import unittest
R=Path(__file__).resolve().parents[1]
class AuditPreflight(unittest.TestCase):
    def test_types_and_scan(self):
        h=(R/'src/core/AssemblyFitAudit.h').read_text(encoding="utf-8")
        c=(R/'src/core/AssemblyFitAudit.cpp').read_text(encoding="utf-8")
        self.assertIn('inspectAssemblyFit(',h)
        self.assertIn('assemblyFitReportCurrent(',h)
        self.assertIn('document.measureMinimumDistance(',c)
        self.assertIn('document.measureInterference(',c)
        self.assertIn('*result=std::move(staged);',c)
        self.assertIn('bodyIds.size()>32',c)
    def test_ui(self):
        m=(R/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        self.assertIn('void MainWindow::showAssemblyFitAudit()',m)
        self.assertIn('Assembly fit audit: all pairs',m)
        self.assertIn('QSaveFile destination(filename)',m)
    def test_cmake(self):
        s=(R/'CMakeLists.txt').read_text(encoding="utf-8")
        self.assertIn('src/core/AssemblyFitAudit.cpp',s)
        self.assertIn('cadcore_v170_assembly_fit_audit',s)
        self.assertIn('cadcore_v170_assembly_fit_report_snapshot',s)
        self.assertIn('cadcore_v170_assembly_fit_policy',s)
        self.assertEqual(len(re.findall(r'add_test\s*\(\s*NAME\s+',s)),130)
    def test_read_only(self):
        s=(R/'src/core/AssemblyFitAudit.cpp').read_text(encoding="utf-8")
        self.assertNotIn('checkpoint()',s)
        self.assertNotIn('parts_=',s)
    def test_no_source_loss(self):
        self.assertTrue((R/'tests/linked_body_batch_v170_tests.cpp').is_file())
        self.assertTrue((R/'src/core/AdvancedSketchProfiles.cpp').is_file())
if __name__=='__main__': unittest.main()

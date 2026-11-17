"""Source integration guards only; native Windows verification is separate."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
class MotionIntervalAuditContract(unittest.TestCase):
    def test_portable_interval_result(self):
        h=(ROOT/'src/core/AdaptiveMotionScan.h').read_text(encoding="utf-8")
        c=(ROOT/'src/core/AdaptiveMotionScan.cpp').read_text(encoding="utf-8")
        self.assertIn('struct IntervalAssessment',h)
        self.assertIn('std::vector<IntervalAssessment> intervals',h)
        self.assertIn('candidate.intervals.push_back',c)
        self.assertIn('provedClearance(it->second,next->second,settings)',c)
        self.assertIn('if(bounded)least=std::min(least,lower)',c)
    def test_scene_atomic_output_and_gui(self):
        c=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        u=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        self.assertIn('if(intervalAudit)*intervalAudit=std::move(intervalCandidate);',c)
        self.assertIn('Export interval audit CSV...',u)
        self.assertIn('unprovenIntervals',u)
        self.assertIn('intervalAudit.size()',u)
        self.assertIn('&conservativeLowerBoundMm,&intervalAudit',u)
    def test_csv_and_native_rollback(self):
        c=(ROOT/'src/core/PartsData.cpp').read_text(encoding="utf-8")
        t=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding="utf-8")
        self.assertIn('exportMotionClearanceIntervalsCsv',c)
        self.assertIn('return writeCsvFile(path,bytes,error);',c)
        self.assertIn('readBytes(auditPath)==auditBytes',t)
        self.assertIn('intervalAudit==auditBeforeBound',t)
    def test_no_extra_registered_native_tests(self):
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding="utf-8")
        self.assertEqual(len(re.findall(r'add_test\(NAME ',cmake)),130)
if __name__=='__main__':unittest.main()

"""Bounded static integration checks for native engineering-workflow additions.

These are NOT substitutes for executing the Windows Qt/OpenCascade build.
"""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]

class EngineeringWorkflowIncrement(unittest.TestCase):
    def test_assembly_diagnostics_reach_native_ui(self):
        header=(ROOT/'src/core/AssemblyNetworkStructureAudit.h').read_text(encoding='utf-8')
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
        self.assertIn('struct OccurrenceImpact',header)
        self.assertIn('criticalOccurrenceImpacts.push_back',header)
        self.assertIn('component[removed]!=component[fixedNode]',header)
        self.assertIn('out.criticalOccurrenceImpacts.front().occurrenceIndex==', (ROOT/'tests/assembly_network_structure_audit_tests.cpp').read_text())
        self.assertIn('audit.criticalOccurrenceImpacts',ui)
        self.assertIn('Save audit report...',ui)
        self.assertIn('QSaveFile file(path);',ui)
    def test_sheet_station_shop_limits_reach_native_ui(self):
        header=(ROOT/'src/core/SheetMetalBendSequenceAudit.h').read_text(encoding='utf-8')
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
        self.assertIn('checkBendStationSpacing',header)
        self.assertIn('tooClosePairs.push_back(i)',header)
        self.assertIn('bendStation->value()',ui)
        self.assertIn('cad::sheet::checkBendStationSpacing(development,bendStation->value()',ui)
    def test_native_ctest_130_unchanged(self):
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cmake)
        self.assertEqual(130,len(names))
        self.assertEqual(len(set(names)),len(names))

if __name__=='__main__': unittest.main()

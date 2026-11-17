"""Source wiring guards for large v1.70 preliminary engineering increment.
These checks do not substitute for native Windows compile or UI testing.
"""
import re
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(path): return (ROOT/path).read_text(encoding='utf-8')
class MajorThreeWorkstreamUpgrade(unittest.TestCase):
    def test_parametric_preview_gui_and_read_only_core(self):
        ui=read('src/ui/MainWindow.cpp')
        core=read('src/core/SceneDocument.cpp')
        for part in ('Preflight staged history (read-only)',
                     'previewInterleavedFeatureHistory(target,steps,&snapshots,&issue)',
                     'BRepCheck_Analyzer(shape).IsValid()',
                     'First failing testable prefix ends at step'):
            self.assertIn(part,ui)
        start=core.index('bool SceneDocument::previewInterleavedFeatureHistory(')
        body=core[start:core.index('bool SceneDocument::setInPlaceSketchFeature(',start)]
        self.assertNotIn('commit(',body)
        self.assertNotIn('pushUndo(',body)
        self.assertIn('replayInterleavedHistory(staged,parts_,indices',body)
    def test_assembly_audit_wired(self):
        ui=read('src/ui/MainWindow.cpp')
        self.assertIn('Audit saved rigid mate structure and cycle links',ui)
        self.assertIn('auditLiveRigidMateStructure()',ui)
        self.assertIn('liveRigidMateDefinitions()',ui)
        self.assertIn('cad::mates::auditMateStructure(',ui)
        self.assertIn('graph cycles',ui.lower())
    def test_sheet_bend_sequence_wired_and_crosschecks_kernel(self):
        ui=read('src/ui/MainWindow.cpp')
        for part in ('Audit selected folded body bend sequence',
                     'auditSelectedFoldedSheetBendSequence()',
                     'cad::sheet::inspectBendSequence(',
                     'cad::calculateFoldedSheetFlatPattern(',
                     'Export sequence CSV',
                     'reference.bendLinePositionsMm'):
            self.assertIn(part,ui)
    def test_both_algorithms_portable_and_native_suites_extended(self):
        runner=read('scripts/verify_portable_consolidation.sh')
        for name in ('mate_structure','bend_sequence'):
            self.assertIn('compile_run '+name,runner)
        self.assertIn('auditMateStructure',read('tests/assembly_constraint_network_v170_tests.cpp'))
        self.assertIn('inspectBendSequence',read('tests/cadcore_tests.cpp'))
    def test_native_inventory_and_windows_gate_still_original(self):
        cmake=read('CMakeLists.txt')
        entries=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cmake)
        self.assertEqual(len(entries),130)
        self.assertEqual(len(set(entries)),130)
        self.assertIn('ExpectedTests 130',read('RUN_WINDOWS_TESTS.ps1'))
    def test_ids_wiring_in_header(self):
        header=read('src/ui/MainWindow.h')
        self.assertIn('void auditLiveRigidMateStructure();',header)
        self.assertIn('void auditSelectedFoldedSheetBendSequence();',header)
if __name__=='__main__':unittest.main()

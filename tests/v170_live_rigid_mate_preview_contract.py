"""Source-contract checks; native Qt/OCCT validation remains mandatory."""
import unittest
from pathlib import Path
R=Path(__file__).resolve().parents[1]
class PreviewContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.h=(R/'src/core/SceneDocument.h').read_text(encoding="utf-8")
        cls.core=(R/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        cls.ui=(R/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        cls.test=(R/'tests/assembly_mate_document_v170_tests.cpp').read_text(encoding="utf-8")
    def test_readonly_public_interface(self):
        self.assertIn('previewLiveRigidMateConnectionEdit(int index',self.h)
        self.assertIn('QString* report,QString* error=nullptr,',self.h)
    def test_full_graph_preflight(self):
        p=self.core.split('bool SceneDocument::previewLiveRigidMateConnectionEdit(',1)[1].split('bool SceneDocument::replaceLiveRigidMateConnection(',1)[0]
        self.assertIn('mates::solveRigidNetwork',p)
        self.assertIn('newWorldConnection',self.core)
        self.assertIn('solution.worstClosureResidualMm',p)
        self.assertIn("*report=lines.join('",p)
        self.assertIn('Kinematic preview exceeds supported placement bounds.',p)
    def test_noop_edit_does_not_checkpoint(self):
        p=self.core.split('bool SceneDocument::replaceLiveRigidMateConnection(',1)[1].split('bool SceneDocument::appendLiveRigidMateConnection(',1)[0]
        self.assertLess(p.index('mateObservationsExactlyEqual'),p.index('return setLiveRigidMateNetwork'))
        self.assertIn('if(mateObservationsExactlyEqual(world.at(index),newWorldConnection))return true;',p)
    def test_user_approval_and_precision(self):
        p=self.ui.split('void MainWindow::manageLiveRigidMates(){',1)[1].split('void MainWindow::solveAssemblyFrameMates(){',1)[0]
        self.assertIn('box->setDecimals(12)',p)
        self.assertIn('if(!changedByUser)',p)
        self.assertIn('previewLiveRigidMateConnectionEdit(index,changed,&preview,&issue,',p)
        self.assertIn('Commit this edit?',p)
        self.assertLess(p.index('previewLiveRigidMateConnectionEdit'),p.index('replaceLiveRigidMateConnection'))
    def test_native_preview_cases(self):
        self.assertIn('read-only no-change mate preview failed',self.test)
        self.assertIn('failed mate preview changed caller report or scene',self.test)
        self.assertIn('no-op mate editing changed scene revision or B-rep token',self.test)
if __name__=='__main__':unittest.main()

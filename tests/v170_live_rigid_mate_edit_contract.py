"""Source wiring checks only; not a substitute for MSVC/Qt native tests."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class MateEditorContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.h=(ROOT/'src/core/SceneDocument.h').read_text(encoding="utf-8")
        cls.cpp=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        cls.ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        cls.native=(ROOT/'tests/assembly_mate_document_v170_tests.cpp').read_text(encoding="utf-8")
    def test_public_edit_api(self):
        self.assertIn('replaceLiveRigidMateConnection(int index',self.h)
        self.assertIn('liveRigidMateWorldDefinitions(QVector<AssemblyNetworkMate>* world',self.h)
    def test_atomic_rebuild(self):
        source=self.cpp.split('bool SceneDocument::replaceLiveRigidMateConnection(',1)[1].split('bool SceneDocument::appendLiveRigidMateConnection(',1)[0]
        self.assertIn('newWorldConnection.parentBodyId!=original.parentBodyId',source)
        self.assertIn('newWorldConnection.childBodyId!=original.childBodyId',source)
        self.assertIn('world[index]=newWorldConnection',source)
        self.assertIn('return setLiveRigidMateNetwork(liveRigidMateFixed_,world,error)',source)
        self.assertNotIn('liveRigidMates_[index]=',source)
    def test_gui_editing_and_undo(self):
        f=self.ui.split('void MainWindow::manageLiveRigidMates(){',1)[1].split('void MainWindow::solveAssemblyFrameMates(){',1)[0]
        self.assertIn('Edit constraint frames...',f)
        self.assertIn('replaceLiveRigidMateConnection(index,changed,&issue)',f)
        self.assertIn('removeLiveRigidMateConnection(index,&issue)',f)
        self.assertIn('QScrollArea',f)
    def test_native_acceptance_is_registered(self):
        self.assertIn('undo mate edit failed',self.native)
        self.assertIn('reopen edited rigid mate network',self.native)
        self.assertIn('contradictory loop mate edit accepted',self.native)
if __name__=='__main__': unittest.main()

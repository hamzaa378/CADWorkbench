"""Source contracts for bounded articulated joint rebind; native Qt/OCCT is mandatory."""
import pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class ArticulatedRebindContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header=(ROOT/'src/core/SceneDocument.h').read_text(encoding="utf-8")
        cls.core=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        cls.ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        cls.native=(ROOT/'tests/articulated_assembly_scene_v170_tests.cpp').read_text(encoding="utf-8")
    def test_public_read_only_and_commit(self):
        self.assertIn('bool previewLiveArticulatedJointRebind(',self.header)
        self.assertIn('bool rebindLiveArticulatedJoint(',self.header)
    def test_rebind_closed_graph_only(self):
        self.assertIn('!reference.contains(parent)||!reference.contains(child)',self.core)
        self.assertIn('parent==child',self.core)
    def test_preview_commits_no_changes(self):
        chunk=self.core.split('bool SceneDocument::previewLiveArticulatedJointRebind(')[1].split('bool SceneDocument::rebindLiveArticulatedJoint(')[0]
        self.assertIn('stageLiveArticulatedRegeneration(',chunk)
        self.assertIn('inspectAssemblyFit(',chunk)
        self.assertNotIn('checkpoint()',chunk)
        self.assertNotIn('\n    parts_=std::move(staged);',chunk)
        self.assertIn('detached.parts_=std::move(staged);',chunk)
        self.assertLess(chunk.index('stageLiveArticulatedRegeneration('),chunk.index('*report='))
    def test_commit_one_checkpoint(self):
        chunk=self.core.split('bool SceneDocument::rebindLiveArticulatedJoint(')[1].split('bool SceneDocument::removeLiveArticulatedNetwork(')[0]
        self.assertEqual(chunk.count('checkpoint()'),1)
        self.assertLess(chunk.index('stageLiveArticulatedRegeneration('),chunk.index('checkpoint()'))
        self.assertIn('if(geometryChanged)parts_=',chunk)
    def test_gui_preview_first(self):
        chunk=self.ui.split('void MainWindow::rebindSavedArticulatedJoint()')[1].split('void MainWindow::editSavedArticulatedDrives()')[0]
        self.assertLess(chunk.index('previewLiveArticulatedJointRebind('),chunk.index('rebindLiveArticulatedJoint('))
        self.assertIn('QMessageBox::question(',chunk)
    def test_native_regressions(self):
        for required in ['disconnected rebind committed','rebind undo','rebind redo','rebound graph did not survive save']:
            self.assertIn(required,self.native)
if __name__=='__main__':unittest.main()

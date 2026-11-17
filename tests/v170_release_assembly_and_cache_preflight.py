"""Source-contract tests only; Qt/OCCT integration requires native MSVC CTest."""
import unittest
from pathlib import Path
root=Path(__file__).resolve().parents[1]
class ConsolidatedRc2Preflight(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scene=(root/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        cls.header=(root/'src/core/SceneDocument.h').read_text(encoding="utf-8")
        cls.ui=(root/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        cls.plan=(root/'src/core/FeatureDependencyPlan.cpp').read_text(encoding="utf-8")
    def test_multi_source_revision_api(self):
        self.assertIn('planReuseWithSources(',self.plan)
        self.assertIn('old->second!=now->second',self.plan)
        self.assertIn('input.rfind("@source:",0)==0',self.plan)
    def test_live_mates_store_local_frames(self):
        self.assertIn('setLiveRigidMateNetwork(',self.scene)
        self.assertIn('freeOccurrencePlacement(*parent).Inverted()',self.scene)
        self.assertIn('freeOccurrencePlacement(*child).Inverted()',self.scene)
        self.assertIn('refreshLiveRigidMateNetwork(',self.scene)
        self.assertIn('matePoint(pw,constraint.parentPoint)',self.scene)
    def test_live_mates_undo_redo_and_delete_guard(self):
        self.assertIn('liveRigidMateFixed_=previous.liveRigidMateFixed',self.scene)
        self.assertIn('liveRigidMateFixed_=next.liveRigidMateFixed',self.scene)
        self.assertIn('if(any&&!all)',self.scene)
    def test_live_archive_is_validated_before_scene_commit(self):
        self.assertIn('root.insert("live_rigid_mates",savedMates)',self.scene)
        self.assertIn('loadedMateFixed=archive.value("fixed").toString()',self.scene)
        self.assertIn('mates::solveRigidNetwork(static_cast<std::size_t>(bodyIds.size()),0,',self.scene)
        self.assertLess(self.scene.index('if(root.contains("live_rigid_mates"))'),self.scene.index('parts_ = std::move(candidate);'))
    def test_editor_provides_authoring_refresh_and_removal(self):
        self.assertIn('Store editable local-frame mates instead',self.ui)
        self.assertIn('setLiveRigidMateNetwork(ids[0]',self.ui)
        self.assertIn('Refresh saved live rigid mates',self.ui)
        self.assertIn('Remove saved live rigid mates',self.ui)
    def test_cached_geometry_not_invalidated_on_no_op_refresh(self):
        self.assertIn('std::none_of(solution.worldCorrections.cbegin()+1',self.scene)
        self.assertIn('if(revision_==initialRevision)checkpoint()',self.scene)
if __name__=='__main__':unittest.main()

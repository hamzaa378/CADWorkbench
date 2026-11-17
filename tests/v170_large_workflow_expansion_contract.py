import unittest
from pathlib import Path
base=Path(__file__).resolve().parents[1]
root=base
class ExpandedWorkflowContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.core=(root/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        cls.ui=(root/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        cls.head=(root/'src/core/SceneDocument.h').read_text(encoding="utf-8")
    def test_design_alternatives_are_full_scene_and_read_only(self):
        fn=self.core.split('bool SceneDocument::evaluateParametricDesignAlternatives(',1)[1].split('QStringList SceneDocument::suppressedParametricFeatures',1)[0]
        self.assertIn('SceneDocument candidate=*this',fn)
        self.assertIn('candidate.setNamedParameterBatch',fn)
        self.assertIn('BRepGProp::VolumeProperties',fn)
        self.assertIn('*results=std::move(stagedResults)',fn)
    def test_design_ui_exposes_explicit_commit(self):
        self.assertIn('exploreParametricDesignAlternatives',self.ui)
        self.assertIn('document_.setNamedParameterBatch(candidates[index].changes',self.ui)
        self.assertIn('document_.evaluateParametricDesignAlternatives',self.ui)
    def test_workspace_samples_are_real_regenerated_solids(self):
        fn=self.core.split('bool SceneDocument::buildLiveArticulatedEnvelopeCsv(',1)[1].split('bool SceneDocument::updateLiveArticulatedCoordinates(',1)[0]
        for term in ['stageLiveArticulatedRegeneration','BRepBndLib::Add','joint_motion::interpolate','*csv=std::move(generated)']:
            self.assertIn(term,fn)
        self.assertIn('buildLiveArticulatedEnvelopeCsv(targets',self.ui)
    def test_multi_part_export_is_atomic(self):
        fn=self.core.split('bool SceneDocument::exportLinkedFlatManufacturingBatch(',1)[1].split('bool SceneDocument::editFoldedSheetMetal(',1)[0]
        for term in ['buildLinkedFlatDevelopmentSet','QTemporaryDir staging','exportLinkedFlatManufacturingPackage','batch-manifest.json','QDir().rename(staging.path(),newDirectory)']:
            self.assertIn(term,fn)
        self.assertIn('exportSelectedAssociativeFlatJobBatch',self.ui)
    def test_old_failures_not_hidden(self):
        c=(root/'CMakeLists.txt').read_text(encoding="utf-8")
        self.assertIn('cadcore_folded_sheet_metal',c)
        self.assertIn('cadcore_v170_assembly_mate_document',c)
if __name__=='__main__':unittest.main()

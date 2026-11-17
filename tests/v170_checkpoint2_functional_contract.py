"""Source-contract guards for checkpoint 2, not a substitute for native Qt tests."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
def source(name): return (ROOT/name).read_text(encoding="utf-8")
class Checkpoint2Integration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.core=source('src/core/SceneDocument.cpp')
        cls.head=source('src/core/SceneDocument.h')
        cls.ui=source('src/ui/MainWindow.cpp')
        cls.uihead=source('src/ui/MainWindow.h')
        cls.lineage=source('src/core/BooleanReferenceLineage.cpp')
        cls.edge_tests=source('tests/face_lineage_v170_tests.cpp')
        cls.mate_tests=source('tests/articulated_assembly_scene_v170_tests.cpp')
        cls.sheet_tests=source('tests/cadcore_tests.cpp')
    def test_edge_original_physical_witness_survives_kernel_rebase(self):
        method=self.lineage.split('bool rebaseFeatureThroughBoolean(',1)[1].split('bool supportsRigidReferenceLineage(',1)[0]
        for item in ('successors(result,TopAbs_EDGE','resolveStableEdgeAnchor(result,oldAnchor,',
                     'captureStableEdgeAnchor(result,chosen,','fresh.insert(QString::fromLatin1(field),oldAnchor.value('):
            self.assertIn(item,method)
        self.assertLess(method.index('resolveStableEdgeAnchor(result,oldAnchor,'),
                        method.index('fresh.insert(QString::fromLatin1(field),oldAnchor.value('))
    def test_edge_two_stage_native_regression(self):
        for token in ('edgeTrim1','edgeTrim2','afterEdgeTrim1','afterEdgeTrim2',
                      'edge witness','resolveStableEdgeAnchor(edgeTrim2.Shape()'):
            self.assertIn(token,self.edge_tests)
    def test_articulated_definition_core_api(self):
        for token in ('previewLiveArticulatedJointDefinitionEdit','replaceLiveArticulatedJointDefinition'):
            self.assertIn(token,self.head)
            self.assertIn('SceneDocument::'+token,self.core)
    def test_articulated_preview_is_detached(self):
        method=self.core.split('bool SceneDocument::previewLiveArticulatedJointDefinitionEdit(',1)[1].split(
            'bool SceneDocument::replaceLiveArticulatedJointDefinition(',1)[0]
        self.assertIn('stageLiveArticulatedRegeneration(definitions,&staged,&moved,error)',method)
        self.assertIn('inspectAssemblyFit(detached,ids,requiredClearanceMm,&pairs,error)',method)
        self.assertNotIn('checkpoint();',method)
        self.assertLess(method.index('stageLiveArticulatedRegeneration'),method.index('*report=lines.join'))
    def test_articulated_commit_is_transactional_and_noop_safe(self):
        method=self.core.split('bool SceneDocument::replaceLiveArticulatedJointDefinition(',1)[1].split(
            'bool SceneDocument::removeLiveArticulatedNetwork(',1)[0]
        self.assertLess(method.index('sameArticulatedDefinition('),method.index('checkpoint();'))
        self.assertLess(method.index('stageLiveArticulatedRegeneration('),method.index('checkpoint();'))
        self.assertIn('liveArticulatedJoints_=std::move(definitions)',method)
    def test_explicit_joint_limits_survive_followup_drives(self):
        core=self.core.split('bool applyArticulatedTargets(',1)[1].split(
            'bool SceneDocument::previewLiveArticulatedCoordinates(',1)[0]
        self.assertIn('joint.maximum-joint.minimum>1e-10',core)
        self.assertIn('outside the saved joint\'s explicit travel limits',core)
        self.assertIn('joint.minimum=it.value();joint.maximum=it.value();',core)
        for token in ('previewLiveArticulatedJointDefinitionEdit','replaceLiveArticulatedJointDefinition',
                      'legal drive edit inside explicit limits rejected','joint definition was not persisted'):
            self.assertIn(token,self.mate_tests)
    def test_joint_authoring_gui_with_preview_and_confirmation(self):
        method=self.ui.split('void MainWindow::editSavedArticulatedJointDefinition()',1)[1].split(
            'void MainWindow::editSavedArticulatedDrives()',1)[0]
        self.assertIn('norm<1e-8',method)
        self.assertIn('proposed.childAtZero.translation=',method)
        self.assertIn('proposed.minimum=',method)
        self.assertLess(method.index('previewLiveArticulatedJointDefinitionEdit('),
                        method.index('replaceLiveArticulatedJointDefinition('))
        self.assertIn('editSavedArticulatedJointDefinition();',self.uihead)
    def test_structural_bend_edit_stages_dependent_flat_blanks(self):
        method=self.core.split('bool SceneDocument::editFoldedSheetMetalWithDevelopments(',1)[1].split(
            'bool SceneDocument::loftRectangles(',1)[0]
        for token in ('linkedFoldedFlatSourceId!=partId','it.value().size()!=bends.size()',
                      'linked.linkedFoldedFlatSourceToken.clear()','propagateLinkedBodies(&proposed,error)',
                      'checkpoint();parts_=std::move(proposed)'):
            self.assertIn(token,method)
        self.assertLess(method.index('propagateLinkedBodies(&proposed,error)'),method.index('checkpoint();'))
    def test_structural_sheet_gui_requires_explicit_factors(self):
        method=self.ui.split('void MainWindow::editFoldedSheetMetal()',1)[1].split('bool MainWindow::saveFile(',1)[0]
        self.assertIn('oldBendCount!=bends.size()',method)
        self.assertIn('QDialog development(this)',method)
        self.assertIn('newDevelopments.insert(it.key(),parsed)',method)
        self.assertLess(method.index('previewFoldedSheetMetalEdit('),
                        method.index('editFoldedSheetMetalWithDevelopments('))
        for token in ('expandedK','expandedFlanges','editFoldedSheetMetalWithDevelopments',
                      'structural source/blank edit did not undo together'):
            self.assertIn(token,self.sheet_tests)
    def test_native_inventory_retained(self):
        self.assertEqual(len(re.findall(r'add_test\(NAME\s+(\w+)',source('CMakeLists.txt'))),130)
if __name__=='__main__':unittest.main()

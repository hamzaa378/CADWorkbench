"""Source integration checks. These do not replace native MSVC/Qt/OCCT tests."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent

def source(path):
    return (ROOT / path).read_text(encoding="utf-8")

class DeepEngineeringSourceContract(unittest.TestCase):
    def test_physical_face_witness_is_not_replaced_by_centroid(self):
        s=source('src/core/BooleanReferenceLineage.cpp')
        self.assertIn('persistentWitness(oldAnchor.value("face_anchor_witness_x")',s)
        self.assertIn('insideTrimmedPlanarFace(selectedSuccessor,persistentWitness)',s)
        self.assertIn('fresh.insert("face_anchor_witness_x",persistentWitness.X())',s)

    def test_secondary_witness_not_recreated_after_trim(self):
        s=source('src/core/BooleanReferenceLineage.cpp')
        self.assertIn('fresh.remove(QStringLiteral("face_anchor_witness2_")',s)
        self.assertIn('insideTrimmedPlanarFace(selectedSuccessor,second)',s)

    def test_multiple_boolean_steps_have_native_regression(self):
        s=source('tests/face_lineage_v170_tests.cpp')
        self.assertIn('second Boolean trim fixture',s)
        self.assertIn('co-planar face merge fixture',s)

    def test_batch_mechanism_is_one_transaction(self):
        s=source('src/core/SceneDocument.cpp')
        block=s[s.index('bool SceneDocument::updateLiveArticulatedCoordinates('):s.index('bool SceneDocument::removeLiveArticulatedNetwork(')]
        self.assertLess(block.index('stageLiveArticulatedRegeneration('),block.index('checkpoint();'))
        self.assertIn('liveArticulatedJoints_=std::move(proposed);',block)

    def test_batch_preview_is_read_only_and_reports_fit_limits(self):
        s=source('src/core/SceneDocument.cpp')
        block=s[s.index('bool SceneDocument::previewLiveArticulatedCoordinates('):s.index('bool SceneDocument::updateLiveArticulatedCoordinates(')]
        self.assertNotIn('checkpoint();',block)
        self.assertIn('inspectAssemblyFit(scene,ids,requiredClearanceMm,&pairs,error)',block)
        self.assertIn('Pairwise solid interference NOT CHECKED',block)

    def test_batch_joint_controls_reach_document_api(self):
        s=source('src/ui/MainWindow.cpp')
        self.assertIn('&MainWindow::editSavedArticulatedDrives',s)
        self.assertIn('document_.previewLiveArticulatedCoordinates(targets',s)
        self.assertIn('document_.commitLiveArticulatedCoordinatesChecked(targets',s)
        core=source('src/core/SceneDocument.cpp')
        body=core[core.index('bool SceneDocument::commitLiveArticulatedCoordinatesChecked('):core.index('namespace {\n// An edited joint stays the same saved graph edge.')]
        self.assertIn('previewLiveArticulatedCoordinates(targets',body)
        self.assertIn('previewLiveArticulatedTrajectory(targets',body)
        self.assertLess(body.index('previewLiveArticulatedCoordinates(targets'),body.index('updateLiveArticulatedCoordinates(targets'))

    def test_saved_folded_dependent_is_in_topological_plan(self):
        s=source('src/core/SceneDocument.cpp')
        graph=s[s.index('std::vector<linkedgraph::Node> linkedBodyGraphNodes('):s.index('bool planLinkedBodyOrder(')]
        planner=s[s.index('bool planLinkedBodyOrder('):s.index('bool linkedBooleanResult(')]
        self.assertIn('!part.linkedFoldedFlatSourceId.isEmpty()',graph)
        self.assertIn('node.primary=part.linkedFoldedFlatSourceId.toStdString();',graph)
        self.assertIn('const auto nodes=linkedBodyGraphNodes(parts);',planner)

    def test_folded_edit_regenerates_dependents_before_checkpoint(self):
        s=source('src/core/SceneDocument.cpp')
        block=s[s.index('bool SceneDocument::editFoldedSheetMetal('):s.index('bool SceneDocument::loftRectangles(')]
        self.assertLess(block.index('if(!propagateLinkedBodies(&proposed,error))return false;'),block.index('checkpoint();'))

    def test_folded_dependent_persistence_is_checked(self):
        s=source('src/core/SceneDocument.cpp')
        self.assertIn('item.insert("linked_folded_flat_blank",flatLink);',s)
        self.assertIn('Saved flat blank no longer matches folded source development.',s)
        self.assertIn('source.geometryToken!=source.foldedSheetGeometryToken',s)

    def test_delete_does_not_orphan_flat_blank(self):
        s=source('src/core/SceneDocument.cpp')
        self.assertIn('Delete the dependent blank too.',s)
        self.assertIn('!requested.contains(part.id)&&requested.contains(part.linkedFoldedFlatSourceId)',s)

    def test_native_regression_exercises_link_and_undo(self):
        s=source('tests/cadcore_tests.cpp')
        self.assertIn('create associative neutral flat blank',s)
        self.assertIn('source edit and flat blank did not undo atomically',s)
        self.assertIn('linked flat blank did not survive save/reopen',s)

    def test_derived_flat_blanks_are_read_only_to_scaling(self):
        s=source('src/core/SceneDocument.cpp')
        scale=s[s.index('bool SceneDocument::scale('):s.index('bool SceneDocument::scaleNonUniform(')]
        self.assertIn('if(!part->linkedFoldedFlatSourceId.isEmpty())',scale)
        native=source('tests/cadcore_tests.cpp')
        self.assertIn('direct scaling of associated neutral blank bypassed dependency guard',native)

    def test_native_batch_regression_exercises_persistence(self):
        s=source('tests/articulated_assembly_scene_v170_tests.cpp')
        self.assertIn('atomically commit two articulated drives',s)
        self.assertIn('saved multi-drive coordinates were lost',s)

if __name__=='__main__':
    unittest.main()

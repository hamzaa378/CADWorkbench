"""Source-level release gate for face lineage, in-place features and mates.

These checks do not replace Windows compilation/native CTests.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
def source(name):
    return (ROOT / name).read_text(encoding='utf-8')

class ThreeFoundationsPreflight(unittest.TestCase):
    def test_face_lineage_uses_trimmed_interior_and_rejects_ambiguous_successors(self):
        text = source('src/core/ShapeFeatureTree.cpp')
        for token in ('BRepClass_FaceClassifier','face_anchor_witness_x',
                      'witnessOnFace(face,trial,1e-5)',
                      'Face lineage is ambiguous after a split/merge',
                      'resolvePlanarFaceAnchor('):
            self.assertIn(token,text)

    def test_in_place_features_use_original_body_and_graph_transaction(self):
        text = source('src/core/SceneDocument.cpp')
        for token in ('bool SceneDocument::setInPlaceSketchFeature(',
                      'part.inPlaceSketchBase=part.shape',
                      '!linkedBooleanResult(input,tool,operation,output,error)',
                      'if(!propagateLinkedBodies(&candidate,error))return false;',
                      'checkpoint();parts_=std::move(candidate);return true;',
                      'bool SceneDocument::removeInPlaceSketchFeature('):
            self.assertIn(token,text)

    def test_saved_inplace_geometry_uses_volume_and_bounds(self):
        text=source('src/core/SceneDocument.cpp')
        self.assertIn('"in_place_sketch_feature"',text)
        self.assertIn('Saved in-place sketch feature does not match its original geometry.',text)
        self.assertIn('Saved in-place sketch geometry bounds differ from its source feature.',text)
        self.assertIn('source.constrainedPolygonGeometryToken!=source.geometryToken',text)

    def test_mate_solver_supports_three_constraint_types_and_rank_check(self):
        head=source('src/core/AssemblyMateSolver.h')
        core=source('src/core/AssemblyMateSolver.cpp')
        for token in ('Point, CoaxialAxis, CoincidentPlane','struct Constraint','struct Transform'):
            self.assertIn(token,head)
        for token in ('normalEquations(', 'linearSolve(a,b,dummy,1e-9)',
                      'originParent=constraints.front().parentPoint',
                      'Assembly mates conflict or did not converge to tolerance.'):
            self.assertIn(token,core)

    def test_mate_regression_survives_release_ndebug(self):
        test=source('tests/assembly_mate_solver_v170_tests.cpp')
        self.assertNotIn('assert(',test)
        self.assertIn('throw std::runtime_error(message)',test)

    def test_ui_is_connected_to_both_document_actions(self):
        ui=source('src/ui/MainWindow.cpp')
        self.assertIn('document_.setInPlaceSketchFeature(',ui)
        self.assertIn('document_.solveAssemblyMates(',ui)
        self.assertIn('document_.removeInPlaceSketchFeature(',ui)

    def test_release_test_registry(self):
        cm=source('CMakeLists.txt')
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        for expected in ('cadcore_v170_face_lineage','cadcore_v170_inplace_sketch_features',
                         'cadcore_v170_inplace_sketch_persistence','cadcore_v170_assembly_mate_solver',
                         'cadcore_v170_assembly_mate_document'):
            self.assertIn(expected,names)
        self.assertIn('ExpectedTests = 130',source('scripts/verify_release_windows.ps1'))

if __name__=='__main__':unittest.main()

"""Static integration contracts; not native C++ compilation."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
def source(path):return (ROOT/path).read_text(encoding='utf8')
class LineageExtensionContracts(unittest.TestCase):
    def test_spline_edge_anchor_version_and_validation(self):
        core=source('src/core/ShapeFeatureTree.cpp')
        for token in ('GeomAbs_BSplineCurve','GeomAbs_BezierCurve',
                      'captured.insert("edge_anchor_version",spline?3.0:2.0)',
                      'kind==3.0&&version!=3.0','if(!compatible)continue;',
                      'GeomAPI_ProjectPointOnCurve projected(',
                      'Edge witness is missing or lies on a split boundary'):
            self.assertIn(token,core)
        tests=source('tests/selected_edge_v160_tests.cpp')
        for token in ('unique trimmed spline successor',
                      'spline edge witness follows exact rotation',
                      'v3 curve kind rejects legacy v2 version'):
            self.assertIn(token,tests)
    def test_rigid_walk_is_explicit_and_transactional(self):
        doc=source('src/core/SceneDocument.cpp')
        h=source('src/core/BooleanReferenceLineage.h')
        cpp=source('src/core/BooleanReferenceLineage.cpp')
        for token in ('rigidOperations.insert(step.id,step.feature)',
                      'addParents(rigidSources);',
                      'rebaseFeatureThroughSimilarity(',
                      'rigidSources.insert(step.id,',
                      'if(!rebuildShapeFeatureTree(input,false,{executable},&next,error,'):
            self.assertIn(token,doc)
        self.assertIn('supportsRigidReferenceLineage',h)
        self.assertIn('supportsSimilarityReferenceLineage',h)
        for token in ('Rotate arbitrary axis','SetRotation(', 'SetTranslation(',
                      'planarAxes(', 'designPoint.Transform(tr)',
                      'resolveStableEdgeAnchor(result,moved',
                      'resolvePlanarFaceAnchor(result,moved'):
            self.assertIn(token,cpp)
        self.assertIn('operation.type==QStringLiteral("Mirror XY")',cpp)
    def test_curved_shell_tracking_and_ui(self):
        core=source('src/core/ShapeFeatureTree.cpp')
        cpp=source('src/core/BooleanReferenceLineage.cpp')
        ui=source('src/ui/MainWindow.cpp')
        for token in ('kCurvedAnchorFields','Curved face references are currently supported for shell openings only.',
                      'return resolveCurvedFaceAnchor(body,anchor,selected,error);'):
            self.assertIn(token,core)
        self.assertIn('captureCurvedFaceAnchor(body->shape,pickedFaces.first().faceIndex',ui)
        self.assertIn('Curved shell witness no longer belongs to its kernel-confirmed Boolean descendants.',cpp)
        self.assertIn('resolveCurvedFaceAnchor(result,moved',cpp)
        self.assertIn('curved shell opening follows kernel cut history',
                      source('tests/curved_face_lineage_v170_tests.cpp'))
if __name__=='__main__':unittest.main()

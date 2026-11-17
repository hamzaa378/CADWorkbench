"""Source contracts for v1.70 mixed history, curved faces, and rigid loops.
Only native Windows CTest can verify the actual Qt/OpenCascade integration.
"""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
def text(relative):return (ROOT/relative).read_text(encoding='utf-8')

class ThreeGapContracts(unittest.TestCase):
    def test_unique_registration_and_gate(self):
        cm=text('CMakeLists.txt')
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        for name in ('cadcore_v170_closed_loop_rigid_network',
                     'cadcore_v170_closed_loop_scene',
                     'cadcore_v170_curved_face_lineage',
                     'cadcore_v170_mixed_feature_history'):
            self.assertIn(name,names)
        self.assertIn('[int]$ExpectedTests = 130',text('scripts/verify_release_windows.ps1'))
    def test_rigid_graph_validates_cycles_and_disconnections(self):
        net=text('src/core/AssemblyConstraintNetwork.cpp')
        doc=text('src/core/SceneDocument.cpp')
        for token in ('rigid frame','graph is disconnected','Closed assembly loop is inconsistent',
                      '*solution=std::move(next)'):
            self.assertIn(token,net)
        body=doc.split('bool SceneDocument::solveAssemblyMateNetwork(',1)[1].split('bool SceneDocument::setJointMotionKeyframes(',1)[0]
        self.assertIn('mates::solveRigidNetwork',body)
        self.assertIn('BRepBuilderAPI_Transform(at->shape,delta,true)',body)
        self.assertLess(body.index('mates::solveRigidNetwork'),body.index('checkpoint();parts_=std::move(candidate)'))
        self.assertIn('freeOccurrencePlacement(*at)',body)
    def test_mixed_history_and_save_guard(self):
        doc=text('src/core/SceneDocument.cpp')
        self.assertIn('if(linked.hasShapeFeatureTree)',doc)
        self.assertIn('rebuildShapeFeatureTree(modified,linked.isSurfaceBody,',doc)
        self.assertIn('const TopoDS_Shape savedInPlaceResult=',doc)
        self.assertIn('child.hasShapeFeatureTree?child.shapeFeatureBase:child.shape',doc)
        self.assertIn('rebuildShapeFeatureTree(original,part.isSurfaceBody,',doc)
    def test_curved_witness_and_ambiguity(self):
        core=text('src/core/ShapeFeatureTree.cpp')
        for key in ('captureCurvedFaceAnchor(', 'resolveCurvedFaceAnchor(',
                    'GeomAbs_Cylinder','GeomAbs_Sphere','BRepClass_FaceClassifier classification',
                    'Curved-face lineage is ambiguous'):
            self.assertIn(key,core)
        self.assertIn('if(matches>1)',core)
        self.assertIn('curved_witness_z',core)

if __name__=='__main__':unittest.main()

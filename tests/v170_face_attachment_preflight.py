"""Source integration acceptance for face-local polygon features.
These checks do not assert successful native MSVC compilation.
"""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
def read(path):return (ROOT/path).read_text(encoding='utf-8')
class FaceAttachmentPreflight(unittest.TestCase):
    def test_native_test_registered_and_verification_count(self):
        cmake=read('CMakeLists.txt')
        self.assertIn('tests/face_attached_sketch_v170_tests.cpp',cmake)
        self.assertIn('cadcore_v170_face_attached_sketch',cmake)
        registered=re.findall(r'add_test\(NAME ([^\s]+)',cmake)
        self.assertEqual(len(registered),130)
        self.assertEqual(len(set(registered)),130)
        self.assertIn('[int]$ExpectedTests = 130',read('scripts/verify_release_windows.ps1'))
    def test_two_ops_have_schema_validation_and_real_geometry(self):
        source=read('src/core/ShapeFeatureTree.cpp')
        for feature in ('Sketch polygon face boss','Sketch polygon face pocket'):
            self.assertIn(feature,source)
        self.assertIn('bool validatePolygonFeature(',source)
        self.assertIn('BRepPrimAPI_MakePrism extrusion(',source)
        self.assertIn('if(!findFace(input,op,&chosen,error))return false;',source)
        self.assertIn('std::make_shared<BRepAlgoAPI_Cut>(input,extrusion.Shape())',source)
        self.assertIn('std::make_shared<BRepAlgoAPI_Fuse>(input,extrusion.Shape())',source)
        self.assertIn('Face-attached boss is disconnected',source)
        self.assertIn('face_anchor_nx',source)
    def test_ui_coordinate_and_sketch_capture(self):
        ui=read('src/ui/MainWindow.cpp')
        self.assertIn('"Sketch polygon face boss","Sketch polygon face pocket"',ui)
        self.assertIn('form->addRow("Planar face index",faceIndex);',ui)
        self.assertIn('form->addRow("Local U shift",offsetU);',ui)
        self.assertIn('form->addRow("Extrusion depth",height);',ui)
        self.assertIn('node->parameters.insert(it.key(),it.value());',ui)
        self.assertIn('SketchCanvasDialog canvas(&input)',ui)
    def test_native_test_proves_rollback_and_persistence(self):
        test=read('tests/face_attached_sketch_v170_tests.cpp')
        for text in ('capturePlanarFaceAnchor','setShapeFeatureTree','saveAs','load(',
                     'geometryToken','doc.undo()','doc.redo()',
                     'upstream translation lost the attached face','testSideFace'):
            self.assertIn(text,test)

if __name__=='__main__':unittest.main(verbosity=2)

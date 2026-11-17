"""Static release wiring checks; do not substitute for the native Windows tests."""
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def content(path):return (ROOT/path).read_text(encoding='utf-8')

class V167Preflight(unittest.TestCase):
    def test_release_gates(self):
        cm=content('CMakeLists.txt')
        self.assertIn('project(CADWorkbench VERSION 1.70.0 LANGUAGES CXX)',cm)
        names=re.findall(r'add_test\(NAME ([^\s)]+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        self.assertIn('ExpectedTests = 130',content('scripts/verify_release_windows.ps1'))
    def test_independent_validator_is_real_cpp(self):
        cm=content('CMakeLists.txt')
        for name in ('src/core/PolygonFeatureProfile.h','src/core/PolygonFeatureProfile.cpp',
                     'tests/polygon_feature_profile_v167_tests.cpp','tests/sketch_polygon_geometry_v167_tests.cpp'):
            self.assertTrue((ROOT/name).exists(),name)
            self.assertIn(name,cm)
        self.assertIn('cadcore_v167_polygon_profile_validation',cm)
        self.assertIn('cadcore_v167_sketch_polygon_features',cm)
    def test_feature_tree_wires_and_validation(self):
        core=content('src/core/ShapeFeatureTree.cpp')
        ui=content('src/ui/MainWindow.cpp')
        for feature in ('Sketch polygon extrude Z','Sketch polygon pocket Z','Sketch polygon revolve Z',
                        'Sketch polygons loft Z'):
            self.assertIn(feature,core)
            self.assertIn(feature,ui)
        for check in ('validatePolygonFeature','polygon::validateLoft','polygon::validate(lower',
                      'BRepPrimAPI_MakePrism maker','BRepPrimAPI_MakeRevol maker',
                      'BRepOffsetAPI_ThruSections maker','BRepAlgoAPI_Cut operation',
                      'BRepAlgoAPI_Fuse operation'):
            self.assertIn(check,core,check)
        self.assertIn('SketchCanvasDialog canvas(&input)',ui)
        self.assertIn('QPlainTextEdit',ui)
    def test_existing_json_schema_is_unchanged(self):
        code=content('src/core/SceneDocument.cpp')
        self.assertIn('shape-independent-modifiers-v1',code)
        self.assertIn('if(!validateShapeFeatureNodes(nodes,error))return false',code)
        self.assertIn('node.parameters.insert(it.key(),it.value().toDouble())',code)
    def test_no_workaround_casts(self):
        for p in (ROOT/'src/core').glob('*.cpp'):
            text=p.read_text(encoding="utf-8")
            self.assertNotIn('#define TopTools_IndexedMapOfShape',text)

if __name__=='__main__':unittest.main(verbosity=2)

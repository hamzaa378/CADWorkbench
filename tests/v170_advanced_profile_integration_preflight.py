"""Static integration contracts only, not a substitute for native Windows CTests."""
import unittest
import re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(name):return (ROOT/name).read_text(encoding='utf-8')
class AdvancedProfileIntegration(unittest.TestCase):
    def test_release_version_and_gate(self):
        cm=read('CMakeLists.txt')
        self.assertIn('project(CADWorkbench VERSION 1.70.0 LANGUAGES CXX)',cm)
        tests=re.findall(r'add_test\(NAME ([^\s)]+)',cm)
        self.assertEqual(len(tests),130)
        self.assertEqual(len(tests),len(set(tests)))
        self.assertIn('[int]$ExpectedTests = 130',read('scripts/verify_release_windows.ps1'))
    def test_three_real_occt_geometry_operations(self):
        core=read('src/core/ShapeFeatureTree.cpp')
        ui=read('src/ui/MainWindow.cpp')
        for name in ('Sketch polygon sweep 3D','Sketch multi-section loft Z','Sketch polygon revolve axis'):
            self.assertIn(name,core)
            self.assertIn(name,ui)
        for operation in ('BRepOffsetAPI_MakePipeShell pipe','BRepOffsetAPI_ThruSections maker(true,false)',
                          'BRepPrimAPI_MakeRevol maker','BRepAlgoAPI_Fuse fused','geometryOkay(addition,false,error)'):
            self.assertIn(operation,core)
        self.assertIn('return validateAdvancedPolygonFeature(node,error)',core)
    def test_sources_tests_and_dialog_registered(self):
        cm=read('CMakeLists.txt')
        for filename in ('src/core/AdvancedSketchProfiles.h','src/core/AdvancedSketchProfiles.cpp',
                        'tests/advanced_sketch_profile_v170_tests.cpp',
                        'tests/advanced_sketch_geometry_v170_tests.cpp'):
            self.assertTrue((ROOT/filename).is_file(),filename)
            self.assertIn(filename,cm)
        self.assertIn('cadcore_v170_advanced_profile_validation',cm)
        self.assertIn('cadcore_v170_advanced_sketch_geometry',cm)
        self.assertIn('editAdvancedProfile(node)',read('src/ui/MainWindow.cpp'))
        self.assertIn('QPlainTextEdit',read('src/ui/MainWindow.cpp'))
    def test_schemas_are_persisted_with_existing_node_format(self):
        code=read('src/core/SceneDocument.cpp')
        self.assertIn('node.parameters.insert(it.key(),it.value().toDouble())',code)
        self.assertIn('if(!validateShapeFeatureNodes(nodes,error))return false',code)
    def test_transactional_rollback_remains(self):
        scene=read('src/core/SceneDocument.cpp')
        self.assertIn('rebuildShapeFeatureTree(',scene)
        native=read('tests/advanced_sketch_geometry_v170_tests.cpp')
        for marker in ('!doc.setShapeFeatureTree','doc.undo()','doc.redo()','doc.saveAs','loaded.load'):
            self.assertIn(marker,native)
class SplineGuideIntegration(unittest.TestCase):
    def test_spline_sweep_has_real_interpolated_spine(self):
        tree=read('src/core/ShapeFeatureTree.cpp')
        ui=read('src/ui/MainWindow.cpp')
        native=read('tests/advanced_sketch_geometry_v170_tests.cpp')
        for fragment in ('Sketch polygon sweep spline 3D',
                         'GeomAPI_Interpolate interpolation(stations,false,1e-6)',
                         'spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(curve).Edge()).Wire()',
                         'curve->D1(curve->FirstParameter(),start,tangent)'):
            self.assertIn(fragment,tree)
        self.assertIn('Sketch polygon sweep spline 3D',ui)
        self.assertIn('exercise("Sketch polygon sweep spline 3D")',native)

if __name__=='__main__':unittest.main(verbosity=2)

"""Static integration checks: body-relative geometric anchor increment.
These checks do not assert native MSVC compilation.
"""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]

def content(path):return (ROOT/path).read_text(encoding='utf-8')

class BodyRelativeAnchorPreflight(unittest.TestCase):
    def test_backwards_compatible_capture_schema(self):
        c=content('src/core/ShapeFeatureTree.cpp')
        self.assertIn('const QStringList kFaceBodyFields=',c)
        self.assertIn('const QStringList kEdgeBodyFields=',c)
        self.assertIn('"face_anchor_body_x",reference.X()',c)
        self.assertIn('captured.insert("edge_body_x",reference.X())',c)
        self.assertIn('if(node.parameters.contains("face_anchor_body_x"))',c)
        self.assertIn('if(hasBody)',c)
        self.assertIn('old.Translate(gp_Vec(oldBody,shapeReferenceCenter(body)))',c)
        self.assertIn('completeOptionalFields',c)
    def test_feature_consumers_do_not_strip_new_edge_fields(self):
        c=content('src/core/ShapeFeatureTree.cpp')
        self.assertEqual(c.count('for(const QString& key:kEdgeBodyFields)anchor.insert(key,op.parameters.value(key));'),2)
        ui=content('src/ui/MainWindow.cpp')
        self.assertIn('it.key().startsWith("face_anchor_")',ui)
        self.assertIn('it.key().startsWith("edge_")',ui)
    def test_native_integration_covers_large_translation_and_legacy(self):
        face=content('tests/face_attached_sketch_v170_tests.cpp')
        edge=content('tests/selected_edge_v160_tests.cpp')
        legacy=content('tests/associative_features_v159_tests.cpp')
        for term in ('large upstream translation','legacy face anchor','incomplete body-relative'):
            self.assertIn(term,face)
        self.assertIn('Anchored edge feature failed after a large upstream translation',edge)
        self.assertIn('Body-relative edge hint failed after large translation',legacy)
        self.assertIn('Legacy four-field edge hint stopped resolving',legacy)
    def test_version_and_existing_test_gate_unchanged(self):
        cmake=content('CMakeLists.txt')
        tests=re.findall(r'add_test\(NAME\s+(\S+)',cmake)
        self.assertEqual(len(tests),130)
        self.assertEqual(len(tests),len(set(tests)))
        self.assertIn('project(CADWorkbench VERSION 1.70.0',cmake)
        self.assertIn('[int]$ExpectedTests = 130',content('scripts/verify_release_windows.ps1'))

if __name__=='__main__':unittest.main(verbosity=2)

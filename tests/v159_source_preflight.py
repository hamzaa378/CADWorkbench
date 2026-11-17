"""Static source preflight. This is NOT an MSVC compile or geometry test."""
from pathlib import Path
import re, unittest
from pygments import lex
from pygments.lexers import CppLexer
from pygments.token import Punctuation, Error
ROOT=Path(__file__).resolve().parents[1]
def read(name):return (ROOT/name).read_text(encoding='utf8')
class Preflight(unittest.TestCase):
    def test_legacy_plane_compiler_fix(self):
        code=read('tests/cadcore_tests.cpp')
        self.assertRegex(code,r'struct PlaneCase\s*\{\s*cad::ExtrusionSketchPlane\s+plane;')
        self.assertRegex(code,r'struct ThroughAllCase\s*\{\s*cad::ExtrusionSketchPlane\s+plane;')
        self.assertNotRegex(code,r'struct (?:PlaneCase|ThroughAllCase)\s*\{\s*cad::SketchPlane\b')
        self.assertEqual(read('src/core/SketchProfile.h').count('struct SketchPlane {'),1)
        self.assertEqual(read('src/core/SceneDocument.h').count('struct SketchPlane {'),0)
    def test_feature_tree_api_and_ui(self):
        core=read('src/core/ShapeFeatureTree.cpp');gui=read('src/ui/MainWindow.cpp')
        for op in ['Revolve annulus Z','Sweep circle path','Loft rectangles']:
            self.assertIn('"'+op+'"',core);self.assertIn('"'+op+'"',gui)
        self.assertIn('BRepOffsetAPI_ThruSections',core)
        self.assertIn('BRepPrimAPI_MakeRevol',core)
        self.assertIn('BRepOffsetAPI_MakePipeShell',core)
    def test_stable_topology_hint_contract(self):
        core=read('src/core/ShapeFeatureTree.cpp');gui=read('src/ui/MainWindow.cpp')
        for api in ['captureStableEdgeAnchor','resolveStableEdgeAnchor','capturePlanarFaceAnchor','findFace']:
            self.assertIn(api,core)
        self.assertIn('face_anchor_nx',core)
        self.assertIn('capturePlanarFaceAnchor',gui)
        self.assertIn('uniquely resolved',core)
    def test_stable_edge_deduplicated_index_contract(self):
        core=read('src/core/ShapeFeatureTree.cpp')
        start=core.index('bool captureStableEdgeAnchor(')
        end=core.index('bool validateShapeFeatureNodes(',start)
        implementation=core[start:end]
        # Capture, strict resolution, and opt-in drawing-only circle resolution
        # all use the same unique-edge map rather than repeated face traversal.
        self.assertEqual(implementation.count('TopExp::MapShapes(body,TopAbs_EDGE,edges)'),3)
        self.assertNotIn('TopExp_Explorer',implementation)
        self.assertIn('edges.FindKey(edgeIndex)',implementation)
        self.assertIn('edges.FindKey(position)',implementation)
        self.assertIn('runnerUp-best<0.01',implementation)
        regression=read('tests/associative_features_v159_tests.cpp')
        self.assertIn('uniqueEdges.Extent()==12',regression)
        self.assertIn('Identical but distinct edges',regression)
    def test_linked_graph_integrity(self):
        core=read('src/core/SceneDocument.cpp')
        for token in ['linked_boolean','propagateLinkedBodies','addLinkedBooleanBody','linkedBooleanCurrent']:
            self.assertIn(token,core)
        self.assertIn('source bodies instead',core)
        self.assertIn('Saved linked Boolean refers to invalid, cyclic, or missing sources',core)
        for method in ['setShapeFeatureTree','updateConstrainedPolygonExtrusion','translate','rotate',
                       'scale','scaleNonUniform','setNamedParametersWithLinks','setParametricFeaturesSuppressed']:
            begin=core.index('bool SceneDocument::'+method+'(')
            match=re.search(r'\n(?:bool|void|QMap<|QStringList) SceneDocument::\w+\(',core[begin+10:])
            end=begin+10+match.start() if match else len(core)
            self.assertIn('propagateLinkedBodies(&candidate,error)',core[begin:end],method)
    def test_canvas_constraints(self):
        h=read('src/ui/SketchCanvasDialog.h');cpp=read('src/ui/SketchCanvasDialog.cpp')
        self.assertIn('enum class Relation',h)
        self.assertIn('Click-to-constrain:',cpp)
        self.assertIn('handleRelationClick',cpp)
        self.assertIn('SketchSolver::solve',cpp)
        self.assertIn('buildSketchExtrusion(trial,boundary_',cpp)
    def test_test_registration(self):
        self.assertIn('cadcore_associative_features_v159',read('CMakeLists.txt'))
        self.assertTrue((ROOT/'tests/associative_features_v159_tests.cpp').is_file())
        self.assertGreaterEqual(read('CMakeLists.txt').count('add_test(NAME '),77)
    def test_punctuation_balanced_modified_sources(self):
        for name in ['src/core/SceneDocument.cpp','src/core/SceneDocument.h',
                     'src/core/ShapeFeatureTree.cpp','src/core/ShapeFeatureTree.h',
                     'src/ui/MainWindow.cpp','src/ui/MainWindow.h',
                     'src/ui/SketchCanvasDialog.cpp','src/ui/SketchCanvasDialog.h',
                     'tests/associative_features_v159_tests.cpp','tests/cadcore_tests.cpp']:
            stack=[]
            for token,value in lex(read(name),CppLexer()):
                if token in Error:self.fail(f'Lexical error in {name}: {value!r}')
                if token not in Punctuation:continue
                for char in value:
                    if char in '{([':stack.append(char)
                    elif char in '})]':
                        self.assertTrue(stack, f'{name}: unmatched closing {char}')
                        prev=stack.pop()
                        self.assertEqual(prev,{')':'(',']':'[','}':'{'}[char],f'{name}: mismatched brace')
            self.assertEqual(stack,[],f'{name}: unclosed delimiters')
if __name__=='__main__':unittest.main(verbosity=2)

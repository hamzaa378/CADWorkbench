"""v1.60 selected-edge and linked rebuild preflight. NOT an MSVC build."""
from pathlib import Path
import unittest
import re
from pygments import lex
from pygments.lexers import CppLexer
from pygments.token import Punctuation, Error
ROOT=Path(__file__).resolve().parents[1]
def source(path):return (ROOT/path).read_text(encoding='utf-8')

class V160Preflight(unittest.TestCase):
    def test_native_test_registration(self):
        self.assertGreaterEqual(source('CMakeLists.txt').count('add_test(NAME '),78)
        self.assertIn('cadcore_selected_edge_v160',source('CMakeLists.txt'))
        self.assertRegex(source('CMakeLists.txt'),r'project\(CADWorkbench VERSION 1\.[0-9]+\.[0-9]+ LANGUAGES CXX\)')
        self.assertTrue((ROOT/'tests/selected_edge_v160_tests.cpp').is_file())
    def test_stable_anchor_validation(self):
        cpp=source('src/core/ShapeFeatureTree.cpp')
        for item in ['Fillet selected edge','Chamfer selected edge','edge_index','kEdgeAnchorFields',
                     'hasEdgeAnchor','resolveStableEdgeAnchor(input,anchor,&selected,error)']:
            self.assertIn(item,cpp)
        self.assertIn('v<1||v>10000||std::floor(v)!=v',cpp)
        self.assertIn('Incomplete or invalid stable edge anchor',cpp)
    def test_actual_selected_edge_modifiers(self):
        cpp=source('src/core/ShapeFeatureTree.cpp')
        fillet=cpp[cpp.index('}else if(op.type=="Fillet all edges"'):cpp.index('}else if(op.type=="Chamfer all edges"')]
        chamfer=cpp[cpp.index('}else if(op.type=="Chamfer all edges"'):cpp.index('if(!geometryOkay(next')]
        for block in (fillet,chamfer):
            self.assertIn('TopExp::MapShapes(input,TopAbs_EDGE,edges)',block)
            self.assertIn('edges.FindKey(selected)',block)
            self.assertIn('resolveStableEdgeAnchor(input,anchor,&selected,error)',block)
            self.assertNotIn('TopExp_Explorer e(input,TopAbs_EDGE)',block)
    def test_ui_converts_viewport_edge_reference(self):
        ui=source('src/ui/MainWindow.cpp')
        for item in ['selectedUniqueEdge','pickedUniqueEdgeIndex','TopExp::MapShapes(body->shape,TopAbs_EDGE,unique)',
                     'unique.FindKey(i).IsSame(edge)','captureStableEdgeAnchor(body->shape',
                     'Fillet selected edge','Chamfer selected edge','[edge anchor]']:
            self.assertIn(item,ui)
    def test_direct_fillet_chamfer_propagate_atomically(self):
        core=source('src/core/SceneDocument.cpp')
        for method,next_method in [('filletAllEdges','prepareFillet'),('chamferAllEdges','draftFace')]:
            start=core.index('bool SceneDocument::'+method+'(')
            end=core.index('bool SceneDocument::'+next_method+'(',start)
            block=core[start:end]
            self.assertIn('if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature)',block)
            self.assertIn('TopExp::MapShapes(part->shape,TopAbs_EDGE,uniqueEdges)',block)
            self.assertIn('if(!propagateLinkedBodies(&candidate,error))return false;',block)
            self.assertLess(block.index('propagateLinkedBodies(&candidate,error)'),block.index('checkpoint()'))
    def test_preview_commits_are_transactional(self):
        core=source('src/core/SceneDocument.cpp')
        for method in ('commitFillet','commitChamfer'):
            start=core.index('bool SceneDocument::'+method+'(')
            end=core.index('\n}',start)
            block=core[start:end]
            self.assertIn('part->hasLinkedBoolean',block)
            self.assertIn('if(!propagateLinkedBodies(&candidate,error))return false;',block)
            self.assertLess(block.index('propagateLinkedBodies(&candidate,error)'),block.index('checkpoint()'))
        for method in ('prepareFillet','prepareChamfer','prepareAsymmetricChamfer'):
            start=core.index('bool SceneDocument::'+method+'(const QVector<EdgeReference>&') if method!='prepareAsymmetricChamfer' else core.index('bool SceneDocument::'+method+'(')
            end=core.index('\n}',start)
            self.assertIn('part->hasLinkedBoolean',core[start:end])
    def test_edited_cpp_lexes_without_unmatched_delimiters(self):
        for file in ['src/core/SceneDocument.cpp','src/core/ShapeFeatureTree.cpp',
                     'src/ui/MainWindow.cpp','tests/selected_edge_v160_tests.cpp']:
            stack=[]
            for token,txt in lex(source(file),CppLexer()):
                self.assertNotIn(token, Error, file)
                if token not in Punctuation:continue
                for ch in txt:
                    if ch in '([{':stack.append(ch)
                    elif ch in ')]}':
                        self.assertTrue(stack,file+' unmatched '+ch)
                        self.assertEqual(stack.pop(),{')':'(',']':'[','}':'{'}[ch],file)
            self.assertFalse(stack,file+' leftover delimiters')
if __name__=='__main__':unittest.main(verbosity=2)

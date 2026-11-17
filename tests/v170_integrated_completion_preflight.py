import pathlib,re,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
def read(path):return (ROOT/path).read_text(encoding='utf-8')
class CompletionCheck(unittest.TestCase):
    def test_registration(self):
        cm=read('CMakeLists.txt')
        names=re.findall(r'add_test\(NAME\s+(\w+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        for name in ('cadcore_v170_articulated_closed_loop',
                     'cadcore_v170_articulated_scene_transaction',
                     'cadcore_v170_interleaved_history'):
            self.assertIn(name,names)
        self.assertIn('[int]$ExpectedTests = 130',read('scripts/verify_release_windows.ps1'))
    def test_articulated_atomic(self):
        s=read('src/core/SceneDocument.cpp')
        self.assertIn('bool SceneDocument::solveArticulatedAssemblyNetwork(',s)
        start=s.index('bool SceneDocument::solveArticulatedAssemblyNetwork(')
        end=s.index('bool SceneDocument::setJointMotionKeyframes(',start)
        body=s[start:end]
        self.assertIn('articulated::solveNetwork(',body)
        self.assertIn('BRepBuilderAPI_Transform(',body)
        self.assertIn('checkpoint();parts_=std::move(candidate)',body)
        self.assertLess(body.index('if(!validShape(moved,error))'),body.index('checkpoint();'))
    def test_interleaved_history(self):
        s=read('src/core/SceneDocument.cpp')
        for marker in ('setInterleavedFeatureHistory','replayInterleavedHistory',
                       'interleavedSourceTokens','linkedSketchFuseEffective',
                       'linkedSketchCutEffective','if(child.hasInterleavedHistory)',
                       'if(linked.hasInterleavedHistory)','item.insert("interleaved_history"'):
            self.assertIn(marker,s)
        self.assertIn('A selected body drives an interleaved feature history',s)
        self.assertIn('Source edits rebuild',read('docs/V170_COMPLETION_ENGINEERING_INCREMENT.md'))
    def test_curved_support(self):
        s=read('src/core/ShapeFeatureTree.cpp')
        for marker in ('GeomAbs_Cone','GeomAbs_Torus','cone.RefRadius()',
                       'torus.MinorRadius()','curved_aux'):
            self.assertIn(marker,s)
        for marker in ('BRepPrimAPI_MakeCone','BRepPrimAPI_MakeTorus',
                       'translated cone resolution','translated torus resolution'):
            self.assertIn(marker,read('tests/curved_face_lineage_v170_tests.cpp'))
if __name__=='__main__':unittest.main()

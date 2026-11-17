"""Release-contract checks; not a substitute for MSVC compilation or CTest."""
import re
import unittest
from pathlib import Path

root = Path(__file__).resolve().parents[1]

def read(path):
    return (root / path).read_text(encoding='utf-8')

class SparseDAGPreflight(unittest.TestCase):
    def test_cmake_test_targets(self):
        cm=read('CMakeLists.txt')
        names=re.findall(r'add_test\(NAME\s+([^\s)]+)',cm)
        self.assertIn('project(CADWorkbench VERSION 1.70.0 LANGUAGES CXX)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(names),len(set(names)))
        for name in ('cadcore_v166_sparse_dependency_planner','cadcore_v166_sparse_feature_regeneration'):
            self.assertIn(name,names)
        for f in ('src/core/FeatureDependencyPlan.cpp','tests/feature_dependency_plan_v166_tests.cpp',
                  'tests/sparse_feature_regeneration_v166_tests.cpp'):
            self.assertTrue((root / f).is_file())
        self.assertIn('ExpectedTests = 130',read('scripts/verify_release_windows.ps1'))

    def test_cache_provenance_guards(self):
        doc=read('src/core/SceneDocument.cpp')
        core=read('src/core/ShapeFeatureTree.cpp')
        self.assertIn('part.shapeFeatureCachedOutputs.size()==part.shapeFeatureNodes.size()',doc)
        self.assertIn('shapeFeatureTreeCurrent(partId)',doc)
        self.assertIn('planShapeFeatureReuse(part.shapeFeatureNodes,nodes,available,&oldPositions,error)',doc)
        self.assertIn('if(!propagateLinkedBodies(&candidate,error))return false;',doc)
        self.assertIn('checkpoint();parts_=std::move(candidate);',doc)
        self.assertIn('if(!geometryOkay(saved,surface,error))',core)
        self.assertIn('reusableOutputs->contains(op.id)',core)
        self.assertIn('outputs.insert(op.id,saved)',core)

    def test_user_interface_rebuild_summary(self):
        ui=read('src/ui/MainWindow.cpp')
        self.assertIn('Planned rebuild: %2 reevaluated, %3 cached outputs reused.',ui)
        self.assertIn('validateShapeFeatureNodes(nodes,&graphError)',ui)
        self.assertIn('document_.setShapeFeatureTree(id,nodes,&error)',ui)
        self.assertIn('rebuildShapeFeatureTree(source,current->isSurfaceBody,nodes,&preview,&error)',ui)

    def test_regression_coverage(self):
        portable=read('tests/feature_dependency_plan_v166_tests.cpp')
        native=read('tests/sparse_feature_regeneration_v166_tests.cpp')
        for key in ('Independent branch invalidation leaked','Explicitly rooted reorder was not reused',
                    'Self link accepted','Broken dependency accepted'):
            self.assertIn(key,portable)
        for key in ('Save failed','Reload failed','Rejected feature changed document token',
                    'Sparse rebuild differs from full rebuild','Independent right child was rebuilt'):
            self.assertIn(key,native)

if __name__=='__main__':
    unittest.main(verbosity=2)

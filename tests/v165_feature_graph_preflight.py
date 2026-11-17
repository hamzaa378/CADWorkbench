"""v1.65 non-native release checks. Native MSVC+Qt+OCCT acceptance remains required."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
def src(name): return (ROOT/name).read_text(encoding='utf-8')
class FeatureGraphPreflight(unittest.TestCase):
    def test_version_registration_and_files(self):
        cm=src('CMakeLists.txt')
        self.assertIn('project(CADWorkbench VERSION 1.70.0 LANGUAGES CXX)', cm)
        names=re.findall(r'add_test\(NAME\s+([^\s)]+)', cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        self.assertIn('cadcore_v165_incremental_feature_graph',names)
        self.assertIn('incremental_feature_graph_v165_tests.cpp',cm)
        self.assertTrue((ROOT/'tests/incremental_feature_graph_v165_tests.cpp').is_file())
        self.assertIn('ExpectedTests = 130',src('scripts/verify_release_windows.ps1'))
    def test_cached_prefix_is_transactional(self):
        code=src('src/core/ShapeFeatureTree.cpp')
        header=src('src/core/ShapeFeatureTree.h')
        document=src('src/core/SceneDocument.cpp')
        self.assertIn('const QVector<TopoDS_Shape>* cachedPrefix',header)
        self.assertIn('QVector<TopoDS_Shape>* perNodeOutputs',header)
        self.assertIn('if(skip>nodes.size())',code)
        self.assertIn('outputs.insert(nodes[i].id,saved)',code)
        self.assertIn('for(int i=skip;i<nodes.size();++i)',code)
        self.assertIn('if(perNodeOutputs)*perNodeOutputs=std::move(intermediate)',code)
        self.assertIn('shapeFeatureCachedOutputs.size()==part.shapeFeatureNodes.size()',document)
        self.assertIn('prefix.append(part.shapeFeatureCachedOutputs[i])',document)
        self.assertIn('if(!propagateLinkedBodies(&candidate,error))return false;',document)
        self.assertIn('checkpoint();parts_=std::move(candidate);',document)
        self.assertIn('loaded.shapeFeatureCachedOutputs=std::move(intermediate)',document)
    def test_real_geometry_operations_not_menu_placeholders(self):
        code=src('src/core/ShapeFeatureTree.cpp')
        ui=src('src/ui/MainWindow.cpp')
        for operation in ('Rotate arbitrary axis','Mirror arbitrary plane',
                          'Linear pattern vector','Circular pattern arbitrary axis',
                          'Intersect branches'):
            self.assertIn(operation,code)
            self.assertIn(operation,ui)
        self.assertIn('BRepAlgoAPI_Common common(input,tool)',code)
        self.assertIn('position.SetTranslation(gp_Vec(copy*v("dx")',code)
        self.assertIn('position.SetRotation(gp_Ax1(center,axis)',code)
        self.assertIn('reflection.SetMirror(gp_Ax2(origin,normal))',code)
        self.assertIn('lengthSquared("dx","dy","dz")<1e-12',code)
        self.assertIn('lengthSquared("axis_x","axis_y","axis_z")<1e-12',code)
    def test_serialization_unchanged_and_cache_not_saved(self):
        doc=src('src/core/SceneDocument.cpp')
        header=src('src/core/SceneDocument.h')
        self.assertIn('shapeFeatureCachedOutputs;',header)
        self.assertIn('root.insert("schema",kSchema24)',doc)
        self.assertNotIn('shape_feature_cache',doc)
        self.assertIn('target.shapeFeatureNodes=nodes',doc)
        self.assertIn('if(!rebuildShapeFeatureTree(base,surfaceBody,nodes,&rebuilt,error,',doc)
if __name__=='__main__': unittest.main(verbosity=2)

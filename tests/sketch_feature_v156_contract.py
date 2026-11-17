"""Static integration checks. They do not compile C++, run CTest or test mouse behavior."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
def source(rel):return (ROOT/rel).read_text(encoding='utf-8')

class SketchFeatureV156Contract(unittest.TestCase):
    def test_sketch_canvas_is_backed_by_persisted_solver(self):
        ui=source('src/ui/SketchCanvasDialog.cpp')
        window=source('src/ui/MainWindow.cpp')
        for text in ('SketchSolver::solve','SketchModel','addConstrainedPolygonExtrusion','Zoom',):
            if text=='Zoom':continue
            self.assertIn(text,ui)
        self.assertIn('document_.addConstrainedPolygonExtrusion(dialog.sketch()',window)
        self.assertIn('document_.updateConstrainedPolygonExtrusion(ids.first()',window)
        for command in ('closeBoundary','undoEdit','setGridSnap','setPointSnap','zoomToFit'):
            self.assertIn(command,ui)

    def test_general_tree_is_geometry_type_independent(self):
        shape=source('src/core/ShapeFeatureTree.cpp')
        for t in ('Translate','Rotate X','Rotate Y','Rotate Z','Uniform scale',
                  'Fillet all edges','Chamfer all edges','Cylinder cut Z'):
            self.assertIn(t,shape)
        self.assertIn('validateShapeFeatureNodes(nodes,error)',shape)
        self.assertIn('BRepCheck_Analyzer',shape)
        self.assertNotIn('Primitive::',shape)

    def test_editing_is_transactional_and_history_aware(self):
        doc=source('src/core/SceneDocument.cpp')
        start=doc.index('bool SceneDocument::setShapeFeatureTree(')
        end=doc.index('bool SceneDocument::rebaseShapeFeatureTree(',start)
        fragment=doc[start:end]
        self.assertLess(fragment.index('rebuildShapeFeatureTree('),fragment.index('checkpoint();'))
        self.assertIn('shapeFeatureTreeCurrent(partId)',fragment)
        self.assertIn('generatedId()',fragment)
        self.assertIn('FEATURE_TREE_STALE',source('src/core/DocumentAudit.cpp'))

    def test_native_persistence_and_regression_hooks(self):
        doc=source('src/core/SceneDocument.cpp')
        self.assertIn('root.insert("schema",kSchema24)',doc)
        self.assertIn('kSchema23',doc)
        self.assertIn('schema!=kSchema23&&schema!=kSchema24',doc)
        self.assertIn('item.insert("shape_feature_tree",tree)',doc)
        self.assertIn('tree.value("base_brep")',doc)
        cmake=source('CMakeLists.txt')
        for file in ('src/core/ShapeFeatureTree.cpp','src/ui/SketchCanvasDialog.cpp',
                     'tests/shape_feature_tree_tests.cpp','cadcore_universal_feature_tree'):
            self.assertIn(file,cmake)
        test=source('tests/shape_feature_tree_tests.cpp')
        for phrase in ('cad::Primitive::Box','cad::Primitive::Sphere','cad::Primitive::Cylinder',
                       'cad::Primitive::Cone','doc.undo()','doc.redo()','doc.saveAs(',
                       'loaded.load(','!loaded.load(','shapeFeatureTreeCurrent('):
            self.assertIn(phrase,test)

if __name__=='__main__':unittest.main(verbosity=2)

from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
def read(p):return (ROOT/p).read_text(encoding='utf-8')

class ExternalSketchLinkPreflight(unittest.TestCase):
    def test_unary_dependency_graph_and_cycle_protection(self):
        graph=read('src/core/LinkedBodyGraph.cpp')
        self.assertIn('node.unary',graph)
        self.assertIn('if(input.empty()||!uniqueInputs.insert(input).second)',graph)
        self.assertIn('allInputs.insert(allInputs.end(),node.additionalSources.begin()',graph)
        self.assertIn('Linked-body dependency graph contains a cycle.',graph)
    def test_geometry_rebuilt_from_saved_sketch_not_source_solid(self):
        source=read('src/core/SceneDocument.cpp')
        block=source.split('bool rebuildLinkedSketchSolid(',1)[1].split('bool SceneDocument::addLinkedSketchExtrusion',1)[0]
        self.assertIn('source.constrainedPolygonSketch',block)
        self.assertIn('source.constrainedPolygonBoundary',block)
        self.assertIn('source.constrainedPolygonGeometryToken!=source.geometryToken',block)
        self.assertIn('(plane.normalZ/normalLength)*offset',block)
    def test_source_sketch_and_boolean_descendants_rebuilt_transactionally(self):
        source=read('src/core/SceneDocument.cpp')
        block=source.split('bool SceneDocument::propagateLinkedBodies(',1)[1].split('bool SceneDocument::linkedBooleanCurrent(',1)[0]
        self.assertLess(block.index('if(linked.hasLinkedSketchExtrusion)'),block.index('if(!linked.hasLinkedBoolean)continue;'))
        update=source.split('bool SceneDocument::updateConstrainedPolygonExtrusion(',1)[1].split('bool SceneDocument::addParametricRectangularExtrusionAt',1)[0]
        self.assertLess(update.index('propagateLinkedBodies(&candidate,error)'),update.index('checkpoint()'))
    def test_saved_links_and_invalid_geometry_rejected(self):
        source=read('src/core/SceneDocument.cpp')
        self.assertIn('item.insert("linked_sketch_extrusion",link);',source)
        self.assertIn('if(item.contains("linked_sketch_extrusion"))',source)
        self.assertIn('Saved linked sketch solid differs from its source sketch.',source)
        self.assertIn('Saved linked sketch placement differs from its source.',source)
    def test_delete_and_direct_edit_restrictions(self):
        source=read('src/core/SceneDocument.cpp')
        self.assertIn('if(part.hasLinkedSketchExtrusion&&!requested.contains(part.id)',source)
        self.assertIn('if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature)',source)
    def test_ui_create_edit_and_inspector(self):
        ui=read('src/ui/MainWindow.cpp')
        for text in ['MainWindow::createLinkedSketchExtrusion','MainWindow::editLinkedSketchExtrusion',
                     'document_.linkedSketchExtrusionCurrent','External sketch from %1',
                     'Associative extrusion from selected body']:
            self.assertIn(text,ui)
    def test_native_tests_and_release_gate(self):
        cm=read('CMakeLists.txt')
        names=re.findall(r'add_test\s*\(\s*NAME\s+([^\s\)]+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        self.assertIn('cadcore_v170_linked_sketch_chain',names)
        self.assertIn('cadcore_v170_linked_sketch_persistence',names)
        self.assertIn('[int]$ExpectedTests = 130',read('scripts/verify_release_windows.ps1'))
    def test_native_regression_checks_real_dependency_chain(self):
        test=read('tests/linked_sketch_extrusion_v170_tests.cpp')
        for snippet in ['updateConstrainedPolygonExtrusion','linkedBodyImpact','updateLinkedSketchExtrusion',
                        'Boolean grandchild did not rebuild','Inconsistent saved linked extrusion geometry accepted']:
            self.assertIn(snippet,test)

if __name__=='__main__':unittest.main(verbosity=2)

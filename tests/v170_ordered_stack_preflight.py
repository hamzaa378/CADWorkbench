"""Source-level contract for v1.70 eight-stage linked sketch increment.
Native Qt/OpenCascade compilation is still required on Windows.
"""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
def src(path):return (ROOT/path).read_text(encoding='utf-8')

class OrderedStackSourceContracts(unittest.TestCase):
    def test_bounded_source_and_editor_api(self):
        core=src('src/core/SceneDocument.cpp')
        header=src('src/core/SceneDocument.h')
        for key in ('kMaxInPlaceSketchStages = 8','appendInPlaceSketchStage(',
                    'updateInPlaceSketchStage(', 'removeLastInPlaceSketchStage('):
            self.assertIn(key,core)
            if key!='kMaxInPlaceSketchStages = 8':self.assertIn(key,header)
        self.assertIn('QVector<AdditionalInPlaceSketchStage> extraInPlaceSketchStages',header)

    def test_dependency_graph_plans_every_stage(self):
        graph=src('src/core/LinkedBodyGraph.cpp')
        core=src('src/core/SceneDocument.cpp')
        self.assertIn('allInputs.insert(allInputs.end(),node.additionalSources.begin()',graph)
        self.assertIn('std::any_of(node.additionalSources.begin(),node.additionalSources.end()',graph)
        self.assertEqual(core.count('node.additionalSources.push_back(stage.sourceId.toStdString());'),1)
        for call in ('linkedBodyGraphNodes(parts)', 'linkedBodyGraphNodes(parts_)',
                     'linkedBodyGraphNodes(staged)'):
            self.assertIn(call,core)
        self.assertIn('Linked-body sources must be nonempty and distinct.',graph)
        self.assertIn('Linked-body dependency graph contains a cycle.',graph)

    def test_atomic_update_and_removal(self):
        core=src('src/core/SceneDocument.cpp')
        for op in ('appendInPlaceSketchStage','updateInPlaceSketchStage','removeLastInPlaceSketchStage'):
            body=core.split('bool SceneDocument::'+op+'(',1)[1].split('\nbool SceneDocument::',1)[0]
            self.assertLess(body.index('propagateLinkedBodies(&candidate,error)'),body.index('checkpoint()'))
        self.assertIn('Remove additional stages in reverse order',core)
        self.assertIn('part.extraInPlaceSketchStages.clear();',core)

    def test_save_load_full_shape_validation(self):
        core=src('src/core/SceneDocument.cpp')
        for key in ('link.insert("extra_stages",extra);','if(link.contains("extra_stages"))',
                    'record.size()!=5','stage.sourceToken!=extraSource.geometryToken',
                    'Saved ordered sketch stage is stale.','expected=following;',
                    'int(link.contains("extra_stages"))+int(link.contains("integrity_sha256"));'):
            self.assertIn(key,core)

    def test_ui_paths(self):
        ui=src('src/ui/MainWindow.cpp')
        for key in ('Append next modifier (stages 3-8)','Edit stage 3-8','Remove last stage 3-8',
                    'document_.appendInPlaceSketchStage(', 'document_.updateInPlaceSketchStage(',
                    'document_.removeLastInPlaceSketchStage(', 'additionalIndex->setRange(3,8)'):
            self.assertIn(key,ui)

    def test_native_registration_and_regressions(self):
        cm=src('CMakeLists.txt')
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        for suffix in ('ordered_stack','ordered_stack_persistence','ordered_stack_corruption','ordered_stack_rollback'):
            self.assertIn('cadcore_v170_inplace_'+suffix,names)
        self.assertIn('[int]$ExpectedTests = 130',src('scripts/verify_release_windows.ps1'))
        reg=src('tests/inplace_sketch_v170_tests.cpp')
        for token in ('orderedStack(false,false,false)','orderedStack(true,false,false)',
                      'orderedStack(true,true,false)','orderedStack(false,false,true)',
                      'reopened.updateConstrainedPolygonExtrusion(third','!f.doc.erase(third'):
            self.assertIn(token,reg)

if __name__=='__main__':unittest.main()

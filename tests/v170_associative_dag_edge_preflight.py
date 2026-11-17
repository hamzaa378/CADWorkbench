"""Static source contracts. This is not a substitute for a Windows native build."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
def source(relative):return (ROOT/relative).read_text(encoding='utf-8')

class AssociativeDagAndEdgeContracts(unittest.TestCase):
    def test_mixed_dag_and_transaction(self):
        header=source('src/core/SceneDocument.h')
        cpp=source('src/core/SceneDocument.cpp')
        replay=cpp.split('bool replayInterleavedHistory(',1)[1].split('// Apply a persisted',1)[0]
        self.assertIn('QStringList inputs; // Earlier step IDs',header)
        for name in ('QMap<QString,TopoDS_Shape> evaluated','evaluated.insert(QStringLiteral("@base"),previous)',
                     'const TopoDS_Shape input=step.inputs.isEmpty()?previous:evaluated.value(step.inputs.first())',
                     'evaluated.value(step.inputs[1])','evaluated.insert(step.id,next)',
                     'distinct.contains(inputId)','!evaluated.contains(inputId)',
                     'linkedSketchFuseEffective(input,next,error)',
                     'linkedSketchCutEffective(input,next,error)'):
            self.assertIn(name,replay)
        change=cpp.split('bool SceneDocument::setInterleavedFeatureHistory(',1)[1].split('bool SceneDocument::interleavedFeatureHistoryCurrent(',1)[0]
        self.assertLess(change.index('if(!propagateLinkedBodies(&candidate,error))return false;'),
                        change.index('checkpoint();parts_=std::move(candidate);'))

    def test_persisted_graph_roundtrip_and_fail_closed_load(self):
        cpp=source('src/core/SceneDocument.cpp')
        self.assertIn('dag-interleaved-history-v2',cpp)
        self.assertIn('linear-interleaved-history-v1',cpp)
        self.assertIn('itemStep.insert("inputs",links)',cpp)
        for key in ('stepData.value("inputs").isArray()',
                    'setError(error,"History references a duplicate or forward dependency.")',
                    'step.inputs.size()!=2', 'saved.interleavedSteps.append(std::move(step))'):
            self.assertIn(key,cpp)
        tests=source('tests/interleaved_history_v170_tests.cpp')
        for text in ('fork/join history built','source edit changes combined branch result',
                     'forward edit rolled back','save branching graph',
                     'reopen branching history','failed graph load preserved document'):
            self.assertIn(text,tests)

    def test_witness_edge_validation_and_legacy(self):
        shape=source('src/core/ShapeFeatureTree.cpp')
        for text in ('const QStringList kEdgeWitnessFields=',
                     'BRep_Tool::Curve(edge,first,last)',
                     'GeomAPI_ProjectPointOnCurve projected(',
                     'if(matches>1)',
                     'Edge witness is missing or lies on a split boundary',
                     'hasBody?2:1',
                     'captured.insert("edge_anchor_version",spline?3.0:2.0)',
                     'captured.insert("edge_curvature",',
                     'for(const QString& key:kEdgeWitnessFields)anchor.insert(key,op.parameters.value(key))',
                     'double best=1e100,runnerUp=1e100;'):
            self.assertIn(text,shape)
        tests=source('tests/face_lineage_v170_tests.cpp')
        for text in ('line edge split should preserve unique interior witness',
                     'split exactly at witness should require reselect',
                     'circular successor keeps interior witness',
                     'legacy seven-field anchor resolution'):
            self.assertIn(text,tests)

    def test_compact_user_authoring_and_rollback(self):
        gui=source('src/ui/MainWindow.cpp')
        header=source('src/ui/MainWindow.h')
        self.assertIn('void editInterleavedFeatureHistory();',header)
        self.assertIn('Mixed associative feature history (branches + sketches)',gui)
        block=gui.split('void MainWindow::editInterleavedFeatureHistory(){',1)[1].split('void MainWindow::editGeneralFeatureTree(){',1)[0]
        for phrase in ('Constrained sketch source','Frozen base','First parent',
                       'Second parent (Boolean only)','Fuse branches',
                       'Subtract branches','SketchFuse','SketchCut',
                       'document_.setInterleavedFeatureHistory(target,steps,&issue)',
                       'The previous solid is unchanged','refresh();statusBar()->showMessage('):
            self.assertIn(phrase,block)
        self.assertLess(block.index('document_.setInterleavedFeatureHistory(target,steps,&issue)'),
                        block.index('dialog.accept();'))

    def test_preserves_release_gate(self):
        cm=source('CMakeLists.txt')
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        self.assertIn('ExpectedTests = 130',source('scripts/verify_release_windows.ps1'))

if __name__=='__main__':unittest.main()

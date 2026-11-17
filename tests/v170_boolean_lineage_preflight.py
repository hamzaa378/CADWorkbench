"""Source and validation contracts; not a native C++ compile/test."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
def source(path):return (ROOT/path).read_text(encoding='utf8')

class BooleanLineageSourceContracts(unittest.TestCase):
    def test_built_kernel_history_scoped_to_transaction(self):
        data=source('src/core/SceneDocument.cpp')
        block=data.split('bool replayInterleavedHistory(',1)[1].split('// Apply a persisted',1)[0]
        self.assertIn('std::shared_ptr<BRepAlgoAPI_BooleanOperation>',block)
        self.assertIn('booleanHistory.insert(step.id',block)
        self.assertIn('rebaseFeatureThroughBoolean(',block)
        self.assertIn('step.referenceSourceId',block)
        self.assertIn('rebuildShapeFeatureTree(input,false,{executable}',block)
        self.assertLess(block.index('rebaseFeatureThroughBoolean('),
                        block.index('rebuildShapeFeatureTree(input,false,{executable}'))

    def test_lineage_filters_to_kernel_output_and_fails_closed(self):
        data=source('src/core/BooleanReferenceLineage.cpp')
        for phrase in ('boolean.Modified(original)','boolean.Generated(original)',
                       'collect(original);','available.FindKey(i).IsSame(candidate)',
                       'candidates.isEmpty()','candidates.size()>1',
                       'captureStableEdgeAnchor(result,chosen',
                       'capturePlanarFaceAnchor(result,explorerIndex',
                       'validateShapeFeatureNodes({changed}',
                       'Boolean operation removed the referenced edge',
                       'Boolean operation removed the referenced face'):
            self.assertIn(phrase,data)
        self.assertNotIn('if(!boolean.IsDeleted(original))collect(original);',data)
        self.assertIn('src/core/BooleanReferenceLineage.cpp',source('CMakeLists.txt'))

    def test_versioned_persistence_rejects_corrupted_records(self):
        data=source('src/core/SceneDocument.cpp')
        for phrase in ('dag-interleaved-history-v3','"reference_source"',
                       'if(withReference)step.referenceSourceId=',
                       'if(!replayInterleavedHistory(child',
                       'topologyHistory||historyType==',
                       'withReference&&!topologyHistory'):
            if phrase == 'withReference&&!topologyHistory':
                self.assertIn('!topologyHistory||!withInputs',data)
            else:self.assertIn(phrase,data)
        tests=source('tests/interleaved_history_v170_tests.cpp')
        for phrase in ('reopened topology link regenerates',
                       'tampered branch provenance rejected on load',
                       'failed retarget preserved last committed feature result'):
            self.assertIn(phrase,tests)

    def test_ui_previews_intermediate_edges_and_planar_faces(self):
        ui=source('src/ui/MainWindow.cpp').split('void MainWindow::editInterleavedFeatureHistory(){',1)[1].split('void MainWindow::editGeneralFeatureTree(){',1)[0]
        for phrase in ('"Fillet selected edge"','"Chamfer selected edge"',
                       '"Face normal pocket"','"Face normal boss"',
                       'Frozen-base edge / intermediate edge',
                       'previewInterleavedFeatureHistory(target',
                       'captureStableEdgeAnchor(referenceGeometry',
                       'capturePlanarFaceAnchor(referenceGeometry',
                       'next.referenceSourceId=sourceId',
                       'Referenced feature cannot be removed',
                       'Invalid feature order'):
            self.assertIn(phrase,ui)

    def test_multiple_boolean_steps_enumerate_kernel_ancestry(self):
        engine=source('src/core/SceneDocument.cpp')
        planner=source('src/core/BooleanLineagePath.h')
        self.assertIn('lineage::allTrackedPaths(ancestry',engine)
        self.assertIn('for(const std::string& stage:chain)',engine)
        self.assertIn('candidate=std::move(rebound)',engine)
        self.assertIn('booleanSources.insert(step.id,{parent})',engine)
        self.assertIn('bool SceneDocument::previewInterleavedFeatureHistory(',engine)
        self.assertIn('if(stageOutputs)*stageOutputs=std::move(evaluated)',engine)
        self.assertIn('multiple branches',planner)
        self.assertIn('depth>40',planner)
        self.assertIn('boolean_lineage_path_v170_tests.cpp',source('docs/V170_THREE_NEXT_IMPROVEMENTS_20261106.md'))

if __name__=='__main__':unittest.main()

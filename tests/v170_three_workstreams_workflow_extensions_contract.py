from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
CORE=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
HEADER=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')

class ThreeWorkstreamsWorkflowExtensions(unittest.TestCase):
    def test_parametric_delete_is_staged_after_kernel_preflight(self):
        region=UI[UI.index('connect(erase,&QPushButton::clicked'):
                  UI.index('const auto reorder=',UI.index('connect(erase,&QPushButton::clicked'))]
        self.assertIn('auto proposed=steps;',region)
        self.assertIn('preflightHistoryStructure(proposed,"Remove feature step")',region)
        self.assertLess(region.index('preflightHistoryStructure'),region.index('steps=std::move(proposed)'))

    def test_parametric_reorder_is_staged_after_kernel_preflight(self):
        region=UI[UI.index('const auto reorder='):UI.index('connect(moveUp,&QPushButton::clicked')]
        self.assertIn('preflightHistoryStructure(proposed,"Reorder feature steps")',region)
        self.assertLess(region.index('preflightHistoryStructure'),region.index('steps=std::move(proposed)'))
        guard=UI[UI.index('const auto preflightHistoryStructure='):UI.index('connect(add,&QPushButton::clicked')]
        self.assertIn('previewInterleavedFeatureHistory(target,proposed',guard)
        self.assertIn('BRepCheck_Analyzer(intermediate).IsValid()',guard)
        self.assertIn('std::isfinite(properties.Mass())',guard)

    def test_articulated_timing_is_exposed_and_validated(self):
        self.assertIn('trajectoryTiming->addItems',UI)
        self.assertIn('trajectoryTiming->currentIndex()==1',UI)
        self.assertIn('bool easeInOut=false',HEADER)
        region=CORE[CORE.index('bool SceneDocument::previewLiveArticulatedTrajectory('):
                    CORE.index('bool SceneDocument::updateLiveArticulatedCoordinates(')]
        self.assertIn('joint_motion::interpolate(drivePlan,fraction',region)
        self.assertIn('const bool explicitTravel=',region)
        self.assertIn('Progression::Smoothstep',region)
        self.assertIn('previewLiveArticulatedCoordinates(frame',region)
        self.assertLess(region.index('joint_motion::interpolate'),region.index('previewLiveArticulatedCoordinates(frame'))

    def test_linked_development_bend_schedule_uses_saved_values(self):
        region=UI[UI.index('void MainWindow::exportSelectedAssociativeFlatPattern()'):
                  UI.index('void MainWindow::exportSelectedFoldedSheetFlatPatternDxf()')]
        self.assertIn('buildLinkedFlatDevelopmentSet(flat->id',region)
        bundle=CORE[CORE.index('bool SceneDocument::buildLinkedFlatDevelopmentSet('):
                    CORE.index('bool SceneDocument::editFoldedSheetMetal(')]
        self.assertIn('buildFoldedSheetBendTableCsv(folded->foldedSheetFlangesMm',bundle)
        self.assertIn('folded->foldedSheetBends,factors,&stagedCsv',bundle)
        self.assertIn('CSV bend schedule',region)
        self.assertIn('saveDxf?dxf:bendCsv',region)
        self.assertIn('output.commit()',region)

if __name__=='__main__':
    unittest.main()

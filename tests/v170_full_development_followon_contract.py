"""Source integration guards for new parametric, articulated and sheet-metal flows."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf8')
SCENE=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf8')
SHEET=(ROOT/'src/core/SheetMetal.cpp').read_text(encoding='utf8')

class FullDevelopmentFollowOn(unittest.TestCase):
    def test_batch_validates_every_stage_before_mutation(self):
        body=UI.split('connect(batchParameters,&QPushButton::clicked',1)[1].split('connect(add,&QPushButton::clicked',1)[0]
        self.assertIn('for(int i=0;i<proposed.size();++i)',body)
        self.assertIn('BRepCheck_Analyzer(intermediate).IsValid()',body)
        self.assertIn('BRepGProp::VolumeProperties(intermediate,volume)',body)
        self.assertLess(body.index('Review complete parametric history rebuild'),body.index('steps=std::move(proposed)'))
        self.assertIn('QMessageBox::No)!=QMessageBox::Yes)return;',body)
    def test_refined_trajectory_bounded_and_readonly(self):
        body=SCENE.split('bool SceneDocument::previewLiveArticulatedTrajectory(',1)[1].split('bool SceneDocument::updateLiveArticulatedCoordinates',1)[0]
        for marker in ('refinementsPerInterval<1','refinementsPerInterval>4',
                       'sampleCount*refinementsPerInterval','i<=totalIntervals','rejectUnsafeSamples&&unsafeFrame'):
            self.assertIn(marker,body)
        self.assertIn('form->addRow("Sub-interval sampling",refinement)',UI)
        self.assertIn('refinement->value(),trajectoryTiming->currentIndex()==1))',UI)
        native=(ROOT/'tests/articulated_assembly_scene_v170_tests.cpp').read_text(encoding="utf-8")
        self.assertIn('refined 12-interval articulated motion preview',native)
    def test_linked_development_all_critical_dimensions_compared(self):
        body=SHEET.split('bool equivalentFoldedSheetDevelopment(',1)[1].split('bool buildFoldedSheetFlatPatternSvg(',1)[0]
        for marker in ('first.flatLengthMm','first.widthMm','first.bendLinePositionsMm',
                       'first.bendAllowancesMm','first.bendDeductionsMm','std::isfinite'):
            self.assertIn(marker,body)
        self.assertIn('buildLinkedFlatDevelopmentSet(flat->id',UI)
        core=SCENE.split('bool SceneDocument::buildLinkedFlatDevelopmentSet(',1)[1].split('bool SceneDocument::editFoldedSheetMetal(',1)[0]
        self.assertIn('equivalentFoldedSheetDevelopment(visual,manufacturing,&issue)',core)
        self.assertLess(core.index('equivalentFoldedSheetDevelopment'),core.index('*svg=std::move(stagedSvg)'))
        native=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding="utf-8")
        self.assertIn('Changed bend allowance escaped',native)
        self.assertIn('Different bend deduction inventories',native)

if __name__=='__main__':unittest.main()

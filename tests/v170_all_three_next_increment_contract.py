from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
SCENE=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
HEADER=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')

class AllThreeWorkstreamsNextIncrement(unittest.TestCase):
    def test_batch_multi_feature_edit(self):
        start=UI.index('connect(batchParameters,&QPushButton::clicked')
        end=UI.index('connect(add,&QPushButton::clicked',start)
        region=UI[start:end]
        for required in ('QTableWidget(0,4','cellKeys','topologyKey(it.key())',
                        'std::isfinite(value)','previewInterleavedFeatureHistory',
                        'BRepCheck_Analyzer(shape).IsValid()', 'steps=std::move(proposed)'):
            self.assertIn(required,region)
        self.assertLess(region.index('previewInterleavedFeatureHistory'),region.index('steps=std::move(proposed)'))
        self.assertIn('key.startsWith("edge_")',UI)
        self.assertIn('key.startsWith("face_")',UI)

    def test_strict_assembly_motion_policy(self):
        self.assertIn('bool* hasUnsafeNominalPairs=nullptr',HEADER)
        self.assertIn('bool rejectUnsafeSamples=false',HEADER)
        p=SCENE[SCENE.index('bool SceneDocument::previewLiveArticulatedCoordinates('):
                SCENE.index('bool SceneDocument::updateLiveArticulatedCoordinates(')]
        for required in ('unsafeNominalPairs=true','unsafeNominalPairs=ids.size()>12',
                        '*hasUnsafeNominalPairs=unsafeNominalPairs',
                        'rejectUnsafeSamples&&unsafeFrame'):
            self.assertIn(required,p)
        ui=UI[UI.index('void MainWindow::editSavedArticulatedDrives()'):UI.index('void MainWindow::auditLiveRigidMateStructure()')]
        self.assertIn('rejectUnsafe->setChecked(true)',ui)
        self.assertIn('rejectUnsafe->isChecked()',ui)
        self.assertLess(ui.index('previewLiveArticulatedCoordinates(targets'),
                        ui.index('commitLiveArticulatedCoordinatesChecked(targets'))

    def test_associative_development_export(self):
        p=UI[UI.index('void MainWindow::exportSelectedAssociativeFlatPattern()'):
             UI.index('void MainWindow::exportSelectedFoldedSheetFlatPatternDxf()')]
        for required in ('linkedFoldedFlatSourceId','linkedFoldedFlatSourceToken',
                         'buildLinkedFlatDevelopmentSet(flat->id',
                         'QSvgRenderer renderer(svg)','QSaveFile output(path)','output.commit()'):
            self.assertIn(required,p)
        # All three are now staged and checked in the core, not assembled
        # independently in the UI after a user's approval.
        generator=SCENE[SCENE.index('bool SceneDocument::buildLinkedFlatDevelopmentSet('):
                        SCENE.index('bool SceneDocument::editFoldedSheetMetal(')]
        for required in ('buildFoldedSheetFlatPatternSvg',
                         'buildFoldedSheetFlatPatternDxf',
                         'buildFoldedSheetBendTableCsv',
                         'equivalentFoldedSheetDevelopment',
                         '*svg=std::move(stagedSvg)'):
            self.assertIn(required,generator)
        self.assertNotIn('checkpoint()',p)
        self.assertIn('&MainWindow::exportSelectedAssociativeFlatPattern',UI)
        self.assertIn('void exportSelectedAssociativeFlatPattern();',
                      (ROOT/'src/ui/MainWindow.h').read_text(encoding='utf-8'))

if __name__=='__main__':
    unittest.main()

import pathlib,unittest
R=pathlib.Path(__file__).resolve().parents[1]
def read(p):return (R/p).read_text(encoding='utf-8')
class PreviewIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.core=read('src/core/SceneDocument.cpp')
        cls.ui=read('src/ui/MainWindow.cpp')
        cls.head=read('src/core/SceneDocument.h')
        cls.mate=read('tests/assembly_mate_document_v170_tests.cpp')
        cls.sheet=read('tests/cadcore_tests.cpp')
    def test_predicted_brep_validation(self):
        p=self.core.split('bool SceneDocument::previewLiveRigidMateConnectionEdit(',1)[1].split('bool SceneDocument::replaceLiveRigidMateConnection(',1)[0]
        for k in ['SceneDocument staged;', 'staged.parts_=parts_;', 'BRepBuilderAPI_Transform(', 'validShape(moved,error)', 'inspectAssemblyFit(staged,ids,requiredClearanceMm,&fitPairs,error)', 'NOMINAL FIT PREVIEW SKIPPED']:
            self.assertIn(k,p)
        self.assertLess(p.index('validShape(moved,error)'),p.index('inspectAssemblyFit('))
        self.assertIn('Nominal overlap may be intentional',p)
        self.assertNotIn('checkpoint();',p)
        self.assertNotIn('parts_=std::move(staged)',p)
    def test_collision_native_regression(self):
        for k in ['NOMINAL FIT PREVIEW: 3 exact pair checks','collisionScene.previewLiveRigidMateConnectionEdit','collisionReport.contains("1 interference pairs")','collisionOldToken']:
            self.assertIn(k,self.mate)
    def test_folded_source_geometry_preview(self):
        p=self.core.split('bool SceneDocument::previewFoldedSheetMetalEdit(',1)[1].split('bool SceneDocument::editFoldedSheetMetal(',1)[0]
        for k in ['hasFoldedSheetMetalDefinition','foldedSheetGeometryToken','makeFoldedSheetSolid(','validShape(candidate,error)','BRepGProp::VolumeProperties','calculateFoldedSheetFlatPattern','*solidVolumeMm3=volume','*neutralFlatLengthMm=flat.flatLengthMm']:
            self.assertIn(k,p)
        self.assertLess(p.index('calculateFoldedSheetFlatPattern'),p.index('*solidVolumeMm3=volume'))
        self.assertNotIn('checkpoint();',p)
        self.assertIn('previewFoldedSheetMetalEdit(',self.head)
        self.assertIn('!stale.previewFoldedSheetMetalEdit',self.sheet)
    def test_edit_flow_uses_preview_before_commit(self):
        p=self.ui.split('void MainWindow::editFoldedSheetMetal()',1)[1].split('bool MainWindow::saveFile(',1)[0]
        self.assertLess(p.index('previewFoldedSheetMetalEdit('),p.index('editFoldedSheetMetalWithDevelopments(ids.first()'))
        self.assertIn('Review folded-body change',p)
    def test_svg_issuance_gate(self):
        p=self.ui.split('void MainWindow::refreshSavedDrawingSource()',1)[1].split('void MainWindow::exportDrawingSvg()',1)[0]
        for k in ['buildOrthographicDrawingSvg(composite,&stagedSvg','QSvgRenderer drawingRenderer(stagedSvg)','if(review.exec()!=QDialog::Accepted)return;','exportOrthographicDrawingSvg(composite,path','setDrawingSheetSettings(masterId,prepared']:
            self.assertIn(k,p)
        self.assertLess(p.index('review.exec()'),p.index('exportOrthographicDrawingSvg(composite,path'))
        self.assertLess(p.index('exportOrthographicDrawingSvg(composite,path'),p.index('setDrawingSheetSettings(masterId,prepared'))
    def test_native_test_inventory(self):
        import re
        names=re.findall(r'add_test\(NAME\s+(\w+)',read('CMakeLists.txt'))
        self.assertEqual(len(names),130)
if __name__=='__main__':unittest.main()

"""Source integration checks; not a substitute for native Qt/OCCT CTests."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
def read(p):return (ROOT/p).read_text(encoding='utf-8')

class DetailedFlatWorkflow(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.core=read('src/core/SceneDocument.cpp')
        cls.header=read('src/core/SceneDocument.h')
        cls.ui=read('src/ui/MainWindow.cpp')
        cls.native=read('tests/cadcore_tests.cpp')

    def test_development_is_exposed_from_ui(self):
        self.assertIn('Edit linked flat blank K-factors and relief',self.ui)
        self.assertIn('void MainWindow::editLinkedFoldedFlatDevelopment()',self.ui)
        self.assertIn('document_.editLinkedFoldedFlatDevelopment(blank->id,requested,relief,&issue)',self.ui)

    def test_same_flat_stations_as_dxf(self):
        section=self.core.split('bool createLinkedFlatBlank(',1)[1].split('bool SceneDocument::addLinkedFoldedFlatBlank',1)[0]
        self.assertIn('buildFoldedSheetFlatPatternDxf(',section)
        self.assertIn('pattern.bendLinePositionsMm',section)
        self.assertIn('BRepAlgoAPI_Cut cut(candidate,tool)',section)
        self.assertIn('BRepCheck_Analyzer(cut.Shape()).IsValid()',section)
        self.assertIn('measured.Mass()-expected',section)
        self.assertLess(section.index('buildFoldedSheetFlatPatternDxf('),section.index('*blank=candidate'))

    def test_derived_parameters_are_persisted(self):
        for marker in ['"developed-flat-blank-v2"','"neutral-flat-blank-v1"','"k_factors"',
                       '"relief_width_mm"','"relief_depth_mm"']:
            self.assertIn(marker,self.core)
        self.assertIn('child.linkedFoldedFlatKFactors,child.linkedFoldedFlatRelief,&expected',self.core)

    def test_development_edits_are_atomic(self):
        section=self.core.split('bool SceneDocument::editLinkedFoldedFlatDevelopment(',1)[1].split('bool SceneDocument::addFoldedSheetMetal(',1)[0]
        self.assertLess(section.index('createLinkedFlatBlank('),section.index('checkpoint();parts_=std::move(staged)'))
        self.assertLess(section.index('propagateLinkedBodies(&staged,error)'),section.index('checkpoint();parts_=std::move(staged)'))
        self.assertIn('linkedFoldedFlatRelief',section)

    def test_regeneration_uses_derived_development_settings(self):
        section=self.core.split('bool SceneDocument::propagateLinkedBodies(',1)[1].split('bool SceneDocument::',1)[0]
        self.assertIn('linked.linkedFoldedFlatKFactors',section)
        self.assertIn('linked.linkedFoldedFlatRelief',section)

    def test_native_rollback_and_persistence_scenarios(self):
        for name in ['overdeep relief modified committed','invalid K-factor modified committed',
                     'relief edit failed atomic undo/redo','source flange edit did not regenerate',
                     'developed blank K-factor/relief lost after save and reopen']:
            self.assertIn(name,self.native)

if __name__=='__main__':unittest.main()

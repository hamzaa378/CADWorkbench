"""Source-level integration guards; not native Windows or OCCT tests."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class LastPretestContract(unittest.TestCase):
    def test_dimension_and_suppression_preview_measures_before_after(self):
        source=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        function=source.split('bool SceneDocument::previewCoordinatedParametricRevision(',1)[1].split('bool SceneDocument::setParametricFeaturesSuppressedBatch(',1)[0]
        for term in ('BRepGProp::VolumeProperties(before.shape,previous)',
                     'BRepGProp::VolumeProperties(after->shape,properties)',
                     'topologyChanges','linkedChanges','TOPOLOGY COUNT CHANGED',
                     'No live geometry, revision or undo change'):
            self.assertIn(term,function)
    def test_coupled_drive_ranges_are_in_the_actual_joint_editor(self):
        source=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        function=source.split('void MainWindow::editSavedArticulatedJoint',1)[-1]
        self.assertIn('admissibleRootRanges(driveInputs,relations,',function)
        self.assertIn('coupledTravelReport',function)
        self.assertIn('previewLiveArticulatedCoordinates(targets,',function)
    def test_stock_gauge_policy_checked_before_publish(self):
        source=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        method=source.split('bool SceneDocument::exportLinkedFlatManufacturingBatch(',1)[1].split('bool SceneDocument::editFoldedSheetMetal(',1)[0]
        self.assertIn('stock_material::validate(gaugeItems',method)
        self.assertLess(method.index('stock_material::validate(gaugeItems'),method.index('QTemporaryDir staging('))
        self.assertIn('stock_material_compatibility_checked',method)
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        self.assertIn('stock.requireCompatibleGaugeAndMaterial=true',ui)
    def test_existing_two_native_tests_not_disabled(self):
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding="utf-8")
        self.assertIn('cadcore_folded_sheet_metal',cmake)
        self.assertIn('cadcore_v170_assembly_mate_document',cmake)
        self.assertIn('src/core/StockMaterialCompatibility.h',cmake)
if __name__=='__main__':unittest.main()

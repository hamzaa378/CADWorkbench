from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[1]
def read(name):return (root/name).read_text(encoding="utf-8")
class ThreeIntegratedWorkflows(unittest.TestCase):
    def test_combined_dimension_and_suppression_final_state(self):
        source=read('src/core/SceneDocument.cpp')
        segment=source.split('bool SceneDocument::setCoordinatedParametricRevision(',1)[1].split('bool SceneDocument::previewCoordinatedParametricRevision(',1)[0]
        for field in ('rebuildParametricSolid(part,values,suppressed',
                      'evaluatePersistentDimensionFormulas(',
                      'resolveParameterLinks(', 'propagateLinkedBodies(&candidate',
                      'checkpoint();parts_=std::move(candidate)', 'optionalParametricFeaturePresent('):
            self.assertIn(field,segment)
        self.assertLess(segment.index('propagateLinkedBodies(&candidate'),segment.index('checkpoint();parts_=std::move(candidate)'))
    def test_suppressed_hole_skips_inapplicable_cut(self):
        block=read('src/core/SceneDocument.cpp').split('bool rebuildParametricSolid(',1)[1].split('const bool hasBlindHole',1)[1]
        self.assertIn('if(activeHole){',block)
        self.assertIn('if(!finitePositive(diameter)',block)
    def test_gui_uses_combined_preview_and_commit(self):
        ui=read('src/ui/MainWindow.cpp').split('void MainWindow::editNamedParameterBatch(){',1)[1].split('void MainWindow::editNamedParameter()',1)[0]
        for name in ('QCheckBox', 'previewCoordinatedParametricRevision(', 'setCoordinatedParametricRevision(', 'proposedSuppression'):
            self.assertIn(name,ui)
        self.assertLess(ui.index('previewCoordinatedParametricRevision('),ui.index('setCoordinatedParametricRevision('))
    def test_assembly_coupling_is_staged_before_production_solve(self):
        ui=read('src/ui/MainWindow.cpp').split('void MainWindow::editSavedArticulatedDrives(){',1)[1].split('void MainWindow::auditLiveRigidMateStructure()',1)[0]
        self.assertIn('drive_coupling::solve',ui)
        self.assertIn('previewLiveArticulatedCoordinates(',ui)
        self.assertLess(ui.index('drive_coupling::solve'),ui.index('previewLiveArticulatedCoordinates('))
        self.assertIn('joint_drive_coupling',read('scripts/verify_portable_consolidation.sh'))
        self.assertIn('Cyclic drive couplings',read('src/core/JointDriveCoupling.h'))
    def test_mfg_real_contour_and_atomic_publish(self):
        doc=read('src/core/SceneDocument.cpp')
        segment=doc.split('bool SceneDocument::exportLinkedFlatManufacturingBatch(',1)[1].split('bool SceneDocument::editFoldedSheetMetal(',1)[0]
        for name in ('reliefCutouts', 'bendLinePositionsMm', 'contour_nesting::build(',
                     'material_utilization_fraction','relief_aware_contour_placement',
                     'QTemporaryDir staging', 'QDir().rename(staging.path(),newDirectory)'):
            self.assertIn(name,segment)
        self.assertLess(segment.index('contour_nesting::build('),segment.index('QTemporaryDir staging'))
        self.assertIn('stock_contour_nesting',read('scripts/verify_portable_consolidation.sh'))
    def test_native_regression_added_and_not_skipped(self):
        native=read('tests/cadcore_tests.cpp')
        self.assertIn('testCoordinatedDimensionSuppressionV170()',native)
        self.assertIn('relief_aware_contour_placement',native)
        cmake=read('CMakeLists.txt')
        self.assertIn('cadcore_folded_sheet_metal',cmake)
        self.assertIn('cadcore_v170_assembly_mate_document',cmake)

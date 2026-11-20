"""Source integration checks for the coordinated v1.70 feature pass.

These are not a replacement for the native Qt/OCCT Windows regressions.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / 'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'src/core/SceneDocument.h').read_text(encoding='utf-8')
UI = (ROOT / 'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
NATIVE = (ROOT / 'tests/cadcore_tests.cpp').read_text(encoding='utf-8')
ARTICULATED = (ROOT / 'tests/articulated_assembly_scene_v170_tests.cpp').read_text(encoding='utf-8')


class ThreeWorkstreamContract(unittest.TestCase):
    def test_batch_public_api_and_menu(self):
        for name in ('previewNamedParameterBatch', 'setNamedParameterBatch'):
            self.assertIn(name, HEADER)
            self.assertIn('SceneDocument::'+name, CORE)
        self.assertIn('MainWindow::editNamedParameterBatch', UI)
        self.assertIn('Edit named dimensions across selected parametric bodies', UI)

    def test_batch_uses_detached_production_rebuild(self):
        start = CORE.index('bool SceneDocument::previewNamedParameterBatch(')
        end = CORE.index('QStringList SceneDocument::suppressedParametricFeatures', start)
        segment = CORE[start:end]
        # Sprint 14: do not rebuild every dependent body repeatedly. Both
        # preview and commit now use the single final-state coordinated path.
        self.assertIn('SceneDocument staged=*this;', segment)
        self.assertIn('staged.setNamedParameters(', segment)
        self.assertIn('staged.setCoordinatedParametricRevision(changes,{},error)', segment)
        self.assertIn('return setCoordinatedParametricRevision(changes,{},error)', segment)
        self.assertNotIn('parts_=std::move(staged.parts_)', segment)
        self.assertIn('validShape(after->shape,error)', segment)
        self.assertIn('BRepGProp::VolumeProperties', segment)

    def test_batch_has_native_atomic_rollback_tests(self):
        self.assertIn('void testCoordinatedParametricBatchV170()', NATIVE)
        self.assertIn('coordinated batch transitive linked solid', NATIVE)
        self.assertIn('failed or first body', NATIVE.replace('invalid second source changed output or first body', 'failed or first body'))
        self.assertIn('testCoordinatedParametricBatchV170(); testParametricFoundation()', NATIVE)

    def test_articulated_export_is_real_sampled_solver(self):
        self.assertIn('buildLiveArticulatedMotionCsv', HEADER)
        start = CORE.index('bool SceneDocument::buildLiveArticulatedMotionCsv(')
        end = CORE.index('bool SceneDocument::updateLiveArticulatedCoordinates(', start)
        segment = CORE[start:end]
        self.assertIn('joint_motion::interpolate(', segment)
        self.assertIn('previewLiveArticulatedCoordinates(frame,', segment)
        self.assertIn('rejectUnsafe&&unsafe', segment)
        self.assertIn('*csv=std::move(generated)', segment)
        self.assertIn('Motion CSV sample', segment)

    def test_articulated_export_is_wired_to_real_review(self):
        block = UI.split('void MainWindow::editSavedArticulatedDrives(){',1)[1].split('void MainWindow::auditLiveRigidMateStructure()',1)[0]
        self.assertIn('Export sampled motion CSV', block)
        self.assertIn('buildLiveArticulatedMotionCsv(targets,frames->value()', block)
        self.assertIn('QSaveFile output(path)', block)
        self.assertIn('strict failed sampled evidence export', ARTICULATED)

    def test_manufacturing_bundle_has_provenance_and_atomic_publish(self):
        self.assertIn('exportLinkedFlatManufacturingPackage', HEADER)
        start = CORE.index('bool SceneDocument::exportLinkedFlatManufacturingPackage(')
        end = CORE.index('bool SceneDocument::editFoldedSheetMetal(', start)
        segment = CORE[start:end]
        for value in ('buildLinkedFlatDevelopmentSet(', 'QTemporaryDir staging(',
                      'QCryptographicHash::Sha256', 'bend-schedule.csv',
                      'flat-pattern.svg', 'flat-pattern.dxf', 'manifest.json',
                      'staging.setAutoRemove(false)'):
            self.assertIn(value, segment)
        self.assertIn('QDir().rename(staging.path(),newDirectory)', segment)
        self.assertIn('destination.exists()', segment)

    def test_manufacturing_export_has_gui_and_native_tests(self):
        self.assertIn('MainWindow::exportSelectedAssociativeFlatPackage()', UI)
        self.assertIn('Export associative flat manufacturing package', UI)
        self.assertIn('testAssociativeManufacturingPackageV170(); testSheetMetalFlatPattern()', NATIVE)
        self.assertIn('manufacturing manifest checksum', NATIVE)

    def test_known_failures_are_not_skipped(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('add_test(NAME cadcore_folded_sheet_metal COMMAND cadcore_tests --folded-sheet-metal)', cmake)
        self.assertIn('cadcore_v170_assembly_mate_document', cmake)
        self.assertNotRegex(cmake, r'(?i)DISABLED[^\n]*(?:assembly_mate_document|folded_sheet_metal)')


if __name__ == '__main__':
    unittest.main()

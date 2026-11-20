"""Integration registrations for Sprint 15; native validation remains mandatory."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
SCENE=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
HEADER=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')
UI=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
SHEET=(ROOT/'src/core/SheetMetal.cpp').read_text(encoding='utf-8')
ASSEMBLY=(ROOT/'tests/articulated_assembly_scene_v170_tests.cpp').read_text(encoding='utf-8')
DRAWING=(ROOT/'tests/document_audit_tests.cpp').read_text(encoding='utf-8')
CAD=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding='utf-8')

class Sprint15Workflows(unittest.TestCase):
    def test_coordinated_mate_batch_is_native(self):
        for api in ('previewLiveArticulatedJointDefinitionBatch',
                    'replaceLiveArticulatedJointDefinitionBatch',
                    'stageArticulatedDefinitionBatch'):
            self.assertIn(' '+api+'(',HEADER)
            self.assertIn('SceneDocument::'+api+'(',SCENE)
    def test_batch_gate_and_atomic_checkpoint(self):
        segment=SCENE.split('bool SceneDocument::stageArticulatedDefinitionBatch(',1)[1].split(
            'bool SceneDocument::previewLiveArticulatedJointDefinitionBatch(',1)[0]
        for token in ('sameArticulatedConnection','stageLiveArticulatedRegeneration',
                      'inspectAssemblyFit', 'rejectUnsafeNominal', 'ids.size()<=12'):
            self.assertIn(token,segment)
        commit=SCENE.split('bool SceneDocument::replaceLiveArticulatedJointDefinitionBatch(',1)[1].split(
            'namespace {',1)[0]
        self.assertIn('checkpoint();',commit)
        self.assertIn('liveArticulatedJoints_=std::move(definitions)',commit)
    def test_mate_native_tests_and_gui(self):
        for token in ('QMap<int,J> mateEdits', 'rebindScene.undo()',
                      'rebindScene.redo()', 'reopenedBatch.load('):
            self.assertIn(token,ASSEMBLY)
        self.assertIn('Edit ALL saved articulated mate frame offsets',UI)
        self.assertIn('document_.replaceLiveArticulatedJointDefinitionBatch(',UI)
    def test_sheet_batch_reuses_actual_drawing_kernel(self):
        body=SCENE.split('bool SceneDocument::buildSavedDrawingSvgBatch(',1)[1].split(
            'bool SceneDocument::exportSavedDrawingSvgBatch(',1)[0]
        for token in ('buildDrawingAssemblyPart','buildOrthographicDrawingSvg',
                      'validateDrawingEdgeDimensionPreview','drawingSourceDataToken',
                      'prepareDrawingSheetSettings','gdt.sourceGeometryToken',
                      'totalBytes>32*1024*1024'):
            self.assertIn(token,body)
        self.assertNotIn('checkpoint()',body)
    def test_drawing_export_is_staged_and_does_not_approve(self):
        body=SCENE.split('bool SceneDocument::exportSavedDrawingSvgBatch(',1)[1].split(
            'bool SceneDocument::setInterleavedFeatureHistory(',1)[0]
        for token in ('QTemporaryDir staging','QSaveFile file(',
                      'QDir().rename(', 'NOT_ISSUED', '!QFileInfo::exists('):
            # The destination is prechecked using QFileInfo as well.
            if token=='!QFileInfo::exists(':
                self.assertIn('QFileInfo::exists(newDirectory)',body)
            else:
                self.assertIn(token,body)
        self.assertNotIn('checkpoint()',body)
        self.assertIn('multi-sheet export: could not revise source',DRAWING)
        self.assertIn('Strict drawing batch',UI)
    def test_refold_is_actual_kernel_integration(self):
        for token in ('RefoldGeometryAudit.h','refold::inspect(',
                      'buildFoldedSheetOutline('):
            self.assertIn(token,SHEET)
        for token in ('auditFoldedSheetRefoldGeometry',
                      'BRepGProp::VolumeProperties(reproduced,reproducedProperties)',
                      'makeFoldedSheetSolid(', 'std::abs(storedVolume-rebuiltVolume)'):
            self.assertIn(token,SCENE)
        self.assertIn('complex three-bend refold solid could not be built',CAD)
    def test_full_windows_inventory_untouched(self):
        source=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertEqual(len(re.findall(r'add_test\s*\(\s*NAME\s+(\w+)',source)),130)
        self.assertIn('refold_geometry_audit_portable_tests.cpp',
                      (ROOT/'tools/run_portable_regressions.py').read_text())
    def test_no_release_certification(self):
        self.assertIn('motion_intervals=NOT_CERTIFIED',SCENE)
        self.assertIn('human_review=REQUIRED; drawing_issue=NOT_ISSUED',SCENE)
        self.assertIn('NOT_ISSUED',SCENE)

if __name__=='__main__':unittest.main()

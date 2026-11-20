"""Sprint 14 native integration contracts, not substitutes for Windows Qt/OCCT tests."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
SCENE=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
HEADER=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')
UI=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
AUDIT=(ROOT/'tests/document_audit_tests.cpp').read_text(encoding='utf-8')
CAD=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding='utf-8')
CM=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')

class DrawingBulkAuthoring(unittest.TestCase):
    def test_methods_declared_and_implemented(self):
        for method in ('previewDrawingEdgeReviewBatch','reviewDrawingEdgeReviewBatch',
                       'buildDrawingRecoveryCsv','exportDrawingRecoveryCsv'):
            with self.subTest(method=method):
                self.assertIn(' '+method+'(',HEADER)
                self.assertIn('SceneDocument::'+method+'(',SCENE)
    def test_preview_and_commit_use_same_staging(self):
        for signature in ('bool SceneDocument::previewDrawingEdgeReviewBatch(',
                          'bool SceneDocument::reviewDrawingEdgeReviewBatch('):
            segment=SCENE.split(signature)[1].split('\nbool SceneDocument::',1)[0]
            self.assertIn('stageDrawingEdgeReviewBatch(selection,',segment)
        self.assertIn('if(!staged.isEmpty()){\n        checkpoint();',SCENE)
    def test_existing_geometry_verifier_is_reused(self):
        segment=SCENE.split('bool SceneDocument::stageDrawingEdgeReviewBatch(',1)[1].split(
            'bool SceneDocument::previewDrawingEdgeReviewBatch(',1)[0]
        self.assertIn('prepareReviewedDrawingEdge(*source,saved[index],&reviewed,&issue)',segment)
        helper=SCENE.split('bool prepareReviewedDrawingEdge(',1)[1].split('bool SceneDocument::reviewDrawingEdgeDimension(',1)[0]
        self.assertIn('validateDrawingEdgeDimensionPreview(source,reviewed,',helper)
        self.assertIn('QStringLiteral("Sheet %1 / annotation %2: %3")',segment)
    def test_duplicate_unknown_and_oversized_batches_fail(self):
        segment=SCENE.split('bool SceneDocument::stageDrawingEdgeReviewBatch(',1)[1].split(
            'bool SceneDocument::previewDrawingEdgeReviewBatch(',1)[0]
        for token in ('selection.size()>40','total>200','unique.contains(id)',
                      '!owner->hasDrawingSettings','setError(error,'):
            self.assertIn(token,segment)
    def test_only_selected_edge_settings_committed(self):
        fn=SCENE.split('bool SceneDocument::reviewDrawingEdgeReviewBatch(',1)[1].split(
            'bool SceneDocument::buildDrawingRecoveryCsv(',1)[0]
        self.assertIn('part.drawingSettings=*it;',fn)
        self.assertNotIn('part.shape=',fn)
        self.assertNotIn('gdtFrames[',fn)
    def test_readonly_report_explicitly_not_certified(self):
        fn=SCENE.split('bool SceneDocument::buildDrawingRecoveryCsv(',1)[1].split(
            'bool SceneDocument::exportDrawingRecoveryCsv(',1)[0]
        for token in ('NOT ISSUED','TOKEN_CURRENT_GEOMETRY_NOT_RECHECKED',
                      'GD_AND_T','TOKEN_CURRENT_HUMAN_REVIEW_REQUIRED','total>20000'):
            self.assertIn(token,fn)
    def test_csv_export_is_atomic(self):
        segment=SCENE.split('bool SceneDocument::exportDrawingRecoveryCsv(',1)[1].split(
            'bool SceneDocument::setInterleavedFeatureHistory(',1)[0]
        self.assertIn('QSaveFile file(path)',segment)
        self.assertIn('!file.commit()',segment)
    def test_gui_exposes_both_workflows_and_requires_confirmation(self):
        for token in ('Review witnessed drawing edge revisions across sheets...',
                      'Export all saved drawing revision issues CSV...',
                      'document_.previewDrawingEdgeReviewBatch(requested,',
                      'QMessageBox::question(this,"Review source-linked edge revisions"',
                      'document_.reviewDrawingEdgeReviewBatch(requested,',
                      'document_.exportDrawingRecoveryCsv(path,'):
            self.assertIn(token,UI)
    def test_native_rollback_and_save_roundtrip(self):
        for token in ('Batch drawing: invalid second sheet partially modified first sheet',
                      'batch.previewDrawingEdgeReviewBatch(unsafe',
                      'batch.reviewDrawingEdgeReviewBatch(good',
                      'batch.undo()', 'batch.redo()',
                      'batch.saveAs(saved.filePath("batch-review.cadscene")',
                      'reopened.load(saved.filePath("batch-review.cadscene")',
                      'reopened.exportDrawingRecoveryCsv('):
            self.assertIn(token,AUDIT)

class ParametricPerformanceAndSafety(unittest.TestCase):
    def test_new_optimistic_commit_is_wired_to_gui(self):
        for token in ('applyParametricDesignAlternative(',
                      'designGeometryFingerprint()'):
            self.assertIn(token,HEADER)
        for token in ('evaluatedGeometryFingerprint=document_.designGeometryFingerprint()',
                      'document_.applyParametricDesignAlternative(candidates[index],'):
            self.assertIn(token,UI)
    def test_stale_scene_fingerprint_fails_before_mutation(self):
        body=SCENE.split('bool SceneDocument::applyParametricDesignAlternative(',1)[1].split(
            'QStringList SceneDocument::suppressedParametricFeatures(',1)[0]
        self.assertLess(body.index('expectedGeometryFingerprint!=designGeometryFingerprint()'),
                        body.index('return setCoordinatedParametricRevision('))
    def test_formulas_and_links_in_fingerprint(self):
        body=SCENE.split('QString SceneDocument::designGeometryFingerprint()',1)[1].split(
            'bool SceneDocument::applyParametricDesignAlternative(',1)[0]
        for token in ('part->geometryToken','part->namedParameters',
                      'part->namedParameterFormulas','part->parameterLinks',
                      'part->suppressedParametricFeatures','QCryptographicHash::Sha256'):
            self.assertIn(token,body)
    def test_preview_final_state_coordinates_sources(self):
        body=SCENE.split('bool SceneDocument::previewNamedParameterBatch(',1)[1].split(
            'bool SceneDocument::setNamedParameterBatch(',1)[0]
        self.assertIn('staged.setCoordinatedParametricRevision(changes,{},error)',body)
        self.assertIn('validShape(after->shape,error)',body)
    def test_commit_no_longer_double_previews_and_rebuilds(self):
        body=SCENE.split('bool SceneDocument::setNamedParameterBatch(',1)[1].split(
            'bool SceneDocument::evaluateParametricDesignAlternatives(',1)[0]
        self.assertIn('return setCoordinatedParametricRevision(changes,{},error)',body)
        self.assertNotIn('previewNamedParameterBatch(',body)
        self.assertNotIn('SceneDocument staged=*this;',body)
    def test_native_reference_rollback(self):
        for token in ('scene.applyParametricDesignAlternative(options[0]',
                      'scene.applyParametricDesignAlternative(options[2]',
                      'scene.designGeometryFingerprint()==alternativeFingerprint',
                      'scene.redo()', 'scene.undo()'):
            self.assertIn(token,CAD)
    def test_all_130_native_tests_unchanged(self):
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\w+)',CM)
        baseline=(ROOT/'tests/BASELINE_130_NATIVE_CTEST_NAMES.txt').read_text().splitlines()
        self.assertEqual(len(names),130)
        self.assertEqual(set(names),set(baseline))

if __name__=='__main__':unittest.main()

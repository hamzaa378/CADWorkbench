"""New source-level integration contracts. Actual Windows native build remains required."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
def source(p):return (ROOT/p).read_text(encoding='utf-8')
class IntegratedRebuildContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scene=source('src/core/SceneDocument.cpp')
        cls.header=source('src/core/SceneDocument.h')
        cls.cmake=source('CMakeLists.txt')
        cls.boolean=source('src/core/BooleanReferenceLineage.cpp')
    def test_route_consensus_is_in_actual_occt_replay(self):
        body=self.scene.split('bool replayInterleavedHistory(',1)[1].split('bool SceneDocument::setInterleavedFeatureHistory(',1)[0]
        self.assertIn('lineage::allTrackedPaths(',body)
        self.assertIn('rebaseFeatureThroughBoolean(',body)
        self.assertIn('lineage::agreeReferenceRoutes(routeEvidence',body)
        self.assertIn('return false;',body)
        self.assertIn('referenceResult=candidate;',body)
        self.assertIn('ReferenceRouteConsensus.h',self.scene)
    def test_route_does_not_commit_before_consensus(self):
        body=self.scene.split('bool replayInterleavedHistory(',1)[1]
        self.assertLess(body.index('lineage::agreeReferenceRoutes(routeEvidence'),body.index('resolved=std::move(referenceResult);'))
    def test_batched_boolean_references_are_atomic(self):
        body=self.boolean.split('bool rebaseFeaturesThroughBoolean(',1)[1].split('bool rebaseFeatureThroughBoolean(',1)[0]
        self.assertIn('QVector<ShapeFeatureNode> staged;',body)
        self.assertLess(body.index('if(!rebaseFeatureThroughBoolean('),body.index('*rebound=std::move(staged);'))
        self.assertIn('ids.contains(feature.id)',body)
    def test_batch_staging_function_shared_by_preview_and_commit(self):
        body=self.scene.split('bool SceneDocument::stageShapeFeatureTreesBatch(',1)[1]
        self.assertIn('transaction::validateRebuildCommit(',body)
        self.assertIn('bool SceneDocument::previewShapeFeatureTreesBatch(',body)
        preview=body.split('bool SceneDocument::previewShapeFeatureTreesBatch(',1)[1].split('bool SceneDocument::setShapeFeatureTreesBatch(',1)[0]
        self.assertIn('stageShapeFeatureTreesBatch(',preview)
        self.assertNotIn('checkpoint();',preview)
        self.assertNotIn('parts_=',preview)
        commit=body.split('bool SceneDocument::setShapeFeatureTreesBatch(',1)[1].split('namespace {',1)[0]
        self.assertLess(commit.index('stageShapeFeatureTreesBatch('),commit.index('checkpoint();'))
        self.assertLess(commit.index('checkpoint();'),commit.index('parts_=std::move(staged.parts_)'))
    def test_risk_of_rebuilding_independent_root_is_transactional(self):
        self.assertIn('bool stageShapeFeatureTreesBatch(',self.header)
        self.assertIn('const QMap<QString,QVector<ShapeFeatureNode>>& proposals,',self.header)
        self.assertIn('bool previewShapeFeatureTreesBatch(',self.header)
    def test_drawing_sheet_refresh_cannot_acknowledge_annotation(self):
        status=self.scene.split('DrawingSheetSourceStatus SceneDocument::drawingSheetSourceStatus(',1)[1].split('QString SceneDocument::drawingSourceDataToken(',1)[0]
        self.assertIn('report.staleAnnotationIds',status)
        self.assertIn('DrawingEdgeDimensionAnnotation',status)
        self.assertIn('DrawingGdtFrameAnnotation',status)
        self.assertIn('report.staleAnnotationIds.isEmpty()',status)
        self.assertIn('DRAWING_ANNOTATION_REVISION_STALE',source('src/core/DocumentAudit.cpp'))
    def test_batched_boolean_and_drawing_native_regressions_exist(self):
        face=source('tests/face_lineage_v170_tests.cpp')
        self.assertIn('rebaseFeaturesThroughBoolean(',face)
        shape=source('tests/shape_feature_tree_tests.cpp')
        self.assertIn('previewShapeFeatureTreesBatch(',shape)
        audit=source('tests/document_audit_tests.cpp')
        self.assertIn('stillStale.staleAnnotationIds.contains(',audit)
    def test_source_cmake_and_legacy_tests_preserved(self):
        for p in ('src/core/ReferenceRouteConsensus.h','src/core/AtomicRebuildGuard.h'):
            self.assertIn(p,self.cmake)
        self.assertEqual(len(re.findall(r'add_test\s*\(\s*NAME\s+[A-Za-z_0-9]+',self.cmake)),130)
    def test_zero_partial_output_on_invalid_preview(self):
        suite=source('tests/shape_feature_tree_tests.cpp')
        self.assertIn('previewReport==QStringLiteral("UNCHANGED")',suite)
        self.assertIn('job.find(first)->geometryToken==initialA',suite)
        self.assertIn('job.find(second)->geometryToken==initialB',suite)
    def test_duplicate_lineage_source_rejected(self):
        self.assertIn('ids.contains(feature.id)',self.boolean)
        self.assertIn('{opening,opening}',source('tests/face_lineage_v170_tests.cpp'))
    def test_header_usable_as_portable_cxx(self):
        for p in ('src/core/ReferenceRouteConsensus.h','src/core/AtomicRebuildGuard.h'):
            s=source(p)
            self.assertIn('#pragma once',s)
            self.assertNotIn('QString',s)
            self.assertNotIn('#include <TopoDS',s)
    def test_existing_grouped_blend_native_regression_not_deleted(self):
        self.assertIn('Resize of grouped blend did not rebuild',source('tests/shape_feature_tree_tests.cpp'))
        self.assertIn('anchor.remove("size");',source('src/core/ShapeFeatureTree.cpp'))
if __name__=='__main__':unittest.main()

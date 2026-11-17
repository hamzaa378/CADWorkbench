"""Sprint 8 static integration contracts. Complements, never replaces, native Qt/OCCT CTests."""
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SHAPE=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text(encoding='utf-8')
ROUTES=(ROOT/'src/core/ReferenceRouteConsensus.h').read_text(encoding='utf-8')
SCENE=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
HEADER=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')
NATIVE=(ROOT/'tests/document_audit_tests.cpp').read_text(encoding='utf-8')
FACE=(ROOT/'tests/face_lineage_v170_tests.cpp').read_text(encoding='utf-8')
CM=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')

class TopologySafety(unittest.TestCase):
    def test_witness_tolerance_is_bounded_not_proportional_for_huge_stock(self):
        self.assertIn('std::clamp(oldLength*1e-6,1e-5,1e-3)',SHAPE)
        self.assertNotIn('std::max(1e-5,oldLength*1e-6)',SHAPE)

    def test_two_witness_placements_must_agree_before_returning(self):
        start=SHAPE.index('bool resolveStableEdgeAnchor(')
        end=SHAPE.index('// Special-purpose full-circle identity',start)
        body=SHAPE[start:end]
        self.assertIn('int agreedEdge=0;',body)
        self.assertIn('agreedEdge!=0&&agreedEdge!=selected',body)
        self.assertIn('if(agreedEdge!=0){*edgeIndex=agreedEdge;',body)
        self.assertNotIn('if(matches==1){*edgeIndex=selected;return true;}',body)

    def test_competing_kernel_occurrences_have_native_regression(self):
        self.assertIn('resolveStableEdgeAnchor(duplicateSupports,edgeHint,',FACE)
        self.assertIn('successor==retainedSuccessor',FACE)
        self.assertIn('different successors',FACE)

    def test_consensus_checks_every_pair(self):
        self.assertIn('for(std::size_t j=i+1;j<routes.size();++j)',ROUTES)
        self.assertIn('samePhysicalWitness(routes[i].witness,routes[j].witness',ROUTES)
        self.assertIn('descriptor.find(key)',ROUTES)

    def test_production_interleaved_history_uses_strict_consensus(self):
        self.assertIn('lineage::allTrackedPaths(ancestry',SCENE)
        self.assertIn('lineage::agreeReferenceRoutes(routeEvidence',SCENE)
        self.assertLess(SCENE.index('lineage::agreeReferenceRoutes(routeEvidence'),
                        SCENE.index('resolved=std::move(referenceResult);'))

class RevisionRecovery(unittest.TestCase):
    def test_dedicated_geometry_review_is_exposed(self):
        self.assertIn('bool reviewDrawingEdgeDimension(',HEADER)
        self.assertIn('bool SceneDocument::reviewDrawingEdgeDimension(',SCENE)

    def test_review_checks_both_identity_and_measured_projection(self):
        review=SCENE.split('bool SceneDocument::reviewDrawingEdgeDimension(',1)[1].split(
            'bool SceneDocument::setInterleavedFeatureHistory(',1)[0]
        for token in ('original.edgeAnchor.isEmpty()',
                      'resolveStableEdgeAnchor(source->shape,original.edgeAnchor',
                      'resolveResizedFullCircleForDrawing(',
                      'validateDrawingEdgeDimensionPreview(*source,reviewed,',
                      'reviewed.sourceGeometryToken=source->geometryToken;'):
            with self.subTest(token=token): self.assertIn(token,review)

    def test_failure_is_before_checkpoint(self):
        review=SCENE.split('bool SceneDocument::reviewDrawingEdgeDimension(',1)[1].split(
            'bool SceneDocument::setInterleavedFeatureHistory(',1)[0]
        self.assertLess(review.index('validateDrawingEdgeDimensionPreview'),review.index('checkpoint();'))
        self.assertLess(review.index('checkpoint();'),review.index('parts_=std::move(staged)'))
        self.assertIn('return false;',review)

    def test_generic_token_rewrite_checks_actual_geometry(self):
        prepare=SCENE.split('bool SceneDocument::prepareDrawingSheetSettings(',1)[1].split(
            'DrawingSheetSourceStatus SceneDocument::drawingSheetSourceStatus(',1)[0]
        self.assertIn('old->sourceGeometryToken==dimension.sourceGeometryToken',prepare)
        self.assertIn('resolveStableEdgeAnchor(source->shape,dimension.edgeAnchor',prepare)
        self.assertIn('setError(error,QStringLiteral("Drawing edge review rejected:',prepare)
        self.assertLess(prepare.index('Drawing edge review rejected:'),prepare.index('*prepared=std::move(persisted)'))

    def test_aggregate_sheet_refresh_cannot_approve_annotation(self):
        status=SCENE.split('DrawingSheetSourceStatus SceneDocument::drawingSheetSourceStatus(',1)[1].split(
            'QString SceneDocument::drawingSourceDataToken(',1)[0]
        self.assertIn('staleAnnotationIds.append(annotation.id)',status)
        self.assertIn('report.staleAnnotationIds.isEmpty()',status)
        self.assertIn('master->drawingSettings.edgeDimensionAnnotations',status)
        self.assertIn('master->drawingSettings.gdtFrames',status)

    def test_native_roundtrip_covers_stale_and_reviewed_states(self):
        for token in ('stale-after-redo.cadscene','reviewed.cadscene',
                      'recovery.redo()', 'recovery.undo()',
                      'reopened.load(stalePath', 'verified.load(approvedPath',
                      'reopened.reviewDrawingEdgeDimension(',
                      'forged.edgeDimensionAnnotations[0].edgeAnchor',
                      'DrawingSourceState::Stale','DrawingSourceState::Current'):
            with self.subTest(token=token):self.assertIn(token,NATIVE)

    def test_native_130_ctest_registry_unchanged(self):
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\w+)',CM)
        expected=(ROOT/'tests/BASELINE_130_NATIVE_CTEST_NAMES.txt').read_text().splitlines()
        self.assertEqual(len(names),130)
        self.assertEqual(set(names),set(expected))
        self.assertIn('cadcore_document_audit',names)

if __name__=='__main__':unittest.main()

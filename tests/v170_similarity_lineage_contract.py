"""Portable pre-build contracts for exact similarity ancestry; not a Windows CTest."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]

def read(path):return (ROOT/path).read_text(encoding='utf8')

class SimilarityLineageContracts(unittest.TestCase):
    def test_lineage_registered_in_document_replay(self):
        doc=read('src/core/SceneDocument.cpp')
        for token in ('supportsSimilarityReferenceLineage(step.feature)',
                      'transportDesignPointThroughSimilarity(',
                      'rebaseFeatureThroughSimilarity(',
                      'addParents(rigidSources);',
                      'lineage::agreeReferenceRoutes(routeEvidence'):
            self.assertIn(token,doc)
        self.assertLess(doc.index('rebaseFeatureThroughSimilarity('),
                        doc.index('if(!rebuildShapeFeatureTree(input,false,{executable},&next,error,'))
    def test_scale_policy_is_positive_and_bounded(self):
        core=read('src/core/BooleanReferenceLineage.cpp')
        for token in ('operation.type==QStringLiteral("Uniform scale")',
                      'transform->SetScale(gp_Pnt(0,0,0),value("factor"))',
                      'value("factor")<1e-4', 'value("factor")>1e4'):
            self.assertIn(token,core)
        self.assertIn('operation.type==QStringLiteral("Mirror XY")',core)
        self.assertIn('transform->SetMirror(gp_Ax2(origin,normal))',core)
    def test_surface_and_edge_units_are_transported(self):
        core=read('src/core/BooleanReferenceLineage.cpp')
        for token in ('edge_length",old.value("edge_length")*scale',
                      'edge_curvature",old.value("edge_curvature")/scale',
                      'face_anchor_area",old.value("face_anchor_area")*scale*scale',
                      'curved_radius",old.value("curved_radius")*scale',
                      'curved_aux",old.value("curved_aux")*scale',
                      'direction.Normalize()',
                      'Uniform-scale rebasing of a freeform face requires explicit reselection'):
            self.assertIn(token,core)
        self.assertIn('if(feature.parameters.contains("u")&&feature.parameters.contains("v"))',core)
    def test_native_tests_cover_scale_and_rollback(self):
        face=read('tests/face_lineage_v170_tests.cpp')
        curved=read('tests/curved_face_lineage_v170_tests.cpp')
        for token in ('scaled planar face support area is updated exactly',
                      'downstream tool diameter is NOT implicitly scaled',
                      'circular edge witness follows scaled support radius',
                      'invalid scale must fail closed',
                      'two scale stages retain the original physical edge dimension',
                      'two mirrors recover original edge witness'):
            self.assertIn(token,face)
        for token in ('cylindrical opening must survive positive uniform scale',
                      'both toroidal radii must scale consistently',
                      'shell wall thickness remains an independent downstream dimension'):
            self.assertIn(token,curved)
    def test_public_api_and_old_rigid_compatibility_retained(self):
        header=read('src/core/BooleanReferenceLineage.h')
        core=read('src/core/BooleanReferenceLineage.cpp')
        for name in ('supportsRigidReferenceLineage','transportDesignPointThroughRigid',
                     'rebaseFeatureThroughRigid','supportsSimilarityReferenceLineage',
                     'transportDesignPointThroughSimilarity','rebaseFeatureThroughSimilarity'):
            self.assertIn(name,header)
            self.assertIn(name,core)

if __name__=='__main__': unittest.main()

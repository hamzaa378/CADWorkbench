"""Source/logic guard for the Windows-only Boolean merge implementation."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
class ReferenceAgreementContracts(unittest.TestCase):
    def test_witness_cannot_be_erased_by_recapture(self):
        source=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
        self.assertIn('gp_Pnt pathWitness=originalWitness',source)
        self.assertIn('lineage::agreeReferenceRoutes(routeEvidence',source)
        self.assertIn('samePhysicalWitness(', (ROOT/'src/core/ReferenceRouteConsensus.h').read_text(encoding='utf-8'))
        self.assertIn('transportDesignPointThroughSimilarity(',source)
        self.assertIn('transportDesignPointThroughPattern(',source)
        self.assertIn('Merged ancestry requires a saved physical design witness',source)
        self.assertLess(source.index('lineage::agreeReferenceRoutes(routeEvidence'),
                        source.index('resolved=std::move(referenceResult);'))

    def test_selected_occurrence_transport_is_shared_with_rebase(self):
        source=(ROOT/'src/core/BooleanReferenceLineage.cpp').read_text(encoding='utf-8')
        self.assertIn('selectedPatternRigidOperation(operation,occurrence,&rigid,error)',source)
        self.assertIn('return transportDesignPointThroughRigid(rigid,designPoint,error)',source)
        self.assertIn('return rebaseFeatureThroughRigid(source,result,rigid,feature,rebound,error)',source)

    def test_native_regression_checks_rollback_and_roundtrip(self):
        test=(ROOT/'tests/interleaved_history_v170_tests.cpp').read_text(encoding='utf-8')
        self.assertIn('disagreeing Boolean ancestry routes must reject tracked references',test)
        self.assertIn('ambiguous Boolean diamond preserved committed shape',test)
        self.assertIn('rejected merge did not corrupt persistent feature history',test)

if __name__ == '__main__':unittest.main()

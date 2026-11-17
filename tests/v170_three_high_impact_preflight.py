"""Source integration checks; these are not native MSVC/Qt build tests."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
def contents(filename):return (ROOT/filename).read_text(encoding='utf-8')
class NewEngineeringContracts(unittest.TestCase):
    def test_spline_surface_signatures_and_projection_guard(self):
        core=contents('src/core/ShapeFeatureTree.cpp')
        for key in ('GeomAbs_BSplineSurface','GeomAbs_BezierSurface',
                    'GeomAPI_ProjectPointOnSurf',
                    'projection.LowerDistance()>std::max(1e-5,std::min(0.25,radius*0.01))',
                    'std::abs(normal.Dot(oldAxis))<0.98',
                    'classification.State()!=TopAbs_IN',
                    'type!=5.0&&type!=6.0',
                    'Curved-face lineage is ambiguous after topology changes'):
            self.assertIn(key,core)
    def test_all_boolean_ancestry_routes_must_agree(self):
        engine=contents('src/core/SceneDocument.cpp')
        for key in ('lineage::allTrackedPaths(ancestry',
                    'for(const auto& chain:routes)',
                    'lineage::agreeReferenceRoutes(routeEvidence',
                    'Persistent Boolean ancestry conflict',
                    'resolved=std::move(referenceResult)',
                    'if(!rebuildShapeFeatureTree(input,false,{executable},&next,error,'):
            self.assertIn(key,engine)
        consensus=contents('src/core/ReferenceRouteConsensus.h')
        self.assertIn('samePhysicalWitness(',consensus)
        self.assertIn('key=="edge_index"',consensus)
        planner=contents('src/core/BooleanLineagePath.h')
        self.assertIn('constexpr std::size_t maxPaths=32',planner)
        self.assertIn('if(overflow||cycle||complete.empty())',planner)
    def test_pattern_occurrence_persistence_rebuild_and_ui(self):
        engine=contents('src/core/SceneDocument.cpp')
        ui=contents('src/ui/MainWindow.cpp')
        rebase=contents('src/core/BooleanReferenceLineage.cpp')
        for key in ('patternOperations.insert(step.id,step.feature)',
                    'addParents(patternSources);','rebaseFeatureThroughPattern(',
                    'patternHops!=1','dag-interleaved-history-v4',
                    'withOccurrence&&(!patternHistory||!withReference',
                    'itemStep.insert("reference_occurrence"'):
            self.assertIn(key,engine)
        for key in ('Selected pattern occurrence no longer exists',
                    'rebaseFeatureThroughRigid(source,result,rigid,feature,rebound,error)',
                    'sweep>=360.0-1e-8?sweep/count:sweep/(count-1)'):
            self.assertIn(key,rebase)
        for key in ('auto* occurrenceTable=new QTableWidget',
                    'next.referencePatternOccurrences.insert(it.key(),it.value()->value())',
                    '"Linear pattern X"','"Circular pattern Z"'):
            self.assertIn(key,ui)
        self.assertIn('referencePatternOccurrence=0',contents('src/core/SceneDocument.h'))
if __name__=='__main__':unittest.main()

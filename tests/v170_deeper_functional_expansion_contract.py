from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
def src(path): return (ROOT/path).read_text(encoding="utf-8")
class DeeperExpansion(unittest.TestCase):
    def test_parametric_feature_batch_is_atomic_and_interactive(self):
        cpp=src('src/core/SceneDocument.cpp')
        body=cpp.split('bool SceneDocument::setParametricFeaturesSuppressedBatch(',1)[1].split('bool SceneDocument::previewParametricFeaturesSuppressedBatch(',1)[0]
        for expected in ('QVector<Part> candidate=parts_', 'rebuildParametricSolid(', 'propagateLinkedBodies(&candidate,error)', 'checkpoint();parts_=std::move(candidate)'):
            self.assertIn(expected,body)
        self.assertLess(body.index('propagateLinkedBodies(&candidate,error)'),body.index('checkpoint();parts_=std::move(candidate)'))
        ui=src('src/ui/MainWindow.cpp')
        self.assertIn('editParametricFeatureSuppressionBatch',ui)
        self.assertIn('previewParametricFeaturesSuppressedBatch(desired,&report,&error)',ui)
        self.assertIn('setParametricFeaturesSuppressedBatch(desired,&error)',ui)
    def test_assembly_same_time_broad_phase(self):
        h=src('src/core/SimultaneousProximity.h')
        self.assertIn('class Tracker',h)
        self.assertIn('class OrderInvariantTracker',h)
        self.assertIn('tracker_.observe(canonical,error)',h)
        self.assertIn('newIds!=ids_',h)
        cpp=src('src/core/SceneDocument.cpp')
        seg=cpp.split('bool SceneDocument::buildLiveArticulatedEnvelopeCsv(',1)[1].split('bool SceneDocument::updateLiveArticulatedCoordinates(',1)[0]
        for key in ('proximity::OrderInvariantTracker sameTime;', 'sameTime.observe(', 'same_time_aabb_overlap_samples', 'closest_same_time_sample'):
            self.assertIn(key,seg)
        self.assertLess(seg.index('sameTime.observe('),seg.index('*csv=std::move(generated)'))
        self.assertIn('simultaneous_proximity',src('scripts/verify_portable_consolidation.sh'))
    def test_stock_free_region_reuse_is_not_shelf_planning(self):
        h=src('src/core/StockSheetLayout.h')
        for marker in ('struct FreeRegion', 'void consume(', 'freeBySheet','Duplicate blank identifier.', '*output=std::move(next)'):
            self.assertIn(marker,h)
        self.assertIn('result.sheets==1',src('tests/stock_sheet_layout_portable_tests.cpp'))
if __name__=='__main__': unittest.main()

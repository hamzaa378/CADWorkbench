import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

class CurvedRatioCutContract(unittest.TestCase):
    def read(self,filename):
        return (ROOT/filename).read_text(encoding='utf8')
    def test_curved_witness_retained_on_boolean(self):
        source=self.read('src/core/BooleanReferenceLineage.cpp')
        segment=source.split('}else if(feature.type==QStringLiteral("Open-face shell")&&',1)[1].split('}else if(selectedFace(feature.type))',1)[0]
        self.assertIn('curved_witness2_',segment)
        self.assertIn('fresh.insert(field,oldAnchor.value(field))',segment)
        self.assertLess(segment.index('resolveCurvedFaceAnchor(result,oldAnchor'),segment.index('fresh.insert(field'))
        test=self.read('tests/curved_face_lineage_v170_tests.cpp')
        self.assertIn('secondCrop',test)
        self.assertIn('twiceRetargeted',test)
    def test_ui_edits_graph_ratios_not_only_one_link(self):
        source=self.read('src/ui/MainWindow.cpp')
        section=source.split('void MainWindow::editSavedArticulatedDrives(){',1)[1].split('void MainWindow::',1)[0]
        for marker in ('additionalCouplings->insertRow','removeRow(', 'std::vector<cad::drive_coupling::Relation> relations',
                       'relations.push_back','drive_coupling::solve(driveInputs,relations',
                       'previewLiveArticulatedCoordinates(targets', 'commitLiveArticulatedCoordinatesChecked('):
            self.assertIn(marker,section)
        self.assertLess(section.index('drive_coupling::solve(driveInputs,relations'),
                        section.index('previewLiveArticulatedCoordinates(targets'))
    def test_cut_length_measures_union_not_envelope(self):
        header=self.read('src/core/StockContourMetrics.h')
        for marker in ('contour_nesting::materialFor','visibleSpan(', 'exposedCutLengthMm',
                       'std::sort(covered.begin()', 'internal.emplace_back'):
            self.assertIn(marker,header)
        script=self.read('scripts/verify_portable_consolidation.sh')
        self.assertIn('stock_contour_metrics_portable_tests.cpp',script)
    def test_batch_cut_length_is_atomic_and_hashed(self):
        source=self.read('src/core/SceneDocument.cpp')
        section=source.split('bool SceneDocument::exportLinkedFlatManufacturingBatch(',1)[1].split('bool SceneDocument::editFoldedSheetMetal(',1)[0]
        for marker in ('contour_metrics::measure','cut-length-summary.csv','total_exposed_neutral_cut_length_mm',
                       'cut_length_csv_sha256','QTemporaryDir staging'):
            self.assertIn(marker,section)
        self.assertLess(section.index('contour_metrics::measure'),section.index('QDir().rename(staging.path(),newDirectory)'))
        self.assertLess(section.index('write("cut-length-summary.csv"'),section.index('QDir().rename(staging.path(),newDirectory)'))
        test=self.read('tests/cadcore_tests.cpp')
        self.assertIn('cut_length_csv_sha256',test)
if __name__=='__main__':unittest.main()

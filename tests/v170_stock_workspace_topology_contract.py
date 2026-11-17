from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[1]
read=lambda x:(root/x).read_text(encoding="utf-8")
class NewThreeWorkstreamContract(unittest.TestCase):
    def test_new_stock_layout_is_real_packing_and_atomic(self):
        h=read("src/core/StockSheetLayout.h")
        for marker in ("std::isfinite", "consume", "freeBySheet", "next.sheets", "Duplicate blank identifier", "*output=std::move(next)"):
            self.assertIn(marker,h)
        self.assertIn("stock_sheet_layout",read("scripts/verify_portable_consolidation.sh"))
        self.assertIn("assert(result.sheets==2",read("tests/stock_sheet_layout_portable_tests.cpp"))
    def test_manufacturing_batch_generates_stock_plan_before_publication(self):
        doc=read("src/core/SceneDocument.cpp")
        segment=doc.split("bool SceneDocument::exportLinkedFlatManufacturingBatch(",1)[1].split("bool SceneDocument::editFoldedSheetMetal(",1)[0]
        for token in ("stock_layout::build(","stock-layout.csv","stock-layout.svg","stock_layout_csv_sha256","rectangular_utilization_fraction","QTemporaryDir staging", "QDir().rename(staging.path(),newDirectory)"):
            self.assertIn(token,segment)
        self.assertLess(segment.index("stock_layout::build("),segment.index("QTemporaryDir staging"))
        self.assertLess(segment.index('write("stock-layout.csv"'),segment.index("QDir().rename(staging.path(),newDirectory)"))
    def test_stock_layout_is_optional_in_ui_and_api(self):
        h=read("src/core/SceneDocument.h")
        ui=read("src/ui/MainWindow.cpp")
        self.assertIn("const StockLayoutSettings& stock={}",h)
        seg=ui.split("void MainWindow::exportSelectedAssociativeFlatJobBatch(){",1)[1].split("void MainWindow::exportSelectedFoldedSheetFlatPatternDxf()",1)[0]
        self.assertIn("stock.stockWidthMm=QInputDialog::getDouble",seg)
        self.assertIn("stock.allowRotation=QMessageBox::question",seg)
        self.assertIn("exportLinkedFlatManufacturingBatch(ids,destination,&issue,stock)",seg)
    def test_alternatives_measure_changed_brep_topology(self):
        doc=read("src/core/SceneDocument.cpp")
        body=doc.split("bool SceneDocument::evaluateParametricDesignAlternatives(",1)[1].split("QStringList SceneDocument::suppressedParametricFeatures(",1)[0]
        for marker in ("BRepGProp::VolumeProperties(part.shape", "BRepGProp::VolumeProperties(changed->shape", "TopExp_Explorer e(part.shape,TopAbs_FACE)","TopExp_Explorer e(changed->shape,TopAbs_EDGE)","row.topologyCountChangedBodies", "row.bodyImpacts.append"):
            self.assertIn(marker,body)
        self.assertLess(body.index("row.bodyImpacts.append"),body.index("*results=std::move(stagedResults)"))
    def test_workspace_overlap_is_candidate_not_collision(self):
        doc=read("src/core/SceneDocument.cpp")
        body=doc.split("bool SceneDocument::buildLiveArticulatedEnvelopeCsv(",1)[1].split("bool SceneDocument::updateLiveArticulatedCoordinates(",1)[0]
        for marker in ("stageLiveArticulatedRegeneration(","BRepBndLib::Add(","possible_workspace_overlap_count", "nearest_workspace_box_gap_mm", "aabbGap", "neither box overlap proves solid collision"):
            self.assertIn(marker,body)
        self.assertLess(body.index("stageLiveArticulatedRegeneration("),body.index("*csv=std::move(generated)"))
if __name__=="__main__":unittest.main()

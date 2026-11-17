from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
def source(path): return (ROOT/path).read_text(encoding="utf-8")

class ArticulatedFlatPreviewContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc=source("src/core/SceneDocument.cpp")
        cls.header=source("src/core/SceneDocument.h")
        cls.ui=source("src/ui/MainWindow.cpp")
        cls.sheet=source("src/core/SheetMetal.cpp")
        cls.sheet_header=source("src/core/SheetMetal.h")

    def test_articulated_preview_is_read_only_and_transactional(self):
        body=self.doc.split("bool SceneDocument::previewLiveArticulatedJointCoordinate(",1)[1].split(
            "bool SceneDocument::updateLiveArticulatedJointCoordinate(",1)[0]
        for marker in ["stageLiveArticulatedRegeneration(proposed,&stagedParts,&geometryChanged,error)",
                       "inspectAssemblyFit(staged,ids,requiredClearanceMm,&pairs,error)",
                       "*report=lines.join", "validThreshold(requiredClearanceMm)"]:
            self.assertIn(marker,body)
        self.assertLess(body.index("inspectAssemblyFit("),body.index("*report="))
        self.assertNotIn("checkpoint();",body)
        self.assertNotIn("liveArticulatedJoints_=",body)
        self.assertNotIn("\n    parts_=std::move(stagedParts)",body)

    def test_articulated_dialog_previews_before_committing(self):
        body=self.ui.split("void MainWindow::editSavedArticulatedJoint(){",1)[1].split(
            "void MainWindow::auditLiveRigidMateStructure(){",1)[0]
        self.assertLess(body.index("previewLiveArticulatedJointCoordinate("),
                        body.index("updateLiveArticulatedJointCoordinate("))
        self.assertIn("if(review.exec()!=QDialog::Accepted)return;",body)
        self.assertIn("requiredGap",body)
        self.assertIn("previewLiveArticulatedJointCoordinate",self.header)

    def test_articulated_native_regression_is_written(self):
        native=source("tests/articulated_assembly_scene_v170_tests.cpp")
        for marker in ["read-only driven revolute preview", "prismatic clearance preview",
                       "rejected driven preview mutated geometry or caller report",
                       "committed prismatic pose disagrees with preview"]:
            self.assertIn(marker,native)

    def test_flat_svg_uses_validated_dxf_geometry(self):
        body=self.sheet.split("bool buildFoldedSheetFlatPatternSvg(",1)[1].split(
            "bool exportFoldedSheetFlatPatternDxf(",1)[0]
        self.assertIn("buildFoldedSheetFlatPatternDxf(",body)
        self.assertIn("pattern.bendLinePositionsMm",body)
        self.assertIn("relief.depthMm",body)
        self.assertLess(body.index("buildFoldedSheetFlatPatternDxf("),body.index("*svg="))
        self.assertIn("buildFoldedSheetFlatPatternSvg",self.sheet_header)

    def test_flat_review_before_export(self):
        body=self.ui.split("void MainWindow::exportSelectedFoldedSheetFlatPatternDxf()",1)[1].split(
            "void MainWindow::exportSelectedFoldedSheetBendTableCsv()",1)[0]
        self.assertLess(body.index("buildFoldedSheetFlatPatternSvg("),
                        body.index("exportFoldedSheetFlatPatternDxf("))
        self.assertLess(body.index("review.exec()"),body.index("exportFoldedSheetFlatPatternDxf("))
        self.assertIn("QSvgRenderer flatRenderer",body)

    def test_parametric_formula_preview_uses_production_rebuild(self):
        segment=self.doc.split("bool SceneDocument::previewNamedParameterFormulas(",1)[1].split(
            "bool SceneDocument::setNamedParameterFormulas(",1)[0]
        self.assertIn("SceneDocument staged=*this",segment)
        self.assertIn("staged.setNamedParameterFormulas(partId,formulas,error)",segment)
        self.assertIn("BRepGProp::VolumeProperties",segment)
        self.assertIn("*report=lines.join",segment)
        self.assertNotIn("checkpoint();",segment)
        self.assertIn("previewNamedParameterFormulas",self.header)
        ui=self.ui.split("void MainWindow::editNamedParameter()",1)[1].split(
            "void MainWindow::exportFeatureInspectionNotes()",1)[0]
        self.assertLess(ui.index("previewNamedParameterFormulas("),ui.index("setNamedParameterFormulas("))

    def test_parametric_formula_preview_native_rollback(self):
        native=source("tests/cadcore_tests.cpp")
        self.assertIn("Parametric formula preflight altered the saved definition",native)
        self.assertIn("Cyclic formula preflight overwrote the report",native)

    def test_flat_native_regression_covers_rollback(self):
        native=source("tests/cadcore_tests.cpp")
        self.assertIn("Relieved SVG preview did not match the validated multi-bend DXF",native)
        self.assertIn("Rejected SVG relief mutated saved preview bytes or pattern",native)
        self.assertIn("Wrong K-factor count silently changed folded-sheet SVG preview",native)

if __name__=="__main__":unittest.main()

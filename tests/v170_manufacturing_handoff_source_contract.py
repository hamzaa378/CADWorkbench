"""Project-local integration contracts for the Windows-only Qt export implementation.

Static integration checks are NOT substitutes for MSVC compilation/native tests.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ManufacturingHandoffIntegrationContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "src/core/ManufacturingHandoff.cpp").read_text(encoding="utf-8")
        cls.header = (ROOT / "src/core/ManufacturingHandoff.h").read_text(encoding="utf-8")
        cls.main = (ROOT / "src/ui/MainWindow.cpp").read_text(encoding="utf-8")
        cls.ui_header = (ROOT / "src/ui/MainWindow.h").read_text(encoding="utf-8")
        cls.cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

    def test_build_includes_implementation(self):
        self.assertIn("src/core/ManufacturingHandoff.cpp", self.cmake)
        self.assertIn("src/core/ManufacturingHandoff.h", self.cmake)

    def test_menu_and_ui_method_wired(self):
        self.assertIn("Create UNISSUED manufacturing handoff package", self.main)
        self.assertIn("void MainWindow::exportManufacturingHandoff()", self.main)
        self.assertIn("void exportManufacturingHandoff();", self.ui_header)

    def test_saved_project_gate(self):
        self.assertIn("document.dirty()", self.source)
        self.assertIn("document.path().isEmpty()", self.source)
        self.assertIn("runDocumentAudit(document)", self.source)
        self.assertIn("if(audit.errorCount>0)", self.source)

    def test_outputs_are_real_scene_exports(self):
        for symbol in ("document.exportSTEP(","document.exportBillOfMaterialsCsv(",
                       "document.exportPartsListCsv(","document.exportBodyScheduleCsv(",
                       "document.billOfMaterials("):
            with self.subTest(symbol=symbol):self.assertIn(symbol,self.source)

    def test_rollback_and_atomic_directory_promotion(self):
        self.assertIn("struct StageCleanup", self.source)
        self.assertIn("directory.removeRecursively()", self.source)
        self.assertIn("parent.rename(transient,packageName)", self.source)
        self.assertIn("committed=true", self.source)
        self.assertLess(self.source.index("parent.rename(transient,packageName)"),
                        self.source.index("committed=true"))

    def test_no_approval_without_engineering_review(self):
        self.assertIn('"UNISSUED_REVIEW_REQUIRED"',self.source)
        self.assertIn("NOT an approved manufacturing release",self.source)
        self.assertIn("no engineering sign-off",self.source.lower())

    def test_sha_and_file_inventory(self):
        self.assertIn("QCryptographicHash::Sha256",self.source)
        self.assertIn('"handoff_manifest.json"',self.source)
        self.assertIn('"procurement_exceptions.csv"',self.source)
        self.assertIn('"feature_inspection_requirements.csv"',self.source)

    def test_no_unsafe_csv_spreadsheet_expressions(self):
        self.assertIn("safeCsv(QString field)",self.source)
        self.assertIn("field.prepend(QChar(0x27))",self.source)

    def test_extracted_file_verifier_exists(self):
        verifier=(ROOT/"tools/verify_manufacturing_handoff.py").read_text(encoding="utf-8")
        self.assertIn("hashlib.sha256",verifier)
        self.assertIn('"UNISSUED_REVIEW_REQUIRED"',verifier)


    def test_export_snapshot_rechecked(self):
        self.assertIn("Saved CAD file changed during export",self.source)
        self.assertIn("finalSavedSource.digest!=entries.first().digest",self.source)

    def test_bom_quantity_gated(self):
        self.assertIn("bomQuantity!=document.parts().size()",self.source)

    def test_all_csv_exports_formula_hardened(self):
        parts=(ROOT/"src/core/PartsData.cpp").read_text(encoding="utf-8")
        document=(ROOT/"src/core/SceneDocument.cpp").read_text(encoding="utf-8")
        self.assertIn('QStringLiteral("=+-@")',parts)
        self.assertIn('QStringLiteral("=+-@")',document)

if __name__=='__main__':unittest.main()

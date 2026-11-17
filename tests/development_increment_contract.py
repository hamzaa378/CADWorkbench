"""Static wiring checks only; Windows C++ build/CTest must still be run."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


class DevelopmentIncrementContract(unittest.TestCase):
    def test_core_wiring(self):
        cmake = read("CMakeLists.txt")
        self.assertIn("src/core/DocumentAudit.cpp", cmake)
        self.assertIn("src/core/DocumentAudit.h", cmake)
        self.assertIn("cadcore_document_audit", cmake)
        self.assertIn("tests/document_audit_tests.cpp", cmake)

    def test_public_read_only_audit_contract(self):
        header = read("src/core/DocumentAudit.h")
        source = read("src/core/DocumentAudit.cpp")
        self.assertRegex(header, r"DocumentAuditReport\s+runDocumentAudit\(const SceneDocument& document\)")
        self.assertIn("document.checkStandards(StandardsProfile::BomData)", source)
        for issue in ("PARAMETRIC_STALE", "SKETCH_STALE", "FOLDED_DEFINITION_STALE",
                      "JOINT_PARENT_INVALID", "DRAWING_SOURCE_STALE", "DRAWING_SOURCE_MISSING",
                      "DIMENSION_EDGE_STALE", "BALLOON_SOURCE_MISSING"):
            self.assertIn(issue, source)
        for mutation in ("document.refreshDrawingSheetSource(", "document.setDrawingSheetSettings(",
                         "document.undo(", "document.saveAs("):
            self.assertNotIn(mutation, source)

    def test_ui_commands_use_real_actions(self):
        header = read("src/ui/MainWindow.h")
        source = read("src/ui/MainWindow.cpp")
        self.assertIn("void showDocumentAudit();", header)
        self.assertIn("void openCommandSearch();", header)
        self.assertIn("runDocumentAudit(document_)", source)
        self.assertIn("QKeySequence(\"Ctrl+K\")", source)
        self.assertIn("action->trigger()", source)
        self.assertIn("entry.action->isEnabled()", source)
        self.assertIn("documentAuditAction,partsReportAction", source)

    def test_native_regression_cases(self):
        native = read("tests/document_audit_tests.cpp")
        for check in ("DOC_EMPTY", "PART_NUMBER_MISSING", "DRAWING_SOURCE_STALE",
                      "PARAMETRIC_STALE", "document.undo()", "toCsv()", "toPlainText()"):
            self.assertIn(check, native)

    def test_manifest_packaging_excludes_old_manifest(self):
        script = read("scripts/package_source_snapshot.ps1")
        self.assertIn("$_.Name -ne 'SOURCE_MANIFEST.sha256'", script)

    def test_roadmap_is_explicit_about_nonverification(self):
        notes = read("docs/DEVELOPMENT_INCREMENT_NOTES.md")
        roadmap = read("docs/ROADMAP_TO_PROFESSIONAL_CAD.md")
        self.assertIn("yet passed a Windows Release build", notes)
        self.assertIn("G1", roadmap)
        self.assertIn("G12", roadmap)
        self.assertIn("transaction", roadmap.lower())


if __name__ == "__main__":
    unittest.main(verbosity=2)

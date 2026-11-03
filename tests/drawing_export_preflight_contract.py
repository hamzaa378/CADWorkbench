"""Source wiring regressions; native C++ tests remain authoritative."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "src/core/SceneDocument.cpp").read_text(encoding="utf-8-sig")
UI = (ROOT / "src/ui/MainWindow.cpp").read_text(encoding="utf-8-sig")
HEADER = (ROOT / "src/core/SceneDocument.h").read_text(encoding="utf-8-sig")
TESTS = (ROOT / "tests/cadcore_tests.cpp").read_text(encoding="utf-8-sig")


class ExportPreflightContracts(unittest.TestCase):
    def test_core_preparation_is_const_and_no_checkpoint(self):
        self.assertIn("DrawingSheetSettings* prepared,QString* error) const", CORE)
        self.assertIn("DrawingSheetSettings* prepared,QString* error = nullptr) const", HEADER)
        section = CORE.split("bool SceneDocument::prepareDrawingSheetSettings(", 1)[1].split(
            "DrawingSheetSourceStatus SceneDocument::drawingSheetSourceStatus", 1)[0]
        self.assertNotIn("checkpoint();", section)
        self.assertIn("*prepared=std::move(persisted);", section)

    def test_export_is_written_before_persistence_commit(self):
        section = UI.split("void MainWindow::exportDrawing(bool asPdf) {", 1)[1].split(
            "void MainWindow::exportSheetMetalFlatPattern()", 1)[0]
        prepared = section.index("document_.prepareDrawingSheetSettings(")
        exported = section.index("if(!exported){", prepared)
        commit = section.index("document_.setDrawingSheetSettings(", exported)
        self.assertLess(prepared, exported)
        self.assertLess(exported, commit)
        self.assertNotIn("document_.undo()", section)
        self.assertIn("prepared.noteAnnotations", section)
        self.assertIn("prepared.viewIds", section)

    def test_native_failure_and_undo_cases_present(self):
        self.assertIn("testDrawingExportPreflightDoesNotAcknowledgeFailedSnapshot", TESTS)
        self.assertIn("Failed snapshot consumed unrelated geometry history", TESTS)
        self.assertIn("Drawing export acknowledgement is not a single undoable edit", TESTS)


if __name__ == "__main__":
    unittest.main()

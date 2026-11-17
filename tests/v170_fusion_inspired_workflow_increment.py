"""Source-integration guards for the Windows-only Qt GUI usability update.

These contracts are not substitutes for native Qt compilation or GUI acceptance.
"""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/ui/MainWindow.cpp").read_text(encoding="utf-8")
CORE = (ROOT / "src/core/SceneDocument.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/ui/MainWindow.h").read_text(encoding="utf-8")
NATIVE = (ROOT / "tests/cadcore_tests.cpp").read_text(encoding="utf-8")


class WorkflowIntegrationTests(unittest.TestCase):
    def test_context_and_timeline_exist(self):
        for name in ("ContextActionsPanel", "ContextSelectionSummary", "FeatureTimelineDock", "FeatureTimelineList"):
            self.assertIn(name, MAIN)
        self.assertIn("void MainWindow::updateContextActions()", MAIN)
        self.assertIn("void MainWindow::updateFeatureTimeline()", MAIN)
        self.assertIn("&QListWidget::itemDoubleClicked", MAIN)
        self.assertIn("editGeneralFeatureTree();", MAIN)

    def test_no_duplicate_geometry_editing_engine(self):
        for name in ("isolateSelectedBodies", "hideSelectedBodies", "showAllBodies"):
            self.assertIn(f"void MainWindow::{name}()", MAIN)
        self.assertIn("document_.setVisibilityBatch(requested,&issue)", MAIN)
        self.assertIn("document_.isolateBodies(ids,&issue)", MAIN)
        self.assertIn("document_.showAllBodies(&issue)", MAIN)
        self.assertIn("void SceneDocument::checkpoint()", CORE)
        self.assertIn("if(!changed)return true;\n    checkpoint();", CORE)
        self.assertIn("testAtomicVisibilityWorkflow();", NATIVE)
        self.assertIn("Rejected visibility batch partly mutated document", NATIVE)

    def test_original_action_reuse(self):
        self.assertIn("button->setDefaultAction(contextCommands[i].second)", MAIN)
        self.assertIn("menu.addAction(generalTreeAction)", MAIN)
        self.assertIn("cad::ui::commandGlyph", MAIN)
        self.assertIn("CadCommandRibbon", MAIN)
        self.assertIn("&MainWindow::openCommandSearch", MAIN)

    def test_window_layout_persistence(self):
        self.assertIn("ui/workspace/geometry", MAIN)
        self.assertIn("ui/workspace/docks", MAIN)
        self.assertIn("void MainWindow::resetWorkspaceLayout()", MAIN)
        self.assertIn("if(maybeSave()){saveWorkspaceLayout();event->accept();}", MAIN)
        self.assertIn("void MainWindow::restoreWorkspaceLayout()", MAIN)

    def test_dedicated_real_world_workspaces(self):
        for tab in ('Drawing', 'Sheet Metal', 'Surface'):
            self.assertIn(f'addRibbonTab("{tab}"', MAIN)
        self.assertIn('workspacePicker->addItems({"Design","Assembly","Drawing","Inspect","View","Sheet Metal","Surface"})', MAIN)
        self.assertIn('static const int tabForWorkspace[]={1,3,6,4,5,7,8};', MAIN)
        self.assertIn('sheetBlankAction', MAIN)
        self.assertIn('surfaceThickenAction', MAIN)
        self.assertIn('arrangeDrawingViewsAction', MAIN)

    def test_recent_files_reject_stale(self):
        self.assertIn("RecentCadProjectsMenu", MAIN)
        self.assertIn("entry->setEnabled(info.exists()&&info.isFile())", MAIN)
        self.assertIn("if(!maybeSave())return;", MAIN)
        self.assertIn("void MainWindow::openRecentProject", MAIN)

    def test_shortcuts_and_browser_integration(self):
        self.assertIn("Ctrl+Shift+I", MAIN)
        self.assertIn("Ctrl+Shift+H", MAIN)
        self.assertIn("Ctrl+Shift+J", MAIN)
        self.assertIn("&QWidget::customContextMenuRequested", MAIN)
        self.assertIn("MainWindow::selectedIds() const", MAIN)
        self.assertIn("updateContextActions();", MAIN)
        self.assertIn("updateFeatureTimeline();", MAIN)

if __name__ == "__main__":
    unittest.main()

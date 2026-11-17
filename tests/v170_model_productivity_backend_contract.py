"""Document-level contracts for the next productivity UI + backend increment.

These are NOT native Qt execution tests; the Windows MSVC audit and real UI
acceptance remain independent required gates.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class ProductivityContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document_h=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')
        cls.document_cpp=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
        cls.main_h=(ROOT/'src/ui/MainWindow.h').read_text(encoding='utf-8')
        cls.main=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
        cls.native=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding='utf-8')
        cls.search=(ROOT/'src/core/DesignBrowserQuery.h').read_text(encoding='utf-8')
        cls.portable=(ROOT/'tests/design_browser_query_portable_tests.cpp').read_text(encoding='utf-8')

    def test_atomic_batch_rename_api_and_undo(self):
        self.assertIn('bool renameBatch(const QMap<QString,QString>& desired',self.document_h)
        method=self.document_cpp.split('bool SceneDocument::renameBatch(',1)[1].split('bool SceneDocument::setVisibility(',1)[0]
        self.assertIn('finalNames',method)
        self.assertIn('if(!find(it.key()))',method)
        self.assertIn('if(!changed)return true;',method)
        self.assertEqual(method.count('checkpoint();'),1)
        self.assertIn('part.name=proposal.value().trimmed()',method)

    def test_batch_rename_dialog_preview_and_one_transaction(self):
        self.assertIn('Batch rename selected bodies...',self.main)
        self.assertIn('Ctrl+Shift+R',self.main)
        self.assertIn('BatchRenamePreview',self.main)
        self.assertIn('document_.renameBatch(proposed,&issue)',self.main)
        self.assertIn('void MainWindow::batchRenameSelectedBodies()',self.main)
        self.assertIn('void batchRenameSelectedBodies();',self.main_h)
        self.assertIn('QDialogButtonBox::Cancel|QDialogButtonBox::Apply',self.main)

    def test_native_document_persistence_rollback(self):
        region=self.native.split('void testAtomicBatchRenameWorkflow()',1)[1].split('void testAtomicVisibilityWorkflow()',1)[0]
        for term in ('"swap names atomically"','"case collision accepted"',
                     '"missing body accepted"','doc.undo()', 'doc.redo()',
                     'doc.saveAs(file,&issue)','loaded.load(file,&issue)'):
            self.assertIn(term,region)
        self.assertIn('testAtomicBatchRenameWorkflow();',self.native)

    def test_structured_model_browser_query(self):
        for term in ('key=="material"','key=="kind"','key=="state"',
                     'key=="feature"','key=="part"','key=="name"','else return false;'):
            self.assertIn(term,self.search)
        self.assertIn('cad::browserquery::matches(index,query)',self.main)
        self.assertIn('applyBrowserFilter();',self.main)
        self.assertIn('browserMatchSummary_',self.main_h)
        self.assertIn('design browser query assertions',self.portable)

    def test_audit_can_navigate_to_an_actual_body(self):
        self.assertIn('AuditSelectAffectedBody',self.main)
        self.assertIn('viewport_->selectIds({id})',self.main)
        self.assertIn('document_.find(id)',self.main)
        self.assertIn('table,&QTableWidget::cellDoubleClicked',self.main)

    def test_hierarchical_assembly_navigator_is_an_unmodified_view(self):
        for marker in ('AssemblyHierarchyNavigator','AssemblyHierarchyDock',
                       'part.assemblyPath.trimmed()', 'tabifyDockWidget(browser,assemblyDock_)',
                       'void MainWindow::rebuildAssemblyNavigator()',
                       'void MainWindow::syncAssemblyNavigatorSelection(',
                       'const QSignalBlocker blocker(assemblyTree_)',
                       'body->setData(0,Qt::UserRole,part.id)',
                       'view->addAction(assemblyDock_->toggleViewAction())'):
            self.assertIn(marker,self.main)
        self.assertIn('QTreeWidget* assemblyTree_',self.main_h)
        self.assertNotIn('document_.moveAssemblySubtree(',self.main.split('void MainWindow::rebuildAssemblyNavigator()',1)[1].split('void MainWindow::applyBrowserFilter()',1)[0])

    def test_release_control_unchanged(self):
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertEqual(cmake.count('add_test(NAME '),130)
        self.assertTrue((ROOT/'RUN_V170_COMPREHENSIVE_AUDIT.ps1').exists())
        self.assertTrue((ROOT/'docs/V170_RELEASE_GATE_85_85_65.md').exists())

if __name__=='__main__': unittest.main()

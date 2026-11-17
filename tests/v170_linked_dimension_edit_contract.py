"""Source-level contract for safe in-place linked drawing dimension editing.
Not a replacement for the native C++ Qt/OpenCascade regression.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
HEAD=(ROOT/'src/ui/MainWindow.h').read_text(encoding='utf-8')
CORE=(ROOT/'src/core/DrawingSheet.cpp').read_text(encoding='utf-8')
TEST=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding='utf-8')

class LinkedDimensionEditContract(unittest.TestCase):
    def test_menu_and_slot(self):
        self.assertIn('editSelectedEdgeDrawingDimension();', HEAD)
        self.assertIn('this,&MainWindow::editSelectedEdgeDrawingDimension', UI)

    def test_preserves_reference_identity_and_revision(self):
        body=UI.split('void MainWindow::editSelectedEdgeDrawingDimension() {',1)[1].split(
             'void MainWindow::removeSelectedEdgeDrawingDimension() {',1)[0]
        self.assertIn('auto edited=original;',body)
        self.assertIn('updated.edgeDimensionAnnotations[index]=edited;',body)
        self.assertIn('if(edited==original)',body)
        self.assertIn('validateDrawingEdgeDimensionPreview(*source,edited,&error)',body)
        self.assertNotIn('sourceGeometryToken=',body)
        self.assertNotIn('refreshDrawingSheetSource(',body)

    def test_preflight_uses_real_export_resolver_without_mutations(self):
        method=CORE.split('bool validateDrawingEdgeDimensionPreview(',1)[1].split(
            'bool exportOrthographicDrawingSvg(',1)[0]
        self.assertIn('buildOrthographicDrawingSvg(source,&preview,error',method)
        self.assertIn('{dimension}, {source}',method)
        self.assertNotIn('checkpoint()',method)
        self.assertNotIn('sourceGeometryToken=',method)

    def test_native_cases_cover_history_and_invalid_views(self):
        for phrase in ('in-place manufacturing-dimension edit',
                       'In-place dimension edit did not round-trip through undo and redo',
                       'circular diameter viewed edge-on',
                       'negative manually entered tolerance',
                       'Edited dimension did not preserve its identity'):
            self.assertIn(phrase,TEST)

if __name__=='__main__':unittest.main()

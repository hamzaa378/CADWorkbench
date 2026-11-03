"""Source-level guard for drawing editors' source-revision semantics.

Run: python tests/drawing_revision_editor_contract.py
This checks UI wiring statically; it does not execute Qt/C++ or prove the app works.
"""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
UI = (ROOT / 'src/ui/MainWindow.cpp').read_text(encoding='utf-8-sig')
CORE = (ROOT / 'src/core/SceneDocument.cpp').read_text(encoding='utf-8-sig')


def method_body(name):
    match = re.search(r'void MainWindow::' + re.escape(name) + r'\(\)\s*\{', UI)
    if match is None:
        raise AssertionError(f'Drawing editor {name} missing')
    depth = 1
    start = match.end()
    for i in range(start, len(UI)):
        if UI[i] == '{':
            depth += 1
        elif UI[i] == '}':
            depth -= 1
            if depth == 0:
                return UI[start:i]
    raise AssertionError(f'Drawing editor {name} has no closing brace')


class DrawingRevisionEditorContract(unittest.TestCase):
    def test_annotation_editors_never_acknowledge_source_geometry(self):
        for editor in ('editDrawingViewLayout', 'editDrawingNotes',
                       'editDrawingDimensions', 'editDrawingPartsList',
                       'editDrawingBalloons', 'editDrawingDetails',
                       'editDrawingSections'):
            with self.subTest(editor=editor):
                body = method_body(editor)
                self.assertIn('setDrawingSheetSettings(', body)
                self.assertNotRegex(body, r'\bsourceGeometryToken\s*=')
                self.assertNotIn('refreshDrawingSheetSource(', body)

    def test_balloon_editor_keeps_source_validation(self):
        body = method_body('editDrawingBalloons')
        self.assertIn('cad::buildDrawingAssemblyPart(sources,&composite,&error)', body)
        self.assertNotIn('settings.sourceGeometryToken=composite.geometryToken', body)

    def test_view_layout_uses_aggregate_source_status(self):
        body = method_body('editDrawingViewLayout')
        self.assertIn('document_.drawingSheetSourceStatus(ids.first())', body)
        self.assertIn('cad::DrawingSourceState::Stale', body)
        self.assertIn('cad::DrawingSourceState::MissingBodies', body)
        self.assertNotIn('settings.sourceGeometryToken!=part->geometryToken', body)

    def test_explicit_export_can_record_current_source_revision(self):
        self.assertRegex(UI, r'settings\.sourceBodyIds\s*=\s*sourceBodyIds;'
                          r'\s*settings\.sourceGeometryToken\s*=\s*drawingPart\.geometryToken;')

    def test_new_sheets_use_aggregate_source_fingerprint(self):
        self.assertIn('persisted.sourceGeometryToken=drawingSourceFingerprint(sources);', CORE)
        self.assertIn('report.currentGeometryToken=drawingSourceFingerprint(sources);', CORE)
        self.assertIn('const QString current=drawingSourceFingerprint(sources);', CORE)


if __name__ == '__main__':
    unittest.main()

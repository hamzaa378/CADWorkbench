"""Source-level contracts for drawing persistence safeguards.

These audits are deliberately limited to source wiring. Native C++ regressions
in cadcore_tests.cpp must still run on Windows/MSVC before merging.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
CPP = (ROOT / 'src/core/SceneDocument.cpp').read_text(encoding='utf-8-sig')
HEADER = (ROOT / 'src/core/SceneDocument.h').read_text(encoding='utf-8-sig')
TESTS = (ROOT / 'tests/cadcore_tests.cpp').read_text(encoding='utf-8-sig')


class DrawingPersistenceContracts(unittest.TestCase):
    def test_partial_updates_preserve_existing_ids(self):
        for expression in (
            'persisted.sheetId=previous.sheetId',
            'persisted.partsListAnnotation.id=previous.partsListAnnotation.id',
            'persisted.viewIds.insert(it.key(),it.value())',
            'persisted.dimensionAnnotations=previous.dimensionAnnotations',
            'persisted.noteAnnotations.append(note)',
        ):
            with self.subTest(expression=expression):
                self.assertIn(expression, CPP)
        self.assertIn('Partial drawing edit regenerated persistent sheet/view/annotation identities', TESTS)

    def test_unchanged_sheet_skips_history_checkpoint(self):
        # Sheet normalization moved into a non-mutating export preflight.
        # The mutating setter must still check equality before checkpoint().
        setter = CPP.split('bool SceneDocument::setDrawingSheetSettings(', 1)[1].split(
            'bool SceneDocument::prepareDrawingSheetSettings(', 1)[0]
        self.assertRegex(setter, r'if\(part->hasDrawingSettings&&part->drawingSettings==persisted\)return true;'
                                 r'\s*checkpoint\(\);part->drawingSettings=std::move\(persisted\);')
        self.assertIn('prepareDrawingSheetSettings(partId,settings,&persisted,error)', setter)
        self.assertIn('bool operator==(const DrawingSheetSettings& other) const = default;', HEADER)
        self.assertIn('Saving unchanged sheet created an extra undo record', TESTS)

    def test_duplicate_sheet_ids_rejected_before_scene_commit(self):
        load_guard=CPP.index('QSet<QString> drawingSheetIds;')
        commit=CPP.index('parts_ = std::move(candidate);', load_guard)
        self.assertLess(load_guard, commit)
        self.assertIn('drawingSheetIds.contains(part.drawingSettings.sheetId)', CPP[load_guard:commit])
        self.assertIn('other.drawingSettings.sheetId==persisted.sheetId', CPP)
        self.assertIn('Invalid duplicate sheet IDs were loaded or mutated the open scene', TESTS)

    def test_svg_metadata_id_normalization_guards(self):
        for expression in ("value.contains(QLatin1Char('\\t'))",
                           "value.contains(QLatin1Char('\\n'))",
                           "value.contains(QLatin1Char('\\r'))",
                           "sourceId.contains(QLatin1Char(','))"):
            with self.subTest(expression=expression):
                self.assertIn(expression, CPP)
        self.assertIn('Loader accepted an ambiguous comma-separated drawing source ID', TESTS)


if __name__ == '__main__':
    unittest.main()

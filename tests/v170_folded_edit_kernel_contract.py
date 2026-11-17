"""Regression guards for the native folded editing crash diagnostics.

These source checks are not a substitute for the Windows OCCT native test.
"""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class FoldedEditKernelContract(unittest.TestCase):
    def test_extrusion_done_checked_before_shape_access(self):
        source = (ROOT / 'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
        start = source.index('TopoDS_Shape makeFoldedSheetSolid(')
        body = source[start:source.index('} // namespace', start)]
        self.assertIn('BRepPrimAPI_MakePrism extrusion(', body)
        self.assertLess(body.index('!extrusion.IsDone()'), body.index('extrusion.Shape()'))
        self.assertIn('solid.IsNull()', body)

    def test_each_edit_phase_is_visible_in_native_crash_output(self):
        source = (ROOT / 'tests/cadcore_tests.cpp').read_text(encoding='utf-8')
        for step in ('preview accepted: before', 'preview accepted: after',
                     'preview rejected: before', 'commit edited solid: before',
                     'undo/redo: before', 'undo/redo: after',
                     'save edited scene: before', 'save edited scene: after',
                     'load edited scene: before', 'load edited scene: after'):
            with self.subTest(step=step):
                self.assertIn('[folded edit] ' + step, source)

    def test_runner_does_not_hide_failed_native_case(self):
        runner = (ROOT / 'RUN_V170_FOLDED_EDIT_DIAGNOSTICS.ps1').read_text(encoding='utf-8')
        self.assertIn('cadcore_folded_sheet_metal', runner)
        self.assertIn('--output-junit', runner)
        self.assertIn('if($ctestExit -ne 0)', runner)
        self.assertNotIn('--exclude-regex', runner)

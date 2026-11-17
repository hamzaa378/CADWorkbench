"""Source integration assertions; full Qt/OCCT native validation requires Windows."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
class FoldedContourIntegration(unittest.TestCase):
    def test_native_builder_has_fail_closed_topology_guard(self):
        src=(ROOT/'src/core/SheetMetal.cpp').read_text(encoding="utf-8")
        self.assertIn('#include "FoldedContourTopology.h"',src)
        self.assertIn('detail::simpleClosedContour(contour,&crossedEdges)',src)
        self.assertLess(src.index('detail::simpleClosedContour'),src.index('*outline=std::move(result)'))
    def test_document_edit_and_creation_use_same_builder(self):
        src=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        self.assertIn('buildFoldedSheetOutline(flanges,width,thickness,bends,&outline,error)',src)
        self.assertIn('const TopoDS_Shape rebuilt=makeFoldedSheetSolid',src)
    def test_portable_and_native_cases_are_registered(self):
        portable=(ROOT/'scripts/verify_portable_consolidation.sh').read_text(encoding="utf-8")
        native=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding="utf-8")
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding="utf-8")
        self.assertIn('folded_contour_topology_portable_tests.cpp',portable)
        for text in ('Self-overlapping folded outline was accepted',
                     'A self-overlapping folded body was committed',
                     'A self-overlapping folded edit altered'):
            self.assertIn(text,native)
        self.assertIn('src/core/FoldedContourTopology.h',cmake)
        self.assertEqual(cmake.count('add_test(NAME '),130)

if __name__=='__main__':unittest.main()

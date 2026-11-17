"""Guard the Qt implicit-sharing iterator invalidation in folded commit."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CODE = (ROOT / "src/core/SceneDocument.cpp").read_text(encoding="utf-8")

class FoldedCommitDetachmentRegression(unittest.TestCase):
    def test_index_captured_before_cow_copy(self):
        start = CODE.index("bool SceneDocument::editFoldedSheetMetalWithDevelopments(")
        end = CODE.index("bool SceneDocument::loftRectangles(", start)
        body = CODE[start:end]
        self.assertIn("parts_.cbegin()", body)
        self.assertIn("const qsizetype sourceIndex=", body)
        self.assertIn("QVector<Part> proposed=parts_;", body)
        self.assertIn("proposed[sourceIndex]", body)
        self.assertLess(body.index("const qsizetype sourceIndex="),
                        body.index("QVector<Part> proposed=parts_;"))
        self.assertNotIn("std::distance(parts_.begin(),found)", body)
        self.assertNotIn("std::find_if(parts_.begin(),parts_.end()", body)

    def test_failure_and_undo_paths_preserved(self):
        body = CODE[CODE.index("bool SceneDocument::editFoldedSheetMetalWithDevelopments("):
                    CODE.index("bool SceneDocument::loftRectangles(")]
        self.assertIn("if(!propagateLinkedBodies(&proposed,error))return false;", body)
        self.assertIn("checkpoint();parts_=std::move(proposed);", body)
        self.assertIn('traceFolded("undo snapshot: before")', body)

if __name__ == '__main__':
    unittest.main()

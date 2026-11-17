"""Source guard for the tracked-edge container compile failure seen with MSVC/OCCT 8.0.1.

This checks source structure; it does not substitute for compiling under MSVC.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCENE = (ROOT / "src/core/SceneDocument.cpp").read_text(encoding="utf-8")


class TrackedEdgeMapDeclaration(unittest.TestCase):
    def test_tracked_edge_uses_declared_collection_type(self):
        self.assertIn("#include <NCollection_IndexedMap.hxx>", SCENE)
        self.assertIn("#include <TopTools_ShapeMapHasher.hxx>", SCENE)
        self.assertIn("NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> inputEdges;", SCENE)
        self.assertNotIn("TopTools_IndexedMapOfShape inputEdges;", SCENE)

    def test_container_is_checked_before_executing_modifier(self):
        begin = SCENE.index("const bool trackedEdge=!step.referenceSourceId.isEmpty()")
        end = SCENE.index("std::shared_ptr<BRepBuilderAPI_MakeShape> recordedModifier;", begin)
        section = SCENE[begin:end]
        self.assertIn("TopExp::MapShapes(input,TopAbs_EDGE,inputEdges);", section)
        self.assertIn("indexValue>inputEdges.Extent()", section)
        self.assertIn('if(error)*error="Tracked edge index is outside the current stage."', section)


if __name__ == "__main__":
    unittest.main()

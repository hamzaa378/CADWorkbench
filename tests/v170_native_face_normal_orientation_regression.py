"""Regression for the native face-normal drilling test's selected-face sense.

The analytic gp_Pln direction is not necessarily a face's physical outward
normal.  TopAbs_REVERSED flips it.  This was previously inverted in the native
fixture, causing the wrong face to be selected despite correct production logic.
"""
import re
import unittest
from pathlib import Path

from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepAdaptor import BRepAdaptor_Surface
from OCP.GeomAbs import GeomAbs_Plane
from OCP.TopoDS import TopoDS
from OCP.TopExp import TopExp_Explorer
from OCP.TopAbs import TopAbs_FACE, TopAbs_REVERSED

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / "tests" / "associative_features_v159_tests.cpp"
CORE = ROOT / "src" / "core" / "SceneDocument.cpp"


def planar_faces():
    shape = BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape()
    explorer = TopExp_Explorer(shape, TopAbs_FACE)
    while explorer.More():
        face = TopoDS.Face_s(explorer.Current())
        surface = BRepAdaptor_Surface(face)
        if surface.GetType() == GeomAbs_Plane:
            analytic = surface.Plane().Axis().Direction()
            orient = -1 if face.Orientation() == TopAbs_REVERSED else 1
            yield (orient * analytic.X(), orient * analytic.Y(), orient * analytic.Z())
        explorer.Next()


class FaceNormalSelectionRegression(unittest.TestCase):
    def test_positive_x_outward_is_negative_x_inward(self):
        normals = list(planar_faces())
        self.assertEqual(len(normals), 6)
        positive_x = [n for n in normals if n[0] > 0.99]
        self.assertEqual(len(positive_x), 1)
        self.assertLess(-positive_x[0][0], -0.99)

    def test_native_fixture_uses_physical_outward_orientation(self):
        source = NATIVE.read_text(encoding="utf-8")
        self.assertRegex(source,
            r"if\(candidate\.Orientation\(\)==TopAbs_REVERSED\)outward\.Reverse\(\);")
        self.assertIn('check(axisHole.inwardAxis.X()<-0.99,', source)
        self.assertNotIn('if(candidate.Orientation()!=TopAbs_REVERSED)outward.Reverse();',source)

    def test_production_hole_direction_keeps_inward_semantics(self):
        source = CORE.read_text(encoding="utf-8")
        start=source.index('bool SceneDocument::prepareHoleOnPlanarFace(')
        end=source.index('bool SceneDocument::prepareCounterbore(',start)
        section=source[start:end]
        self.assertIn('if(face.Orientation()!=TopAbs_REVERSED)inward.Reverse();', section)
        self.assertIn('command->inwardAxis=inward;',section)

if __name__ == '__main__':
    unittest.main()

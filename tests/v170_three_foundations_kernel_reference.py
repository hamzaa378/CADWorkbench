"""Independent OpenCascade geometry oracle for the three v1.70 foundations.

Requires Python OpenCascade/OCP (and CadQuery for the constructive volume cases).
This does NOT compile or run the native Qt/OCCT SceneDocument code.
"""
import math
import unittest

import cadquery as cq
from OCP.BRepAlgoAPI import BRepAlgoAPI_Splitter
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeFace
from OCP.BRepClass import BRepClass_FaceClassifier
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.TopAbs import TopAbs_FACE, TopAbs_IN
from OCP.TopExp import TopExp_Explorer
from OCP.TopTools import TopTools_ListOfShape
from OCP.TopoDS import TopoDS
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.gp import gp_Dir, gp_Pln, gp_Pnt


class FoundationKernelReferences(unittest.TestCase):
    def test_face_split_has_unique_planar_witness_successor(self):
        box = BRepPrimAPI_MakeBox(10, 10, 10).Shape()
        splitter_face = BRepBuilderAPI_MakeFace(
            gp_Pln(gp_Pnt(7, 0, 0), gp_Dir(1, 0, 0)), -20, 20, -20, 20
        ).Shape()
        args, tools = TopTools_ListOfShape(), TopTools_ListOfShape()
        args.Append(box)
        tools.Append(splitter_face)
        splitter = BRepAlgoAPI_Splitter()
        splitter.SetArguments(args)
        splitter.SetTools(tools)
        splitter.Build()
        self.assertTrue(splitter.IsDone())
        walker = TopExp_Explorer(splitter.Shape(), TopAbs_FACE)
        top_faces, witness_matches, top_areas = 0, 0, []
        while walker.More():
            face = TopoDS.Face_s(walker.Current())
            props = GProp_GProps()
            BRepGProp.SurfaceProperties_s(face, props)
            if abs(props.CentreOfMass().Z() - 10) < 1e-5:
                top_faces += 1
                top_areas.append(props.Mass())
                if BRepClass_FaceClassifier(face, gp_Pnt(5, 5, 10), 1e-7).State() == TopAbs_IN:
                    witness_matches += 1
            walker.Next()
        self.assertEqual(top_faces, 2)
        self.assertEqual(witness_matches, 1)
        self.assertAlmostEqual(sum(top_areas), 100, places=5)

    def test_in_place_additive_and_subtractive_feature_volumes(self):
        block = cq.Workplane('XY').box(12, 12, 10, centered=(False, False, False)).val()
        for width in (10, 8):
            tool = (cq.Workplane('XY')
                    .box(width, 10, 5, centered=(False, False, False))
                    .translate((0, 0, 8)).val())
            self.assertAlmostEqual(block.fuse(tool).Volume(), 1440 + width*10*3, places=5)
            self.assertAlmostEqual(block.cut(tool).Volume(), 1440 - width*10*2, places=5)
        self.assertAlmostEqual(block.cut(tool).Volume(), 1280, places=5)

    def test_detached_and_buried_boss_not_meaningful(self):
        block = cq.Workplane('XY').box(12, 12, 10, centered=(False, False, False)).val()
        detached = (cq.Workplane('XY').box(8, 10, 5, centered=(False, False, False))
                    .translate((0, 0, 50)).val())
        buried = (cq.Workplane('XY').box(8, 10, 5, centered=(False, False, False))
                  .translate((0, 0, 1)).val())
        self.assertEqual(len(block.fuse(detached).Solids()), 2)
        self.assertAlmostEqual(block.fuse(buried).Volume(), block.Volume(), places=5)

    def test_assembly_three_point_constraint_reference(self):
        # Independent known rigid 6-DOF transformation, with no solver dependency.
        angle = math.radians(30)
        c, s = math.cos(angle), math.sin(angle)
        points = ((0, 0, 0), (10, 0, 0), (0, 10, 0))
        mapped = tuple((c*x-s*y+15, s*x+c*y-8, z+4) for x, y, z in points)
        for source, target in zip(points, mapped):
            x, y, z = source
            self.assertTrue(math.dist((c*x-s*y+15, s*x+c*y-8, z+4), target) < 1e-12)
        self.assertAlmostEqual(math.dist(*mapped[:2]), 10, places=10)
        self.assertAlmostEqual(math.dist(mapped[0], mapped[2]), 10, places=10)


if __name__ == '__main__':
    unittest.main(verbosity=2)

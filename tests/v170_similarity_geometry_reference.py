"""Independent OpenCascade similarity/stock-cut reference fixtures.
These exercise the reference kernel, not the Windows Qt/C++ implementation.
"""
import math
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Trsf, gp_Pnt, gp_Vec, gp_Ax2, gp_Dir


def volume(shape):
    props=GProp_GProps()
    BRepGProp.VolumeProperties_s(shape,props)
    return props.Mass()

class IndependentScaleReference(unittest.TestCase):
    def test_scaled_stock_volume_and_fixed_tool_radius(self):
        original=BRepPrimAPI_MakeBox(10,10,10).Shape()
        scale=gp_Trsf();scale.SetScale(gp_Pnt(0,0,0),2.0)
        enlarged=BRepBuilderAPI_Transform(original,scale,True).Shape()
        self.assertAlmostEqual(volume(enlarged),8*volume(original),places=4)
        # The subsequent bore is specified in actual millimetres, not in the
        # upstream body's pre-scale coordinate system.
        tool=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(10,10,-1),gp_Dir(0,0,1)),1,22).Shape()
        cut=BRepAlgoAPI_Cut(enlarged,tool);cut.Build()
        self.assertTrue(cut.IsDone())
        self.assertAlmostEqual(volume(enlarged)-volume(cut.Shape()),math.pi*20,places=3)
    def test_exact_design_witness_transform_and_inverse(self):
        forward=gp_Trsf();forward.SetScale(gp_Pnt(0,0,0),1.5)
        back=gp_Trsf();back.SetScale(gp_Pnt(0,0,0),1/1.5)
        p=gp_Pnt(4.2,-1.1,8.5);q=p.Transformed(forward)
        self.assertAlmostEqual(q.Distance(gp_Pnt(6.3,-1.65,12.75)),0,places=9)
        self.assertLess(p.Distance(q.Transformed(back)),1e-9)
    def test_exact_mirror_geometry_and_reverse_witness(self):
        stock=BRepPrimAPI_MakeBox(10,12,8).Shape()
        reflection=gp_Trsf()
        reflection.SetMirror(gp_Ax2(gp_Pnt(0,0,0),gp_Dir(0,0,1)))
        mirrored=BRepBuilderAPI_Transform(stock,reflection,True).Shape()
        self.assertAlmostEqual(volume(stock),volume(mirrored),places=5)
        witness=gp_Pnt(5,6,8)
        first=witness.Transformed(reflection)
        self.assertAlmostEqual(first.Z(),-8,places=7)
        self.assertLess(first.Transformed(reflection).Distance(witness),1e-9)
    def test_curvature_and_area_units(self):
        r=3.2;s=1.75
        self.assertAlmostEqual((1/(r*s)),(1/r)/s,places=12)
        self.assertAlmostEqual((9*11)*s*s, (9*s)*(11*s),places=12)

if __name__=='__main__':unittest.main()

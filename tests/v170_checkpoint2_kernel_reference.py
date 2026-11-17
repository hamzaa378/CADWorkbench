"""Independent OpenCascade geometry fixture for checkpoint 2.

This validates the physical Boolean trims, not CADWorkbench's C++
feature-reference rebasing or the Windows native regression test.
"""
import unittest
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.BRepAdaptor import BRepAdaptor_Curve
from OCP.TopAbs import TopAbs_EDGE
from OCP.TopExp import TopExp_Explorer
from OCP.TopoDS import TopoDS
from OCP.gp import gp_Pnt


def volume(shape):
    result=GProp_GProps()
    BRepGProp.VolumeProperties_s(shape,result)
    return result.Mass()


def bottom_edge_midpoints(shape):
    cursor=TopExp_Explorer(shape,TopAbs_EDGE)
    midpoints=[]
    while cursor.More():
        curve=BRepAdaptor_Curve(TopoDS.Edge_s(cursor.Current()))
        at=curve.Value((curve.FirstParameter()+curve.LastParameter())/2)
        if abs(at.Y())<1e-8 and abs(at.Z())<1e-8:
            midpoints.append(at.X())
        cursor.Next()
    return midpoints


class TrackedEdgeTrimReference(unittest.TestCase):
    def test_successive_boolean_trims_keep_original_point(self):
        base=BRepPrimAPI_MakeBox(10,10,10).Shape()
        first=BRepAlgoAPI_Cut(base,
            BRepPrimAPI_MakeBox(gp_Pnt(7,-1,-1),4,3,3).Shape())
        first.Build()
        self.assertTrue(first.IsDone())
        self.assertTrue(BRepCheck_Analyzer(first.Shape()).IsValid())
        second=BRepAlgoAPI_Cut(first.Shape(),
            BRepPrimAPI_MakeBox(gp_Pnt(-1,-1,-1),4,3,3).Shape())
        second.Build()
        self.assertTrue(second.IsDone())
        self.assertTrue(BRepCheck_Analyzer(second.Shape()).IsValid())
        self.assertAlmostEqual(volume(base),1000.0,places=6)
        self.assertAlmostEqual(volume(first.Shape()),988.0,places=6)
        self.assertAlmostEqual(volume(second.Shape()),976.0,places=6)
        # The first cut shifts the descendant midpoint to x=3.5.
        # The next cut brings it back to x=5; an anchor which followed the
        # midpoint would drift in the intermediate stage.
        self.assertTrue(any(abs(x-3.5)<1e-6 for x in bottom_edge_midpoints(first.Shape())))
        self.assertTrue(any(abs(x-5.0)<1e-6 for x in bottom_edge_midpoints(second.Shape())))

if __name__=='__main__':unittest.main()

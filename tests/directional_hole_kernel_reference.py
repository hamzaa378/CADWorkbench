"""Independent OCCT reference checks for shape-independent, directional cylinders.

These check geometry and volume only; they do not replace the native Qt/C++
feature-tree and linked-body integration test on Windows.
"""
import math
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Pnt, gp_Dir, gp_Ax2


def volume(shape):
    props=GProp_GProps()
    BRepGProp.VolumeProperties_s(shape,props)
    return props.Mass()


def cut_cylinder(shape, entry, axis, diameter, depth):
    cylinder=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(*entry),gp_Dir(*axis)),diameter/2,depth).Shape()
    cutter=BRepAlgoAPI_Cut(shape,cylinder)
    cutter.Build()
    if not cutter.IsDone():
        raise RuntimeError('OCCT cutter failed')
    result=cutter.Shape()
    if not BRepCheck_Analyzer(result).IsValid():
        raise RuntimeError('OCCT produced invalid B-rep')
    return result


class DirectionalGeometryReferences(unittest.TestCase):
    def setUp(self):
        self.box=BRepPrimAPI_MakeBox(20.0,20.0,10.0).Shape()

    def test_face_normal_entire_thickness(self):
        shape=cut_cylinder(self.box,(20.01,10,5),(-1,0,0),4,20.02)
        self.assertAlmostEqual(volume(self.box)-volume(shape),80*math.pi,places=5)

    def test_arbitrary_axis_blind_depth_is_editable(self):
        shallow=cut_cylinder(self.box,(20,10,5),(-1,0,0),4,8)
        deep=cut_cylinder(self.box,(20,10,5),(-1,0,0),4,14)
        self.assertAlmostEqual(volume(self.box)-volume(shallow),32*math.pi,places=5)
        self.assertAlmostEqual(volume(self.box)-volume(deep),56*math.pi,places=5)

    def test_changed_source_changes_boolean_descendant(self):
        second=BRepPrimAPI_MakeBox(3,3,3).Shape()
        original=BRepAlgoAPI_Fuse(self.box,second)
        original.Build()
        modified=cut_cylinder(self.box,(20,10,5),(-1,0,0),4,14)
        regenerated=BRepAlgoAPI_Fuse(modified,second)
        regenerated.Build()
        self.assertTrue(original.IsDone())
        self.assertTrue(regenerated.IsDone())
        self.assertTrue(BRepCheck_Analyzer(regenerated.Shape()).IsValid())
        self.assertGreater(volume(original.Shape())-volume(regenerated.Shape()),1.0)

    def test_external_axis_miss_preserves_geometry(self):
        missed=cut_cylinder(self.box,(20,100,5),(-1,0,0),4,25)
        self.assertAlmostEqual(volume(missed),volume(self.box),places=6)

if __name__=='__main__':unittest.main()

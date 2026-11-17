"""Independent OpenCascade geometric preconditions for a fan-out rollback test.

These validate the intended scenario, NOT SceneDocument transaction semantics.
The latter is exercised in the Windows-native interleaved-history CTest.
"""
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Pnt


def mass(s):
    props=GProp_GProps()
    BRepGProp.VolumeProperties_s(s,props)
    return props.Mass()


def builder(cls,first,second):
    op=cls(first,second)
    op.Build()
    assert op.IsDone()
    return op.Shape()


class FanOutRollbackGeometry(unittest.TestCase):
    def setUp(self):
        self.add_target=BRepPrimAPI_MakeBox(12,12,10).Shape()
        self.cut_target=BRepPrimAPI_MakeBox(4,12,10).Shape()

    def cutter(self,x,width=10):
        return BRepPrimAPI_MakeBox(gp_Pnt(x,0,8),width,10,5).Shape()

    def test_old_sketch_affects_both_targets(self):
        initial=self.cutter(0)
        additive=builder(BRepAlgoAPI_Fuse,self.add_target,initial)
        subtractive=builder(BRepAlgoAPI_Cut,self.cut_target,initial)
        self.assertGreater(mass(additive),mass(self.add_target)+1)
        self.assertLess(mass(subtractive),mass(self.cut_target)-1)
        self.assertAlmostEqual(mass(additive),1740,places=3)
        self.assertAlmostEqual(mass(subtractive),400,places=3)

    def test_shift_keeps_one_branch_valid_and_breaks_other(self):
        shifted=self.cutter(8)
        additive=builder(BRepAlgoAPI_Fuse,self.add_target,shifted)
        subtractive=builder(BRepAlgoAPI_Cut,self.cut_target,shifted)
        self.assertGreater(mass(additive),mass(self.add_target)+1)
        self.assertAlmostEqual(mass(subtractive),mass(self.cut_target),places=5)

    def test_valid_width_change_affects_both_branches(self):
        narrowed=self.cutter(0,8)
        additive=builder(BRepAlgoAPI_Fuse,self.add_target,narrowed)
        subtractive=builder(BRepAlgoAPI_Cut,self.cut_target,narrowed)
        self.assertGreater(mass(additive),mass(self.add_target)+1)
        self.assertLess(mass(subtractive),mass(self.cut_target)-1)

if __name__=='__main__':
    unittest.main()

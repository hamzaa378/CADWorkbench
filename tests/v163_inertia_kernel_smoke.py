"""Optional independent OpenCascade Python-binding smoke test for inertias.

Requires pip-installed OCP; deliberately excluded from native CTest. This checks
OCCT's unit-density volume/moment conventions, not the Qt application.
"""
import math
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Trsf, gp_Vec
from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform

class OpenCascadeInertiaReference(unittest.TestCase):
    def measure(self,shape):
        values=GProp_GProps()
        BRepGProp.VolumeProperties_s(shape,values)
        return values
    def test_box_volume_center_and_inertia(self):
        props=self.measure(BRepPrimAPI_MakeBox(10.,20.,30.).Shape())
        self.assertAlmostEqual(props.Mass(),6000.0)
        for actual,expected in zip((props.CentreOfMass().X(),props.CentreOfMass().Y(),
                                    props.CentreOfMass().Z()),(5.,10.,15.)):
            self.assertAlmostEqual(actual,expected,places=8)
        matrix=props.MatrixOfInertia()
        for k,expected in enumerate((650000.,500000.,250000.),1):
            self.assertAlmostEqual(matrix.Value(k,k),expected,places=6)
        for i in range(1,4):
            for j in range(1,4):
                if i!=j:self.assertAlmostEqual(matrix.Value(i,j),0.,places=7)
        # Convert geometry inertia mm^5 into kg*mm^2 for assigned steel density.
        factor=7850.0/1e9
        self.assertAlmostEqual(props.Mass()*factor,0.0471,places=8)
        self.assertAlmostEqual(matrix.Value(3,3)*factor,1.9625,places=8)
    def test_translation_preserves_central_inertia(self):
        shape=BRepPrimAPI_MakeBox(10.,20.,30.).Shape()
        original=self.measure(shape)
        transform=gp_Trsf();transform.SetTranslation(gp_Vec(100.,-50.,25.))
        moved=BRepBuilderAPI_Transform(shape,transform,True).Shape()
        shifted=self.measure(moved)
        self.assertAlmostEqual(shifted.CentreOfMass().X(),105.0,places=7)
        self.assertAlmostEqual(shifted.CentreOfMass().Y(),-40.0,places=7)
        self.assertAlmostEqual(shifted.CentreOfMass().Z(),40.0,places=7)
        for i in range(1,4):
            for j in range(1,4):
                self.assertTrue(math.isclose(shifted.MatrixOfInertia().Value(i,j),
                                             original.MatrixOfInertia().Value(i,j),
                                             abs_tol=1e-5,rel_tol=1e-10))
if __name__=='__main__':unittest.main(verbosity=2)

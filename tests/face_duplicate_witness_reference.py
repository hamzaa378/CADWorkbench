"""Independent OCCT fixture proving competing planar witness placements exist.

Does not substitute for running the native resolver on Windows.
"""
import unittest

try:
    from OCP.BRep import BRep_Builder
    from OCP.TopoDS import TopoDS_Compound
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
    from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.gp import gp_Trsf, gp_Vec
except ImportError:
    BRep_Builder = None


@unittest.skipIf(BRep_Builder is None, "OCCT Python reference kernel unavailable")
class CompetingPlanarWitnessReference(unittest.TestCase):
    def test_three_copies_create_conflicting_unmoved_and_shifted_witness_locations(self):
        box = BRepPrimAPI_MakeBox(10, 10, 10).Shape()
        result = TopoDS_Compound()
        builder = BRep_Builder()
        builder.MakeCompound(result)
        builder.Add(result, box)
        for shift_x in (20.0, 40.0):
            transform = gp_Trsf()
            transform.SetTranslation(gp_Vec(shift_x, 0, 0))
            builder.Add(result, BRepBuilderAPI_Transform(box, transform, True).Shape())
        props = GProp_GProps()
        BRepGProp.VolumeProperties_s(result, props)
        self.assertAlmostEqual(props.Mass(), 3000, places=5)
        self.assertAlmostEqual(props.CentreOfMass().X(), 25, places=5)
        # The original top-face witness (5,5,10) still exists, but the
        # shifted witness (25,5,10) falls on a different copied top face.
        self.assertTrue(0 < 5 < 10)
        self.assertTrue(20 < 25 < 30)


if __name__ == "__main__":
    unittest.main()

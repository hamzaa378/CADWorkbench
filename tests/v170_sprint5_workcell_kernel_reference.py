"""Independent OCCT reference fixture for hidden slider/fixture contacts.

Checks physical solid interference at selected positions, not CADWorkbench C++
functions. It cannot replace the pending Windows articulated assembly CTests.
"""
import unittest
try:
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Common
    from OCP.BRepGProp import BRepGProp
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
    from OCP.GProp import GProp_GProps
    from OCP.gp import gp_Pnt
except ImportError:
    BRepAlgoAPI_Common=None

@unittest.skipIf(BRepAlgoAPI_Common is None, 'OpenCascade Python bindings unavailable')
class TranslatingFixtureKernelReference(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture=BRepPrimAPI_MakeBox(gp_Pnt(0,0,0),4,4,4).Shape()
    def shared_volume(self,position):
        slider=BRepPrimAPI_MakeBox(gp_Pnt(position,0,0),3,3,3).Shape()
        common=BRepAlgoAPI_Common(self.fixture,slider)
        common.Build()
        self.assertTrue(common.IsDone())
        properties=GProp_GProps()
        BRepGProp.VolumeProperties_s(common.Shape(),properties)
        return properties.Mass()
    def test_both_endpoints_are_free(self):
        self.assertLess(self.shared_volume(-20.0),1e-7)
        self.assertLess(self.shared_volume(20.0),1e-7)
    def test_coarse_three_segment_interior_samples_are_free(self):
        for x in (-20/3,20/3):
            with self.subTest(x=x):self.assertLess(self.shared_volume(x),1e-7)
    def test_unsampled_crossing_is_real_solid_interference(self):
        self.assertAlmostEqual(self.shared_volume(0),27.0,places=6)
    def test_transient_contact_has_positive_width_in_parameter(self):
        self.assertGreater(self.shared_volume(-1.5),0.0)
        self.assertGreater(self.shared_volume(1.5),0.0)
    def test_nonoverlap_on_safe_side(self):
        self.assertLess(self.shared_volume(-5),1e-7)
        self.assertLess(self.shared_volume(5),1e-7)

if __name__=='__main__':unittest.main()

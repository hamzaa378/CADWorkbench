"""Independent OCCT checks and source-path contracts for editable face fastener holes.

These are reference geometry checks, not a substitute for Windows MSVC CTests.
"""
import math
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
TYPES=('Face through-all hole','Face counterbore through all','Face countersink through all')

class FastenerFeatureContracts(unittest.TestCase):
    def test_rebuild_and_saved_parameters_wired(self):
        shape=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text()
        gui=(ROOT/'src/ui/MainWindow.cpp').read_text()
        native=(ROOT/'tests/associative_features_v159_tests.cpp').read_text()
        for name in TYPES:
            with self.subTest(operation=name):
                self.assertIn(f'"{name}"',shape)
                self.assertIn(f'"{name}"',gui)
                self.assertIn(name,native)
        self.assertIn('recessProps.Mass()>=pilotProps.Mass()',shape)
        self.assertIn('BRepAlgoAPI_Fuse combined(tool,recess)',shape)
        self.assertIn('cad::directional_holes::throughAllSpan(box,',shape)
        self.assertIn('Face fastener drilling failed.',shape)
        self.assertIn('Fastener recess radius must exceed',shape)
        self.assertIn('cannot',shape.lower())

    def test_new_nodes_use_existing_transaction_and_file(self):
        shape=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text()
        native=(ROOT/'tests/associative_features_v159_tests.cpp').read_text()
        self.assertIn('if(!geometryOkay(next,surface,error))',shape)
        self.assertIn('Saved counterbore lost its dimensions',native)
        self.assertIn('Failed counterbore operation changed committed stock',native)
        self.assertIn('Face counterbore did not follow translated source face',native)
        self.assertIn('Countersink conical head removal is incorrect',native)

try:
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox,BRepPrimAPI_MakeCylinder,BRepPrimAPI_MakeCone
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse,BRepAlgoAPI_Cut
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.gp import gp_Pnt,gp_Dir,gp_Ax2
except ImportError:
    BRepPrimAPI_MakeBox=None

@unittest.skipIf(BRepPrimAPI_MakeBox is None,'OCP not installed')
class PhysicalFastenerReferences(unittest.TestCase):
    @staticmethod
    def volume(shape):
        mass=GProp_GProps();BRepGProp.VolumeProperties_s(shape,mass)
        return mass.Mass()

    def cut(self,shape,thickness,head_radius=None,depth=None,angle=None,z_shift=0):
        # Geometry is independent of the C++ path: explicit coaxial cutters.
        axis=gp_Dir(0,0,-1)
        at=lambda z: gp_Ax2(gp_Pnt(10,10,z+z_shift),axis)
        tool=BRepPrimAPI_MakeCylinder(at(thickness+.02),2,thickness+.04).Shape()
        if head_radius is not None:
            if angle is None:
                recess=BRepPrimAPI_MakeCylinder(at(thickness+.01),head_radius,depth+.01).Shape()
            else:
                slope=math.tan(math.radians(angle/2))
                recess_depth=(head_radius-2)/slope
                recess=BRepPrimAPI_MakeCone(at(thickness+.01),head_radius+.01*slope,2,
                                            recess_depth+.01).Shape()
            fuse=BRepAlgoAPI_Fuse(tool,recess);fuse.Build()
            self.assertTrue(fuse.IsDone());self.assertTrue(BRepCheck_Analyzer(fuse.Shape()).IsValid())
            tool=fuse.Shape()
        cut=BRepAlgoAPI_Cut(shape,tool);cut.Build()
        self.assertTrue(cut.IsDone());self.assertTrue(BRepCheck_Analyzer(cut.Shape()).IsValid())
        return self.volume(cut.Shape())

    def test_plain_through_all_and_counterbore(self):
        for thickness in (10,15,20):
            with self.subTest(thickness=thickness):
                stock=BRepPrimAPI_MakeBox(20,20,thickness).Shape()
                bore=self.cut(stock,thickness)
                cb=self.cut(stock,thickness,head_radius=4,depth=3)
                self.assertAlmostEqual(bore,400*thickness-4*math.pi*thickness,delta=1e-3)
                self.assertAlmostEqual(cb,400*thickness-
                    math.pi*(4*thickness+36),delta=1e-3)

    def test_countersink_90_degrees_and_deeper_82_degree_head(self):
        stock=BRepPrimAPI_MakeBox(20,20,10).Shape()
        for included in (82,90,100):
            with self.subTest(included=included):
                volume=self.cut(stock,10,head_radius=4,angle=included)
                depth=(4-2)/math.tan(math.radians(included/2))
                extra=math.pi*depth*(4*4+4*2-2*2*2)/3
                self.assertAlmostEqual(volume,4000-40*math.pi-extra,delta=1e-3)

    def test_translated_stock_keeps_face_entry(self):
        # This verifies the intended local face-frame calculation when a
        # face witness is translated by the same vector as the stock.
        from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
        from OCP.gp import gp_Trsf,gp_Vec
        t=gp_Trsf();t.SetTranslation(gp_Vec(0,0,5))
        stock=BRepBuilderAPI_Transform(BRepPrimAPI_MakeBox(20,20,10).Shape(),t).Shape()
        actual=self.cut(stock,10,head_radius=4,depth=3,z_shift=5)
        self.assertAlmostEqual(actual,4000-76*math.pi,delta=1e-3)

if __name__=='__main__': unittest.main()

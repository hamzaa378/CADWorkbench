"""Independent OCCT geometry references and source-contract checks for
saved face-local hole grids and bolt-circle drilling.

These are not a replacement for compiling the actual CADWorkbench C++ code
under Windows and executing all 130 native CTests.
"""
import math
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HolePatternSourceContracts(unittest.TestCase):
    def test_saved_ui_kernel_and_lineage_paths(self):
        core=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text()
        gui=(ROOT/'src/ui/MainWindow.cpp').read_text()
        lineage=(ROOT/'src/core/BooleanReferenceLineage.cpp').read_text()
        native=(ROOT/'tests/associative_features_v159_tests.cpp').read_text()
        for item in ('Face hole grid through all','Face bolt circle through all'):
            with self.subTest(item=item):
                self.assertIn('"'+item+'"',core)
                self.assertIn('"'+item+'"',gui)
                self.assertIn('"'+item+'"',native)
                self.assertIn('"'+item+'"',lineage)
        self.assertIn('hole_patterns::grid(',core)
        self.assertIn('hole_patterns::boltCircle(',core)
        self.assertIn('BRep_Builder builder;',core)
        self.assertIn('auto cut=std::make_shared<BRepAlgoAPI_Cut>(input,cutters)',core)
        self.assertIn('producedHistory=cut;',core)
        self.assertIn('witnessOnFace(chosen,edge,1e-6)',core)
        self.assertIn('A hole-pattern center has left the selected face.',core)
        self.assertIn('pattern removes no material.',core)
        self.assertIn('manufacturing_hole_patterns_portable_tests.cpp', (ROOT/'tools/run_portable_regressions.py').read_text())
        self.assertIn('Find existing features by name, parameter or ID',gui)
        self.assertIn('applyFeatureFilter()',gui)

try:
    from OCP.BRep import BRep_Builder
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepGProp import BRepGProp
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
    from OCP.GProp import GProp_GProps
    from OCP.TopoDS import TopoDS_Compound
    from OCP.gp import gp_Ax2,gp_Dir,gp_Pnt
except ImportError:
    BRepPrimAPI_MakeBox=None


@unittest.skipIf(BRepPrimAPI_MakeBox is None,'OpenCascade reference module unavailable')
class PhysicalHolePatternReferences(unittest.TestCase):
    @staticmethod
    def volume(shape):
        props=GProp_GProps();BRepGProp.VolumeProperties_s(shape,props)
        return props.Mass()

    def cut_pattern(self, stock, thickness, centers, radius, top=10.0):
        builder=BRep_Builder();compound=TopoDS_Compound();builder.MakeCompound(compound)
        for u,v in centers:
            cutter=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(u,v,top+.1),gp_Dir(0,0,-1)), radius, thickness+.2).Shape()
            builder.Add(compound,cutter)
        cut=BRepAlgoAPI_Cut(stock,compound);cut.Build()
        self.assertTrue(cut.IsDone())
        self.assertTrue(BRepCheck_Analyzer(cut.Shape()).IsValid())
        return cut.Shape()

    def test_grid_stock_thickness_and_rotation(self):
        for z in (10,18):
            stock=BRepPrimAPI_MakeBox(40,40,z).Shape()
            for columns,rows,pitch,angle in ((2,2,12,0),(3,2,10,30),(2,3,10,90)):
                with self.subTest(z=z,cols=columns,rows=rows,angle=angle):
                    a=math.radians(angle)
                    centers=[]
                    for j in range(rows):
                        for i in range(columns):
                            du=(i-(columns-1)/2)*pitch
                            dv=(j-(rows-1)/2)*pitch
                            centers.append((20+du*math.cos(a)-dv*math.sin(a),
                                            20+du*math.sin(a)+dv*math.cos(a)))
                    result=self.cut_pattern(stock,z,centers,1.5,top=z)
                    expected=40*40*z - columns*rows*math.pi*1.5**2*z
                    self.assertAlmostEqual(self.volume(result),expected,places=3)

    def test_bolt_circle_pcd_and_angle(self):
        stock=BRepPrimAPI_MakeBox(40,40,10).Shape()
        for n,pcd,first_angle in ((4,22,0),(6,22,30),(8,26,-45)):
            with self.subTest(n=n,pcd=pcd):
                centers=[(20+0.5*pcd*math.cos(math.radians(first_angle+k*360/n)),
                          20+0.5*pcd*math.sin(math.radians(first_angle+k*360/n)))
                         for k in range(n)]
                result=self.cut_pattern(stock,10,centers,1,top=10)
                self.assertAlmostEqual(self.volume(result),16000-n*10*math.pi,places=3)

    def test_round_blank_can_be_drilled(self):
        stock=BRepPrimAPI_MakeCylinder(25,10).Shape()
        centers=[(10*math.cos(math.radians(a)),10*math.sin(math.radians(a)))
                 for a in (0,60,120,180,240,300)]
        result=self.cut_pattern(stock,10,centers,1.25,top=10)
        self.assertAlmostEqual(self.volume(result),math.pi*25**2*10-6*math.pi*1.25**2*10,places=3)


if __name__=='__main__':
    unittest.main()

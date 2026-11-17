"""Contracts and independent B-rep references for the November 10 increment.

Source contracts cannot substitute for an MSVC/Qt native build. Geometry checks
execute the *reference* OpenCascade kernel, not the C++ GUI implementation.
"""
import math
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

def source(path):
    return (ROOT/path).read_text(encoding='utf-8')

class ThroughAllHistoryWiring(unittest.TestCase):
    def test_node_registered_with_no_fixed_depth(self):
        text=source('src/core/ShapeFeatureTree.cpp')
        self.assertIn('{"Cylinder through all arbitrary axis",{"x","y","z","axis_x","axis_y","axis_z","radius"}}',text)
        self.assertIn('cad::directional_holes::throughAllSpan',text)
        self.assertIn('BRepBndLib::Add(input,bounds)',text)
        self.assertIn('cutterOrigin.Translate(gp_Vec(direction).Multiplied(span.start))',text)
        self.assertIn('cutterDepth=span.length',text)
        self.assertIn('if(after.Mass()>=before.Mass()',text)

    def test_explicit_numeric_validation_and_transaction_guard(self):
        text=source('src/core/ShapeFeatureTree.cpp')
        self.assertIn('node.type=="Cylinder through all arbitrary axis")&&',text)
        self.assertIn('lengthSquared("axis_x","axis_y","axis_z")<1e-12',text)
        self.assertIn('Feature parameter names do not match the operation.',text)
        native=source('tests/associative_features_v159_tests.cpp')
        for clause in ('Through-all original box bore volume incorrect',
                       'Through-all did not extend to the new scaled thickness',
                       'Second upstream scale failed to regenerate through-all cutter',
                       'Rejected through-all edit changed source or linked child',
                       'Through-all feature was stale after save/reopen'):
            self.assertIn(clause,native)

    def test_ui_search_and_controls(self):
        ui=source('src/ui/MainWindow.cpp')
        self.assertIn('Cylinder through all arbitrary axis',ui)
        self.assertIn('picker.setWindowTitle("Search geometry features")',ui)
        self.assertIn('choice.contains(term,Qt::CaseInsensitive)',ui)
        self.assertIn('if(matches->currentItem())picker.accept();',ui)

    def test_two_other_workstreams_wired_to_engineering_dialogs(self):
        ui=source('src/ui/MainWindow.cpp')
        self.assertIn('Fixed-frame isolation if a critical connection is removed',ui)
        self.assertIn('impact.detachedFromFixed',ui)
        self.assertIn('Straight neutral flange intervals on the developed blank',ui)
        self.assertIn('audit.flatFlanges',ui)
        self.assertIn('result.bridgeImpacts.push_back',source('src/core/AssemblyNetworkStructureAudit.h'))

try:
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
    from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
    from OCP.BRepGProp import BRepGProp
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.GProp import GProp_GProps
    from OCP.gp import gp_Pnt, gp_Ax2, gp_Dir, gp_Trsf, gp_Vec
except ImportError:
    BRepAlgoAPI_Cut=None

@unittest.skipIf(BRepAlgoAPI_Cut is None,'Independent OpenCascade reference kernel not available')
class ReferenceGeometry(unittest.TestCase):
    @staticmethod
    def volume(solid):
        props=GProp_GProps()
        BRepGProp.VolumeProperties_s(solid,props)
        return props.Mass()

    def reference_cut(self,base,extent,offset=0):
        # Independent reference: full bbox X span across the current scaled box.
        cutter=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(-.02,10+offset,5),gp_Dir(1,0,0)),2,extent+.04).Shape()
        result=BRepAlgoAPI_Cut(base,cutter)
        result.Build()
        self.assertTrue(result.IsDone())
        self.assertTrue(BRepCheck_Analyzer(result.Shape()).IsValid())
        return result.Shape()

    def test_through_hole_regenerates_on_stretched_stock(self):
        for x,y,z in ((20,20,10),(30,30,15),(40,40,20)):
            with self.subTest(width=x):
                stock=BRepPrimAPI_MakeBox(x,y,z).Shape()
                cut=self.reference_cut(stock,x)
                self.assertAlmostEqual(self.volume(cut),x*y*z-math.pi*4*x,delta=1e-3)

    def test_missed_through_all_cut_does_not_change_stock(self):
        stock=BRepPrimAPI_MakeBox(30,30,15).Shape()
        miss=self.reference_cut(stock,30,offset=990)
        self.assertAlmostEqual(self.volume(stock),self.volume(miss),delta=1e-5)

    def test_through_all_on_rotated_non_axis_aligned_solid(self):
        stock=BRepPrimAPI_MakeBox(30,20,10).Shape()
        tr=gp_Trsf();tr.SetRotation(gp_Ax2(gp_Pnt(0,0,0),gp_Dir(0,0,1)).Axis(),math.pi/4)
        rotated=BRepBuilderAPI_Transform(stock,tr,True).Shape()
        # Projected span across a rotated body, with an arbitrary world-space
        # X direction. This specifically is NOT the original box X depth.
        from OCP.BRepBndLib import BRepBndLib
        from OCP.Bnd import Bnd_Box
        bounds=Bnd_Box();BRepBndLib.Add_s(rotated,bounds)
        xmin,ymin,zmin,xmax,ymax,zmax=bounds.Get()
        center=(xmin+xmax)/2,(ymin+ymax)/2,(zmin+zmax)/2
        cutter=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(xmin-.02,center[1],center[2]),gp_Dir(1,0,0)),2,xmax-xmin+.04).Shape()
        cut=BRepAlgoAPI_Cut(rotated,cutter)
        cut.Build()
        self.assertTrue(cut.IsDone())
        self.assertTrue(BRepCheck_Analyzer(cut.Shape()).IsValid())
        self.assertGreater(self.volume(rotated)-self.volume(cut.Shape()),1)

if __name__=='__main__':unittest.main()

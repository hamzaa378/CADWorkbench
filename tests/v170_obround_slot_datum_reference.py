"""Independent kernel references and source-wiring contracts for witnessed slots.

Reference OCP solids are independent of the exact C++ execution. A native
Windows build/CTest and GUI acceptance remain mandatory.
"""
import math
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class SlotAndDatumContracts(unittest.TestCase):
    def test_integrated_operation_is_editable_and_saved(self):
        kernel=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text()
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text()
        lineage=(ROOT/'src/core/BooleanReferenceLineage.cpp').read_text()
        native=(ROOT/'tests/associative_features_v159_tests.cpp').read_text()
        for op in ('Face slot through all', 'Face slot pocket'):
            with self.subTest(op=op):
                self.assertIn('"'+op+'"', kernel)
                self.assertIn('"'+op+'"', ui)
                self.assertIn('"'+op+'"', native)
        self.assertIn('type.startsWith("Face slot ")', kernel)
        self.assertIn('Face slot does not remove any stock.', kernel)
        self.assertIn('producedHistory=cut',kernel)
        self.assertIn('FaceMachiningDatum.h',kernel)
        self.assertIn('machining_datum::chooseOrigin(',kernel)
        self.assertIn('witnessOnFace(chosen,center,1e-6)',kernel)
        self.assertIn('const bool rigidTranslation=',kernel)
        self.assertIn('obround capsule adjustment mounting hole',ui)
        self.assertIn('type.startsWith(QStringLiteral("Face slot "))',lineage)
        self.assertIn('type.startsWith("Face slot ")',ui)

    def test_native_regressions_include_history_and_persistence(self):
        native=(ROOT/'tests/associative_features_v159_tests.cpp').read_text()
        for evidence in ('Reloaded obround pocket changed its saved dimensions',
                         'Rejected slot edit changed committed stock',
                         'Coplanar face merge moved a witnessed machining datum',
                         'Co-planar face merge displaced the slot from its design datum',
                         'In-plane translation failed to move the hole with its source face'):
            with self.subTest(evidence=evidence):
                self.assertIn(evidence,native)

try:
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse,BRepAlgoAPI_Cut,BRepAlgoAPI_Common
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.gp import gp_Ax2,gp_Pnt,gp_Dir
except ImportError:
    BRepPrimAPI_MakeBox=None

@unittest.skipIf(BRepPrimAPI_MakeBox is None,'OCP reference kernel not installed')
class PhysicalSlotReferences(unittest.TestCase):
    @staticmethod
    def volume(shape):
        g=GProp_GProps();BRepGProp.VolumeProperties_s(shape,g);return g.Mass()

    def slot_cut(self,stock,thickness,radius=2.,spacing=8.,angle_deg=0.,depth=None,
                 center=(10.,10.),top=10.):
        a=math.radians(angle_deg)
        longitudinal=(math.cos(a), math.sin(a))
        transverse=(math.sin(a), -math.cos(a))
        z=top+.01
        length=thickness+.02 if depth is None else depth+.01
        points=[gp_Pnt(center[0]+sign*spacing/2*longitudinal[0],
                       center[1]+sign*spacing/2*longitudinal[1],z)
                for sign in (-1,1)]
        first=BRepPrimAPI_MakeCylinder(gp_Ax2(points[0],gp_Dir(0,0,-1)),radius,length).Shape()
        second=BRepPrimAPI_MakeCylinder(gp_Ax2(points[1],gp_Dir(0,0,-1)),radius,length).Shape()
        corner=gp_Pnt(points[0].X()-radius*transverse[0],
                      points[0].Y()-radius*transverse[1],z)
        bridge=BRepPrimAPI_MakeBox(
            gp_Ax2(corner,gp_Dir(0,0,-1),gp_Dir(longitudinal[0],longitudinal[1],0)),
            spacing,2*radius,length).Shape()
        merged=BRepAlgoAPI_Fuse(first,bridge);merged.Build()
        self.assertTrue(merged.IsDone())
        merged2=BRepAlgoAPI_Fuse(merged.Shape(),second);merged2.Build()
        self.assertTrue(merged2.IsDone())
        self.assertTrue(BRepCheck_Analyzer(merged2.Shape()).IsValid())
        cut=BRepAlgoAPI_Cut(stock,merged2.Shape());cut.Build()
        self.assertTrue(cut.IsDone());self.assertTrue(BRepCheck_Analyzer(cut.Shape()).IsValid())
        return cut.Shape()

    def test_through_all_angles(self):
        stock=BRepPrimAPI_MakeBox(20,20,10).Shape()
        for angle in (0,15,30,45,90,-60):
            with self.subTest(angle=angle):
                result=self.slot_cut(stock,10,angle_deg=angle)
                self.assertAlmostEqual(self.volume(result),
                    4000-10*(32+4*math.pi),delta=2e-3)

    def test_pocket_vs_through_and_upstream_thickness(self):
        for thickness in (10,15,20):
            with self.subTest(thickness=thickness):
                stock=BRepPrimAPI_MakeBox(20,20,thickness).Shape()
                for depth in (None,4):
                    result=self.slot_cut(stock,thickness,depth=depth,top=thickness)
                    removed=(depth or thickness)*(32+4*math.pi)
                    self.assertAlmostEqual(self.volume(result),
                        400*thickness-removed,delta=2e-3)

    def test_general_cylindrical_stock(self):
        # No rectangular-primitive assumptions. A round blank with a planar
        # top accepts a centered obround slot entirely inside its radius.
        cylinder=BRepPrimAPI_MakeCylinder(12,10).Shape()
        result=self.slot_cut(cylinder,10,center=(0,0),top=10)
        self.assertAlmostEqual(self.volume(result),
            math.pi*12*12*10-10*(32+4*math.pi),delta=2e-3)

    def test_coplanar_merge_changes_centroid_not_original_design_location(self):
        source=BRepPrimAPI_MakeBox(20,20,10).Shape()
        attached=BRepPrimAPI_MakeBox(gp_Pnt(20,0,0),gp_Pnt(40,20,10)).Shape()
        joined=BRepAlgoAPI_Fuse(source,attached);joined.Build()
        self.assertTrue(joined.IsDone())
        joined.SimplifyResult(True,True)
        # A merged face is now centered at x=20, but the original source
        # witness and design datum at x=10 still lie on its interior.
        result=self.slot_cut(joined.Shape(),10,center=(10,10))
        probe=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(6,10,10),gp_Dir(0,0,-1)),1,10).Shape()
        common=BRepAlgoAPI_Common(result,probe);common.Build()
        self.assertTrue(common.IsDone())
        self.assertAlmostEqual(self.volume(common.Shape()),0,delta=1e-4)

if __name__=='__main__': unittest.main()

"""Independent OCCT regression examples for machining lineage safety.

These cases test real OCCT shapes, not the Windows Qt/C++ application binary.
"""
import unittest
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
from OCP.BRepAdaptor import BRepAdaptor_Curve, BRepAdaptor_Surface
from OCP.BRepClass import BRepClass_FaceClassifier
from OCP.GeomAbs import GeomAbs_Plane
from OCP.TopAbs import TopAbs_EDGE, TopAbs_FACE, TopAbs_IN
from OCP.TopExp import TopExp
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopoDS import TopoDS
from OCP.gp import gp_Pnt, gp_Dir, gp_Ax2
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
import math


def indexed(shape, kind):
    found=TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(shape,kind,found)
    return [found.FindKey(i) for i in range(1,found.Extent()+1)]


def inside_face(face, point):
    surface=BRepAdaptor_Surface(TopoDS.Face_s(face))
    return (surface.GetType()==GeomAbs_Plane and
            surface.Plane().Distance(point)<=1e-5 and
            BRepClass_FaceClassifier(TopoDS.Face_s(face),point,1e-5).State()==TopAbs_IN)


class MachiningProvenanceReferences(unittest.TestCase):
    def setUp(self):
        self.body=BRepPrimAPI_MakeBox(12,12,10).Shape()

    def test_boolean_clips_edge_midpoint_even_with_surviving_descendants(self):
        edge=indexed(self.body,TopAbs_EDGE)[0]
        cutter=BRepPrimAPI_MakeBox(gp_Pnt(-1,-1,4),3,3,2).Shape()
        cut=BRepAlgoAPI_Cut(self.body,cutter)
        cut.Build()
        self.assertTrue(cut.IsDone())
        self.assertGreater(cut.Modified(edge).Size(),0)
        # Source edge (0,0,z) originally crosses midpoint z=5. After
        # trimming, there are fragments but neither contains the witness.
        portions=[]
        for successor in indexed(cut.Shape(),TopAbs_EDGE):
            curve=BRepAdaptor_Curve(TopoDS.Edge_s(successor))
            p=curve.Value((curve.FirstParameter()+curve.LastParameter())/2)
            if abs(p.X())<1e-5 and abs(p.Y())<1e-5:
                a=curve.Value(curve.FirstParameter())
                b=curve.Value(curve.LastParameter())
                portions.append((min(a.Z(),b.Z()),max(a.Z(),b.Z())))
        self.assertGreaterEqual(len(portions),2)
        self.assertFalse(any(low<5<high for low,high in portions))

    def test_top_face_witness_stays_but_machining_center_is_removed(self):
        cutter=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(6,6,8),gp_Dir(0,0,1)),1,4).Shape()
        cut=BRepAlgoAPI_Cut(self.body,cutter)
        cut.Build()
        self.assertTrue(cut.IsDone())
        original_witness=gp_Pnt(1,1,10)
        consumed_center=gp_Pnt(6,6,10)
        top=[f for f in indexed(cut.Shape(),TopAbs_FACE) if inside_face(f,original_witness)]
        self.assertEqual(len(top),1)
        self.assertFalse(inside_face(top[0],consumed_center))

    def test_off_plane_point_never_counts_as_trimmed_face_interior(self):
        # A face classifier can project points onto other planes. Geometric
        # attachment requires a plane-distance check as well.
        face=next(f for f in indexed(self.body,TopAbs_FACE)
                  if inside_face(f,gp_Pnt(6,6,10)))
        self.assertFalse(inside_face(face,gp_Pnt(6,6,9)))

    def test_exact_direct_replay_face_pocket_fixture_is_valid(self):
        # C++ uses a 0.01 mm outside overlap, 2 mm depth and 0.6 mm radius.
        # This checks only the OCCT Boolean geometry, not the Qt C++ validator.
        cutter=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(6,6,10.01),gp_Dir(0,0,-1)),0.6,2.01).Shape()
        op=BRepAlgoAPI_Cut(self.body,cutter)
        op.Build()
        self.assertTrue(op.IsDone())
        self.assertTrue(BRepCheck_Analyzer(op.Shape()).IsValid())
        before=GProp_GProps();after=GProp_GProps()
        BRepGProp.VolumeProperties_s(self.body,before)
        BRepGProp.VolumeProperties_s(op.Shape(),after)
        self.assertAlmostEqual(before.Mass(),1440.0,places=5)
        self.assertAlmostEqual(before.Mass()-after.Mass(),math.pi*0.6**2*2.0,places=5)
    def test_face_machining_cut_and_boss_have_current_kernel_descendants(self):
        face=next(f for f in indexed(self.body,TopAbs_FACE)
                  if inside_face(f,gp_Pnt(6,6,10)))
        pocket=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(6,6,8),gp_Dir(0,0,1)),1,4).Shape()
        boss=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(6,6,9.99),gp_Dir(0,0,1)),1,3).Shape()
        for maker in (BRepAlgoAPI_Cut(self.body,pocket), BRepAlgoAPI_Fuse(self.body,boss)):
            maker.Build()
            self.assertTrue(maker.IsDone())
            result=indexed(maker.Shape(),TopAbs_FACE)
            descendants=[f for f in maker.Modified(face) if any(f.IsSame(g) for g in result)]
            self.assertGreater(len(descendants),0)
            self.assertTrue(maker.Shape().IsSame(maker.Shape()))

if __name__=='__main__':
    unittest.main()

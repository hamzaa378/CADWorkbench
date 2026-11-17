"""Independent OpenCascade geometry checks and source contracts.

These checks do not compile the CADWorkbench Qt/C++ application.
"""
import math
import unittest
from pathlib import Path
from OCP.BRepPrimAPI import BRepPrimAPI_MakeTorus, BRepPrimAPI_MakeCylinder
from OCP.BRepAdaptor import BRepAdaptor_Surface, BRepAdaptor_Curve
from OCP.BRep import BRep_Tool, BRep_Builder
from OCP.BRepClass import BRepClass_FaceClassifier
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeFace, BRepBuilderAPI_Transform
from OCP.Geom import Geom_BezierSurface
from OCP.GeomAPI import GeomAPI_ProjectPointOnSurf
from OCP.GeomAbs import GeomAbs_Torus, GeomAbs_Circle
from OCP.TColgp import TColgp_Array2OfPnt
from OCP.TopAbs import TopAbs_FACE, TopAbs_EDGE, TopAbs_IN
from OCP.TopExp import TopExp
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopoDS import TopoDS, TopoDS_Compound
from OCP.gp import gp_Pnt, gp_Vec, gp_Trsf

ROOT=Path(__file__).resolve().parents[1]

def subshapes(body,kind):
    mapping=TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(body,kind,mapping)
    convert=TopoDS.Face_s if kind==TopAbs_FACE else TopoDS.Edge_s
    return [convert(mapping.FindKey(i)) for i in range(1,mapping.Extent()+1)]

def torus_matches(body,original,minor,center,axis):
    found=[]
    for face in subshapes(body,TopAbs_FACE):
        adaptor=BRepAdaptor_Surface(face)
        if adaptor.GetType()!=GeomAbs_Torus:continue
        tor=adaptor.Torus()
        if abs(tor.MajorRadius()-16)>1e-4 or abs(tor.MinorRadius()-minor)>original*.20:continue
        if tor.Location().Distance(center)>1e-4:continue
        if abs(tor.Axis().Direction().Dot(axis))<.99999:continue
        projection=GeomAPI_ProjectPointOnSurf(gp_Pnt(-12,0,0),BRep_Tool.Surface_s(face))
        if projection.NbPoints()<1 or projection.LowerDistance()>.21*original:continue
        u,v=projection.LowerDistanceParameters()
        point=adaptor.Value(u,v)
        if BRepClass_FaceClassifier(face,point,1e-5).State()==TopAbs_IN:found.append(face)
    return found

def spline_face(u0=0.,u1=1.):
    poles=TColgp_Array2OfPnt(1,4,1,4)
    for i in range(1,5):
        for j in range(1,5):poles.SetValue(i,j,gp_Pnt(i*4,j*4,(i-2)**2*.2+(j-2)**2*.1))
    surface=Geom_BezierSurface(poles)
    return BRepBuilderAPI_MakeFace(surface,u0,u1,0.0,1.0,1e-7).Face()

class EngineeringIncrementChecks(unittest.TestCase):
    def test_small_toroidal_fillet_change_is_unique(self):
        new=BRepPrimAPI_MakeTorus(16,4.4).Shape()
        old=BRepPrimAPI_MakeTorus(16,4).Shape()
        torus=BRepAdaptor_Surface(next(f for f in subshapes(old,TopAbs_FACE) if BRepAdaptor_Surface(f).GetType()==GeomAbs_Torus)).Torus()
        matches=torus_matches(new,4.0,4.4,torus.Location(),torus.Axis().Direction())
        self.assertEqual(len(matches),1)

    def test_excessive_fillet_change_and_ambiguous_twins_rejected(self):
        old=BRepPrimAPI_MakeTorus(16,4).Shape()
        torus=BRepAdaptor_Surface(next(f for f in subshapes(old,TopAbs_FACE) if BRepAdaptor_Surface(f).GetType()==GeomAbs_Torus)).Torus()
        self.assertFalse(torus_matches(BRepPrimAPI_MakeTorus(16,6).Shape(),4.,4.,torus.Location(),torus.Axis().Direction()))
        new=BRepPrimAPI_MakeTorus(16,4.4).Shape()
        duplicated=TopoDS_Compound();builder=BRep_Builder();builder.MakeCompound(duplicated)
        builder.Add(duplicated,new);builder.Add(duplicated,BRepPrimAPI_MakeTorus(16,4.4).Shape())
        self.assertEqual(len(torus_matches(duplicated,4.,4.4,torus.Location(),torus.Axis().Direction())),2)

    def test_two_freeform_witnesses_reject_split_across_regions(self):
        full=spline_face()
        left=spline_face(0.,.5);right=spline_face(.5,1.)
        surface=BRepAdaptor_Surface(full)
        p1=surface.Value(.3,.5);p2=surface.Value(.7,.5)
        def in_patch(face,p):
            projection=GeomAPI_ProjectPointOnSurf(p,BRep_Tool.Surface_s(face))
            if projection.NbPoints()<1 or projection.LowerDistance()>.01:return False
            u,v=projection.LowerDistanceParameters()
            return BRepClass_FaceClassifier(face,BRepAdaptor_Surface(face).Value(u,v),1e-5).State()==TopAbs_IN
        self.assertTrue(in_patch(full,p1) and in_patch(full,p2))
        self.assertTrue(in_patch(left,p1) and not in_patch(left,p2))
        self.assertTrue(in_patch(right,p2) and not in_patch(right,p1))

    def test_two_circular_centers_are_geometric_not_edge_indices(self):
        cylinder=BRepPrimAPI_MakeCylinder(5,12).Shape()
        moved=gp_Trsf();moved.SetTranslation(gp_Vec(25,0,0))
        duplicate=BRepBuilderAPI_Transform(cylinder,moved,True).Shape()
        compound=TopoDS_Compound();builder=BRep_Builder();builder.MakeCompound(compound)
        builder.Add(compound,cylinder);builder.Add(compound,duplicate)
        circles=[]
        for edge in subshapes(compound,TopAbs_EDGE):
            curve=BRepAdaptor_Curve(edge)
            if curve.GetType()!=GeomAbs_Circle:continue
            if abs(curve.LastParameter()-curve.FirstParameter()-2*math.pi)>1e-4:continue
            if abs(curve.Circle().Location().Z()-12)>1e-5:continue
            circles.append(curve.Circle().Location())
        self.assertEqual(len(circles),2)
        self.assertAlmostEqual(circles[0].Distance(circles[1]),25.,places=5)

    def test_source_contract_and_legacy_persistence(self):
        core=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text(encoding="utf-8")
        lineage=(ROOT/'src/core/BooleanReferenceLineage.cpp').read_text(encoding="utf-8")
        scene=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        drawing=(ROOT/'src/core/DrawingSheet.cpp').read_text(encoding="utf-8")
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        self.assertIn('kCurvedWitness2Fields',core)
        self.assertIn('if(second)',core)
        self.assertIn('pointFields(&moved,"curved_witness2_",tr);',lineage)
        self.assertIn('findDirectStageFilletTorus',lineage)
        self.assertIn('"second_edge_anchor"',scene)
        self.assertIn('upper_tolerance_mm',scene)
        self.assertIn('CENTER_DISTANCE',drawing)
        self.assertIn('fullCircle(secondCurve)',drawing)
        self.assertIn('firstCircle.Location().Distance(otherCircle.Location())',drawing)
        self.assertIn('annotation.kind=kind->currentData().toString()',ui)
        self.assertIn('QStringLiteral("LENGTH")', (ROOT/'src/core/SceneDocument.h').read_text(encoding="utf-8"))

if __name__=='__main__':unittest.main()

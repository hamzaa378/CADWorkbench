"""Independent OpenCascade geometry checks; NOT CADWorkbench native integration tests."""
import math
import unittest
from OCP.BRep import BRep_Tool, BRep_Builder
from OCP.BRepAdaptor import BRepAdaptor_Surface, BRepAdaptor_Curve
from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform, BRepBuilderAPI_MakeFace
from OCP.BRepClass import BRepClass_FaceClassifier
from OCP.BRepPrimAPI import BRepPrimAPI_MakeCylinder
from OCP.Geom import Geom_BezierSurface
from OCP.GeomConvert import GeomConvert
from OCP.GeomAPI import GeomAPI_ProjectPointOnSurf
from OCP.GeomAbs import GeomAbs_BezierSurface, GeomAbs_BSplineSurface
from OCP.TColgp import TColgp_Array2OfPnt
from OCP.gp import gp_Pnt, gp_Vec, gp_Trsf, gp_Ax1, gp_Dir
from OCP.TopAbs import TopAbs_IN, TopAbs_FACE
from OCP.TopExp import TopExp
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopoDS import TopoDS_Compound, TopoDS


def make_surface(center_change=0):
    poles=TColgp_Array2OfPnt(1,4,1,4)
    for u in range(1,5):
        for v in range(1,5):
            height=0.2*(u-2)**2+0.15*(v-2)**2
            if u==v==2:height+=center_change
            poles.SetValue(u,v,gp_Pnt(4*u,4*v,height))
    return Geom_BezierSurface(poles)


def surface_normal(face,u,v):
    d=BRepAdaptor_Surface(face)
    p,du,dv=gp_Pnt(),gp_Vec(),gp_Vec()
    d.D1(u,v,p,du,dv)
    n=du.Crossed(dv)
    n.Normalize()
    return n


def possible_successors(source_face,candidates):
    old=BRepAdaptor_Surface(source_face)
    original=old.Value(0.5,0.5)
    normal=surface_normal(source_face,.5,.5)
    accepted=[]
    for candidate in candidates:
        proj=GeomAPI_ProjectPointOnSurf(original,BRep_Tool.Surface_s(candidate))
        if proj.NbPoints()<1 or proj.LowerDistance()>0.16:continue
        u,v=proj.LowerDistanceParameters()
        p=proj.NearestPoint()
        if abs(surface_normal(candidate,u,v).Dot(normal))<.98:continue
        if BRepClass_FaceClassifier(candidate,p,1e-5).State()!=TopAbs_IN:continue
        accepted.append(candidate)
    return accepted


class EngineeringGeometryChecks(unittest.TestCase):
    def test_bezier_small_deformation(self):
        base=BRepBuilderAPI_MakeFace(make_surface(),1e-7).Face()
        edited=BRepBuilderAPI_MakeFace(make_surface(.08),1e-7).Face()
        self.assertEqual(BRepAdaptor_Surface(base).GetType(),GeomAbs_BezierSurface)
        self.assertEqual(len(possible_successors(base,[edited])),1)

    def test_bezier_unrelated_or_ambiguous(self):
        base=BRepBuilderAPI_MakeFace(make_surface(),1e-7).Face()
        moved=gp_Trsf();moved.SetTranslation(gp_Vec(0,0,3))
        detached=TopoDS.Face_s(BRepBuilderAPI_Transform(base,moved,True).Shape())
        self.assertEqual(possible_successors(base,[detached]),[])
        tiny=gp_Trsf();tiny.SetTranslation(gp_Vec(0,0,.02))
        nearby=TopoDS.Face_s(BRepBuilderAPI_Transform(base,tiny,True).Shape())
        self.assertEqual(len(possible_successors(base,[base,nearby])),2)

    def test_bspline_conversion(self):
        spline=GeomConvert.SurfaceToBSplineSurface_s(make_surface())
        face=BRepBuilderAPI_MakeFace(spline,1e-7).Face()
        self.assertEqual(BRepAdaptor_Surface(face).GetType(),GeomAbs_BSplineSurface)
        self.assertEqual(len(possible_successors(face,[face])),1)

    def test_pattern_3_instances_and_circular_overlap(self):
        cylinder=BRepPrimAPI_MakeCylinder(10,20).Shape()
        builder=BRep_Builder()
        compound=TopoDS_Compound();builder.MakeCompound(compound)
        builder.Add(compound,cylinder)
        for i in (1,2):
            transform=gp_Trsf();transform.SetTranslation(gp_Vec(i*50,0,0))
            builder.Add(compound,BRepBuilderAPI_Transform(cylinder,transform,True).Shape())
        faces=TopTools_IndexedMapOfShape();TopExp.MapShapes_s(compound,TopAbs_FACE,faces)
        self.assertEqual(len([i for i in range(1,faces.Extent()+1)
                             if BRepAdaptor_Surface(TopoDS.Face_s(faces.FindKey(i))).GetType()
                             == BRepAdaptor_Surface(TopoDS.Face_s(faces.FindKey(1))).GetType()]),3)
        overlap=TopoDS_Compound();builder.MakeCompound(overlap)
        builder.Add(overlap,cylinder)
        for i in (1,2):
            rotate=gp_Trsf()
            rotate.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),i*math.pi/2)
            builder.Add(overlap,BRepBuilderAPI_Transform(cylinder,rotate,True).Shape())
        overlappingFaces=TopTools_IndexedMapOfShape()
        TopExp.MapShapes_s(overlap,TopAbs_FACE,overlappingFaces)
        # The copies have distinct topology but the same physical cylindrical support.
        self.assertEqual(overlappingFaces.Extent(),9)

if __name__=='__main__':unittest.main()

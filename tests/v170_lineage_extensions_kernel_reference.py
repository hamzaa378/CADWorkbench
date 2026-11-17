"""Independent OCP geometry checks for spline and curved-support witnesses.
These prove OCC geometry behavior, NOT the CADWorkbench C++ integration.
"""
import math
import unittest
from OCP.gp import gp_Pnt, gp_Vec, gp_Dir, gp_Ax1, gp_Trsf
from OCP.TColgp import TColgp_Array1OfPnt
from OCP.GeomAPI import GeomAPI_PointsToBSpline, GeomAPI_ProjectPointOnCurve
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge, BRepBuilderAPI_Transform
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
from OCP.BRepAdaptor import BRepAdaptor_Curve, BRepAdaptor_Surface
from OCP.BRepClass import BRepClass_FaceClassifier
from OCP.GeomAbs import GeomAbs_BSplineCurve, GeomAbs_Cylinder
from OCP.TopAbs import TopAbs_FACE, TopAbs_IN
from OCP.TopExp import TopExp
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopoDS import TopoDS


def spline_match(edges,point,tangent):
    matched=[]
    for e in edges:
        curve=BRepAdaptor_Curve(e)
        if curve.GetType()!=GeomAbs_BSplineCurve:continue
        start,end=curve.FirstParameter(),curve.LastParameter()
        projector=GeomAPI_ProjectPointOnCurve(point,curve.Curve().Curve(),start,end)
        if projector.NbPoints()<1 or projector.LowerDistance()>1e-5:continue
        t=projector.LowerDistanceParameter()
        if t<=start+1e-7*(end-start) or t>=end-1e-7*(end-start):continue
        sample=gp_Pnt();derivative=gp_Vec()
        curve.D1(t,sample,derivative)
        derivative.Normalize()
        if abs(derivative.Dot(tangent))<0.995:continue
        matched.append(e)
    return matched


def lateral_faces(shape):
    map_=TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(shape,TopAbs_FACE,map_)
    return [TopoDS.Face_s(map_.FindKey(i)) for i in range(1,map_.Extent()+1)
            if BRepAdaptor_Surface(TopoDS.Face_s(map_.FindKey(i))).GetType()==GeomAbs_Cylinder]


def contains(face,point):
    return BRepClass_FaceClassifier(face,point,1e-5).State()==TopAbs_IN


class NewLineageKernel(unittest.TestCase):
    def test_spline_trim_and_rotation(self):
        poles=TColgp_Array1OfPnt(1,5)
        for i,p in enumerate([(0,0,0),(4,3,0),(8,0,0),(12,-2,0),(16,0,0)],1):
            poles.SetValue(i,gp_Pnt(*p))
        curve=GeomAPI_PointsToBSpline(poles).Curve()
        lo,hi=curve.FirstParameter(),curve.LastParameter()
        t=0.5*(lo+hi)
        point=curve.Value(t)
        tangent=gp_Vec();dummy=gp_Pnt()
        curve.D1(t,dummy,tangent);tangent.Normalize()
        edge=BRepBuilderAPI_MakeEdge(curve).Edge()
        self.assertEqual(len(spline_match([edge],point,tangent)),1)
        cut=lo+0.8*(hi-lo)
        e1=BRepBuilderAPI_MakeEdge(curve,lo,cut).Edge()
        e2=BRepBuilderAPI_MakeEdge(curve,cut,hi).Edge()
        self.assertEqual(len(spline_match([e1,e2],point,tangent)),1)
        left=BRepBuilderAPI_MakeEdge(curve,lo,t).Edge()
        right=BRepBuilderAPI_MakeEdge(curve,t,hi).Edge()
        self.assertEqual(len(spline_match([left,right],point,tangent)),0)
        tr=gp_Trsf();tr.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),math.radians(65))
        rotated=BRepBuilderAPI_Transform(edge,tr,True).Shape()
        newPoint=gp_Pnt(point.X(),point.Y(),point.Z());newPoint.Transform(tr)
        newTangent=gp_Vec(tangent.X(),tangent.Y(),tangent.Z());newTangent.Transform(tr)
        self.assertEqual(len(spline_match([TopoDS.Edge_s(rotated)],newPoint,newTangent)),1)

    def test_curved_witness_after_cut_and_rotation(self):
        shape=BRepPrimAPI_MakeCylinder(10,20).Shape()
        witness=gp_Pnt(0,10,10)
        faces=lateral_faces(shape)
        self.assertEqual(sum(contains(f,witness) for f in faces),1)
        cut=BRepAlgoAPI_Cut(shape,BRepPrimAPI_MakeBox(gp_Pnt(-20,-20,14),40,40,20).Shape())
        cut.Build()
        self.assertTrue(cut.IsDone())
        successors=lateral_faces(cut.Shape())
        self.assertEqual(sum(contains(f,witness) for f in successors),1)
        tr=gp_Trsf();tr.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,0,0)),math.pi/4)
        rotated=BRepBuilderAPI_Transform(shape,tr,True).Shape()
        new=gp_Pnt(witness.X(),witness.Y(),witness.Z());new.Transform(tr)
        self.assertEqual(sum(contains(f,new) for f in lateral_faces(rotated)),1)

if __name__=='__main__': unittest.main()

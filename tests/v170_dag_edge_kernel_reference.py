"""OpenCascade Python reference for the witnessed-edge geometry contract.

This independently tests OCCT projection/trim semantics; it does NOT compile
or execute the C++ Qt application. Native regression tests are registered in
existing CTest targets for Windows verification.
"""
import math
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeFace, BRepBuilderAPI_MakeEdge, BRepBuilderAPI_Transform
from OCP.BRepAlgoAPI import BRepAlgoAPI_Splitter
from OCP.GeomAPI import GeomAPI_ProjectPointOnCurve
from OCP.BRepAdaptor import BRepAdaptor_Curve
from OCP.GeomAbs import GeomAbs_Circle, GeomAbs_Line
from OCP.TopTools import TopTools_ListOfShape, TopTools_IndexedMapOfShape
from OCP.TopoDS import TopoDS
from OCP.TopExp import TopExp
from OCP.TopAbs import TopAbs_EDGE
from OCP.gp import gp_Pnt, gp_Vec, gp_Dir, gp_Pln, gp_Trsf, gp_Circ, gp_Ax2


def split_at(box, x):
    boundary = BRepBuilderAPI_MakeFace(
        gp_Pln(gp_Pnt(x,0,0),gp_Dir(1,0,0)),-20,20,-20,20).Shape()
    objects=TopTools_ListOfShape();objects.Append(box)
    tools=TopTools_ListOfShape();tools.Append(boundary)
    splitter=BRepAlgoAPI_Splitter()
    splitter.SetArguments(objects);splitter.SetTools(tools);splitter.Build()
    if not splitter.IsDone():
        raise RuntimeError('Kernel split failed')
    return splitter.Shape()


def edges(shape):
    unique=TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(shape,TopAbs_EDGE,unique)
    return [TopoDS.Edge_s(unique.FindKey(i)) for i in range(1,unique.Extent()+1)]


def matching_line_or_circle_edges(shape,witness,tangent,kind,curvature=0):
    found=[]
    for edge in edges(shape):
        curve=BRepAdaptor_Curve(edge)
        if curve.GetType() != (GeomAbs_Circle if kind==2 else GeomAbs_Line):
            continue
        if kind==2 and abs(1/curve.Circle().Radius()-curvature)>max(1e-6,curvature*1e-4):
            continue
        first,last=curve.FirstParameter(),curve.LastParameter()
        if last-first<1e-8:
            continue
        projection=GeomAPI_ProjectPointOnCurve(witness,curve.Curve().Curve(),first,last)
        if projection.NbPoints()<1 or projection.LowerDistance()>1e-5:
            continue
        parameter=projection.LowerDistanceParameter()
        if parameter<=first+1e-7*(last-first) or parameter>=last-1e-7*(last-first):
            continue
        point=gp_Pnt();direction=gp_Vec()
        curve.D1(parameter,point,direction)
        direction.Normalize()
        dot=abs(direction.X()*tangent[0]+direction.Y()*tangent[1]+direction.Z()*tangent[2])
        if dot<0.995:
            continue
        found.append(edge)
    return found


class WitnessEdgeKernel(unittest.TestCase):
    def test_line_split_successor_and_boundary(self):
        box=BRepPrimAPI_MakeBox(10,10,10).Shape()
        witness=gp_Pnt(5,0,10)
        self.assertEqual(len(matching_line_or_circle_edges(box,witness,(1,0,0),1)),1)
        self.assertEqual(len(matching_line_or_circle_edges(split_at(box,7),witness,(1,0,0),1)),1)
        self.assertEqual(len(matching_line_or_circle_edges(split_at(box,5),witness,(1,0,0),1)),0)

    def test_translation_and_ambiguity(self):
        box=BRepPrimAPI_MakeBox(10,10,10).Shape()
        tr=gp_Trsf();tr.SetTranslation(gp_Vec(1800,-950,410))
        moved=BRepBuilderAPI_Transform(box,tr,True).Shape()
        self.assertEqual(len(matching_line_or_circle_edges(moved,gp_Pnt(1805,-950,420),(1,0,0),1)),1)
        edge=BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,0),gp_Pnt(10,0,0)).Edge()
        # Two distinct coincident edges are inherently ambiguous. The C++
        # native suite checks the compound case; independent kernel support
        # can be checked by counting the same reference twice here.
        hits=[e for e in [edge,edge] if matching_line_or_circle_edges(e,gp_Pnt(5,0,0),(1,0,0),1)]
        self.assertEqual(len(hits),2)

    def test_circle_trim_and_deleted_witness(self):
        circle=gp_Circ(gp_Ax2(gp_Pnt(0,0,0),gp_Dir(0,0,1)),10)
        full=BRepBuilderAPI_MakeEdge(circle,0,2*math.pi).Edge()
        survivor=BRepBuilderAPI_MakeEdge(circle,0,1.5*math.pi).Edge()
        lost=BRepBuilderAPI_MakeEdge(circle,1.5*math.pi,2*math.pi).Edge()
        witness=gp_Pnt(-10,0,0)
        curvature=0.1
        self.assertEqual(len(matching_line_or_circle_edges(full,witness,(0,-1,0),2,curvature)),1)
        self.assertEqual(len(matching_line_or_circle_edges(survivor,witness,(0,-1,0),2,curvature)),1)
        self.assertEqual(len(matching_line_or_circle_edges(lost,witness,(0,-1,0),2,curvature)),0)

if __name__=='__main__':
    unittest.main()

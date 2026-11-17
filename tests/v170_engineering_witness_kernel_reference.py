"""Independent OCCT geometry preconditions for v1.70 engineering increment.

These exercises use Python OpenCascade and are NOT the native C++ rebuild tests.
"""
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeCylinder, BRepPrimAPI_MakeBox
from OCP.BRepAdaptor import BRepAdaptor_Surface, BRepAdaptor_Curve
from OCP.GeomAbs import GeomAbs_Cylinder, GeomAbs_Line
from OCP.BRepClass import BRepClass_FaceClassifier
from OCP.BRep import BRep_Builder
from OCP.GeomAPI import GeomAPI_ProjectPointOnCurve
from OCP.TopAbs import TopAbs_FACE, TopAbs_EDGE, TopAbs_IN
from OCP.TopExp import TopExp
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopoDS import TopoDS, TopoDS_Compound
from OCP.gp import gp_Pnt, gp_Vec


def mapped(shape,kind):
    out=TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(shape,kind,out)
    return [out.FindKey(i) for i in range(1,out.Extent()+1)]


def coaxial_candidates(shape,old_radius,witness):
    accepted=[]
    for generic in mapped(shape,TopAbs_FACE):
        face=TopoDS.Face_s(generic)
        surface=BRepAdaptor_Surface(face)
        if surface.GetType()!=GeomAbs_Cylinder:continue
        cylinder=surface.Cylinder()
        radius=cylinder.Radius()
        if abs(radius-old_radius)>0.2*old_radius:continue
        axis=cylinder.Axis()
        if abs(axis.Direction().Z())<0.99999:continue
        if axis.Location().X()**2+axis.Location().Y()**2>1e-8:continue
        # Original witness is radial, not on the periodic seam.
        new_witness=gp_Pnt(witness.X()*radius/old_radius,
                            witness.Y()*radius/old_radius,witness.Z())
        if BRepClass_FaceClassifier(face,new_witness,1e-5).State()==TopAbs_IN:
            accepted.append(face)
    return accepted


def referenced_lines(shape, witness):
    found=[]
    for generic in mapped(shape,TopAbs_EDGE):
        edge=TopoDS.Edge_s(generic)
        curve=BRepAdaptor_Curve(edge)
        if curve.GetType()!=GeomAbs_Line:continue
        lo,hi=curve.FirstParameter(),curve.LastParameter()
        projected=GeomAPI_ProjectPointOnCurve(witness,curve.Curve().Curve(),lo,hi)
        if projected.NbPoints()<1 or projected.LowerDistance()>1e-5:continue
        at=projected.LowerDistanceParameter()
        if at<=lo+1e-7*(hi-lo) or at>=hi-1e-7*(hi-lo):continue
        p=gp_Pnt();t=gp_Vec()
        curve.D1(at,p,t)
        if t.SquareMagnitude()<1e-12:continue
        t.Normalize()
        if abs(t.Z())>0.995:found.append(edge)
    return found


class EngineeringWitnessGeometry(unittest.TestCase):
    def test_coaxial_generated_cylinder_radius_change(self):
        old=gp_Pnt(0,10,10)
        self.assertEqual(len(coaxial_candidates(BRepPrimAPI_MakeCylinder(11,20).Shape(),10,old)),1)
        self.assertEqual(len(coaxial_candidates(BRepPrimAPI_MakeCylinder(16,20).Shape(),10,old)),0)

    def test_two_generated_cylinders_are_ambiguous(self):
        compound=TopoDS_Compound()
        builder=BRep_Builder()
        builder.MakeCompound(compound)
        builder.Add(compound,BRepPrimAPI_MakeCylinder(11,20).Shape())
        builder.Add(compound,BRepPrimAPI_MakeCylinder(11,20).Shape())
        self.assertEqual(len(coaxial_candidates(compound,10,gp_Pnt(0,10,10))),2)

    def test_line_edge_survives_unrelated_dimension_edit(self):
        witness=gp_Pnt(0,0,5)
        old=BRepPrimAPI_MakeBox(10,10,10).Shape()
        changed=BRepPrimAPI_MakeBox(15,10,10).Shape()
        self.assertEqual(len(referenced_lines(old,witness)),1)
        self.assertEqual(len(referenced_lines(changed,witness)),1)

    def test_removed_edge_witness_is_not_reassigned(self):
        witness=gp_Pnt(0,0,5)
        changed=BRepPrimAPI_MakeBox(10,10,4).Shape()
        self.assertEqual(len(referenced_lines(changed,witness)),0)

if __name__=='__main__':unittest.main()

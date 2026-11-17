"""Independent OCCT behavior checks, not native CADWorkbench integration tests."""
import unittest
from OCP.BRep import BRep_Builder
from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform, BRepBuilderAPI_MakePolygon
from OCP.BRepFilletAPI import BRepFilletAPI_MakeFillet, BRepFilletAPI_MakeChamfer
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections
from OCP.TopExp import TopExp
from OCP.TopAbs import TopAbs_EDGE, TopAbs_SOLID
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopoDS import TopoDS, TopoDS_Compound
from OCP.gp import gp_Pnt, gp_Vec, gp_Trsf
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps


def subshapes(shape, kind):
    m=TopTools_IndexedMapOfShape();TopExp.MapShapes_s(shape,kind,m)
    return [m.FindKey(i) for i in range(1,m.Extent()+1)]


def instance_compound(source, count, delta):
    builder=BRep_Builder();result=TopoDS_Compound();builder.MakeCompound(result)
    for i in range(count):
        tr=gp_Trsf();tr.SetTranslation(gp_Vec(*(i*v for v in delta)))
        builder.Add(result,BRepBuilderAPI_Transform(source,tr,True).Shape())
    return result


class NestedModifierGeometryReference(unittest.TestCase):
    def setUp(self):
        self.base=BRepPrimAPI_MakeBox(12,12,10).Shape()

    def test_exact_edge_identity_survives_selected_fillet(self):
        source=subshapes(self.base,TopAbs_EDGE)
        op=BRepFilletAPI_MakeFillet(self.base);op.Add(.2,TopoDS.Edge_s(source[0]));op.Build()
        self.assertTrue(op.IsDone())
        after=subshapes(op.Shape(),TopAbs_EDGE)
        preserved=[e for e in source if any(e.IsSame(n) for n in after)]
        self.assertGreater(len(preserved),0)
        self.assertFalse(any(source[0].IsSame(n) for n in after))

    def test_chosen_edge_requires_exact_surviving_topology(self):
        source=subshapes(self.base,TopAbs_EDGE)
        op=BRepFilletAPI_MakeFillet(self.base);op.Add(.15,TopoDS.Edge_s(source[0]));op.Build()
        self.assertTrue(op.IsDone())
        after=subshapes(op.Shape(),TopAbs_EDGE)
        self.assertTrue(any(source[5].IsSame(n) for n in after))
        self.assertFalse(any(source[0].IsSame(n) for n in after))

    def test_exact_edge_identity_survives_selected_chamfer(self):
        source=subshapes(self.base,TopAbs_EDGE)
        op=BRepFilletAPI_MakeChamfer(self.base)
        op.Add(.2,TopoDS.Edge_s(source[0]));op.Build()
        self.assertTrue(op.IsDone())
        after=subshapes(op.Shape(),TopAbs_EDGE)
        self.assertTrue(any(source[5].IsSame(n) for n in after))
        self.assertFalse(any(source[0].IsSame(n) for n in after))

    def test_two_nested_patterns_have_six_distinct_copy_instances(self):
        first=instance_compound(self.base,3,(30,0,0))
        nested=instance_compound(first,2,(0,45,0))
        solids=subshapes(nested,TopAbs_SOLID)
        self.assertEqual(len(solids),6)
        centers=set()
        for solid in solids:
            props=GProp_GProps();BRepGProp.VolumeProperties_s(solid,props)
            p=props.CentreOfMass()
            centers.add((round(p.X()),round(p.Y()),round(p.Z())))
        self.assertEqual(len(centers),6)
        self.assertIn((36,51,5),centers)  # instance (x=1, y=1)
        self.assertIn((66,51,5),centers)  # instance (x=2, y=1)

    def test_loft_produces_positive_solid_volume(self):
        def rect(x,y,w,d,z):
            builder=BRepBuilderAPI_MakePolygon()
            for p in ((x,y,z),(x+w,y,z),(x+w,y+d,z),(x,y+d,z)):
                builder.Add(gp_Pnt(*p))
            builder.Close();return builder.Wire()
        maker=BRepOffsetAPI_ThruSections(True,True)
        maker.AddWire(rect(0,0,5,5,0));maker.AddWire(rect(1,1,6,6,8))
        maker.Build();self.assertTrue(maker.IsDone())
        props=GProp_GProps();BRepGProp.VolumeProperties_s(maker.Shape(),props)
        self.assertGreater(props.Mass(),0.0)

if __name__=='__main__':unittest.main()

"""Independent OCCT geometry contracts for the current-build provenance path.

Does not compile CADWorkbench's Qt/MSVC integration. Builder modifications are
examined against only the final result, never treated as persisted names.
"""
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepFilletAPI import BRepFilletAPI_MakeFillet, BRepFilletAPI_MakeChamfer
from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse, BRepAlgoAPI_Cut
from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakePolygon
from OCP.TopAbs import TopAbs_EDGE, TopAbs_FACE
from OCP.TopoDS import TopoDS
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopExp import TopExp
from OCP.gp import gp_Pnt


def items(shape,kind):
    mapped=TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(shape,kind,mapped)
    return [mapped.FindKey(i) for i in range(1,mapped.Extent()+1)]


def confirmed(maker,source,result,kind):
    """A candidate must be an actual Modified/Generated descendant OR unchanged."""
    available=items(result,kind)
    found=[]
    if not maker.IsDeleted(source):
        found += [edge for edge in available if edge.IsSame(source)]
    for method in (maker.Modified,maker.Generated):
        for descendant in method(source):
            found += [candidate for candidate in available if candidate.IsSame(descendant)]
    return len(set(id(shape) for shape in found))  # distinct not important for presence


def loft_shape():
    def rect(x,y,w,z):
        poly=BRepBuilderAPI_MakePolygon()
        for p in [(x,y,z),(x+w,y,z),(x+w,y+w,z),(x,y+w,z)]:
            poly.Add(gp_Pnt(*p))
        poly.Close()
        return poly.Wire()
    loft=BRepOffsetAPI_ThruSections(True,True)
    loft.AddWire(rect(0,0,8,0));loft.AddWire(rect(1,1,7,15));loft.Build()
    assert loft.IsDone()
    return loft.Shape()


class KernelModifierHistoryTests(unittest.TestCase):
    def setUp(self):
        self.box=BRepPrimAPI_MakeBox(12,12,10).Shape()
        self.edges=items(self.box,TopAbs_EDGE)
        self.faces=items(self.box,TopAbs_FACE)

    def test_fillet_returns_real_modified_face_descendant(self):
        maker=BRepFilletAPI_MakeFillet(self.box)
        maker.Add(.3,TopoDS.Edge_s(self.edges[0]));maker.Build()
        self.assertTrue(maker.IsDone())
        modified=[face for face in self.faces if maker.Modified(face).Size()>0]
        self.assertGreater(len(modified),0)
        for face in modified:
            self.assertGreater(confirmed(maker,face,maker.Shape(),TopAbs_FACE),0)

    def test_chamfer_returns_real_modified_face_descendant(self):
        maker=BRepFilletAPI_MakeChamfer(self.box)
        maker.Add(.25,TopoDS.Edge_s(self.edges[0]));maker.Build()
        self.assertTrue(maker.IsDone())
        modified=[face for face in self.faces if maker.Modified(face).Size()>0]
        self.assertGreater(len(modified),0)
        self.assertGreater(confirmed(maker,modified[0],maker.Shape(),TopAbs_FACE),0)

    def test_consumed_edge_is_not_relabelled_by_geometric_similarity(self):
        maker=BRepFilletAPI_MakeFillet(self.box)
        maker.Add(.3,TopoDS.Edge_s(self.edges[0]));maker.Build()
        self.assertTrue(maker.IsDone())
        self.assertFalse(any(self.edges[0].IsSame(edge) for edge in items(maker.Shape(),TopAbs_EDGE)))

    def test_loft_fuse_preserves_kernel_trace_for_existing_box_faces(self):
        maker=BRepAlgoAPI_Fuse(self.box,loft_shape());maker.Build()
        self.assertTrue(maker.IsDone())
        survivors=sum(bool(confirmed(maker,face,maker.Shape(),TopAbs_FACE)) for face in self.faces)
        self.assertGreater(survivors,0)

    def test_cylinder_cut_preserves_kernel_trace_for_unaffected_faces(self):
        tool=BRepPrimAPI_MakeBox(gp_Pnt(3,3,8),2,2,4).Shape()
        cut=BRepAlgoAPI_Cut(self.box,tool);cut.Build()
        self.assertTrue(cut.IsDone())
        survivors=sum(bool(confirmed(cut,face,cut.Shape(),TopAbs_FACE)) for face in self.faces)
        self.assertGreaterEqual(survivors,1)

if __name__=='__main__':
    unittest.main()

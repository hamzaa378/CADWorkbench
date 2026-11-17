"""Independent OCCT reference tests for Boolean Modified/Generated history.

These exercise OpenCascade through OCP, not the CADWorkbench C++ implementation.
"""
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.TopoDS import TopoDS
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopExp import TopExp
from OCP.TopAbs import TopAbs_EDGE, TopAbs_FACE
from OCP.gp import gp_Pnt
from OCP.BRepFilletAPI import BRepFilletAPI_MakeFillet
from OCP.BRepCheck import BRepCheck_Analyzer


def mapped(shape, kind):
    storage = TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(shape, kind, storage)
    return [storage.FindKey(i) for i in range(1, storage.Extent() + 1)]


def center(shape, kind):
    props = GProp_GProps()
    if kind == TopAbs_EDGE:
        BRepGProp.LinearProperties_s(shape, props)
    else:
        BRepGProp.SurfaceProperties_s(shape, props)
    return props.CentreOfMass()


def lineage_matches(op, source, kind):
    surviving = mapped(op.Shape(), kind)
    possible = []
    if not op.IsDeleted(source):
        possible.append(source)
    possible.extend(list(op.Modified(source)))
    possible.extend(list(op.Generated(source)))
    return [i for i, piece in enumerate(surviving)
            if any(piece.IsSame(candidate) for candidate in possible)]


class BooleanLineageReference(unittest.TestCase):
    def setUp(self):
        self.base = BRepPrimAPI_MakeBox(10, 10, 10).Shape()
        self.edges = mapped(self.base, TopAbs_EDGE)

    def test_cut_retains_bottom_edge_for_fillet(self):
        tool = BRepPrimAPI_MakeBox(gp_Pnt(2, 2, 8), 6, 6, 6).Shape()
        op = BRepAlgoAPI_Cut(self.base, tool)
        op.Build()
        self.assertTrue(op.IsDone())
        candidate = next(edge for edge in self.edges if
                         abs(center(edge, TopAbs_EDGE).X()-5)<1e-6 and
                         abs(center(edge, TopAbs_EDGE).Y())<1e-6 and
                         abs(center(edge, TopAbs_EDGE).Z())<1e-6)
        found = lineage_matches(op, candidate, TopAbs_EDGE)
        self.assertEqual(len(found), 1)
        edge = TopoDS.Edge_s(mapped(op.Shape(), TopAbs_EDGE)[found[0]])
        fillet = BRepFilletAPI_MakeFillet(op.Shape())
        fillet.Add(0.2, edge)
        fillet.Build()
        self.assertTrue(fillet.IsDone())
        self.assertTrue(BRepCheck_Analyzer(fillet.Shape()).IsValid())

    def test_planar_face_survives_cut(self):
        tool = BRepPrimAPI_MakeBox(gp_Pnt(2, 2, 8), 6, 6, 6).Shape()
        op = BRepAlgoAPI_Cut(self.base, tool)
        op.Build()
        bottom = next(face for face in mapped(self.base, TopAbs_FACE)
                      if abs(center(face, TopAbs_FACE).Z()) < 1e-6)
        self.assertEqual(len(lineage_matches(op, bottom, TopAbs_FACE)), 1)

    def test_face_offset_correction_after_asymmetric_cut(self):
        upper = next(face for face in mapped(self.base, TopAbs_FACE)
                     if abs(center(face, TopAbs_FACE).Z()-10)<1e-6)
        old = center(upper, TopAbs_FACE)
        tool = BRepPrimAPI_MakeBox(gp_Pnt(1, 1, 8), 3, 3, 5).Shape()
        op = BRepAlgoAPI_Cut(self.base, tool)
        op.Build()
        self.assertTrue(op.IsDone())
        descendants = lineage_matches(op, upper, TopAbs_FACE)
        self.assertEqual(len(descendants), 1)
        new = center(mapped(op.Shape(), TopAbs_FACE)[descendants[0]], TopAbs_FACE)
        self.assertGreater(abs(old.X()-new.X())+abs(old.Y()-new.Y()), 0.05)
        new_u, new_v = old.X()-new.X(), old.Y()-new.Y()
        self.assertAlmostEqual(new.X()+new_u, old.X(), places=5)
        self.assertAlmostEqual(new.Y()+new_v, old.Y(), places=5)

    def test_removed_edge_does_not_claim_old_identity(self):
        removed = BRepPrimAPI_MakeBox(gp_Pnt(0, -2, -2), 10, 4, 4).Shape()
        op = BRepAlgoAPI_Cut(self.base, removed)
        op.Build()
        source = next(edge for edge in self.edges if
                      abs(center(edge, TopAbs_EDGE).X()-5)<1e-6 and
                      abs(center(edge, TopAbs_EDGE).Y())<1e-6 and
                      abs(center(edge, TopAbs_EDGE).Z())<1e-6)
        self.assertEqual(lineage_matches(op, source, TopAbs_EDGE), [])

    def test_upstream_edit_fuse_keeps_reference(self):
        source = next(edge for edge in self.edges if
                      abs(center(edge, TopAbs_EDGE).X()-5)<1e-6 and
                      abs(center(edge, TopAbs_EDGE).Y())<1e-6 and
                      abs(center(edge, TopAbs_EDGE).Z())<1e-6)
        volumes = []
        for width in (8, 6):
            boss = BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 8), width, 7, 5).Shape()
            op = BRepAlgoAPI_Fuse(self.base, boss)
            op.Build()
            self.assertTrue(op.IsDone())
            self.assertEqual(len(lineage_matches(op, source, TopAbs_EDGE)), 1)
            volume = GProp_GProps()
            BRepGProp.VolumeProperties_s(op.Shape(), volume)
            volumes.append(volume.Mass())
        self.assertNotAlmostEqual(*volumes)


if __name__ == '__main__':
    unittest.main()

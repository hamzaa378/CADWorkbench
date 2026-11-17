"""Independent OCCT circle geometry + drawing integration contracts.

This is a geometry reference test, NOT a native CADWorkbench/Qt test. The full
circle matching policy is also tested in portable C++.
"""
import math
import unittest
from pathlib import Path
from OCP.BRepAdaptor import BRepAdaptor_Curve
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge
from OCP.GeomAbs import GeomAbs_Circle
from OCP.gp import gp_Ax2, gp_Circ, gp_Dir, gp_Pnt

ROOT=Path(__file__).resolve().parents[1]


def circular_edge(radius,xyz=(10,20,0),axis=(0,0,1),partial=False):
    circle=gp_Circ(gp_Ax2(gp_Pnt(*xyz),gp_Dir(*axis)),radius)
    if partial:
        return BRepBuilderAPI_MakeEdge(circle,0,math.pi).Edge()
    return BRepBuilderAPI_MakeEdge(circle).Edge()


def circle_record(edge):
    curve=BRepAdaptor_Curve(edge)
    if curve.GetType()!=GeomAbs_Circle or abs(curve.LastParameter()-curve.FirstParameter()-2*math.pi)>1e-4:
        return None
    circle=curve.Circle()
    c=circle.Location(); axis=circle.Axis().Direction()
    return ((c.X(),c.Y(),c.Z()),(axis.X(),axis.Y(),axis.Z()),circle.Radius())


def compatible(a,b):
    c0,axis0,r0=a;c1,axis1,r1=b
    return (math.dist(c0,c1)<=max(1e-4,r0*1e-5) and
            abs(sum(x*y for x,y in zip(axis0,axis1)))>=0.99999 and
            abs(r1-r0)<=0.2*r0+1e-9)


class DrawingCircleResize(unittest.TestCase):
    def test_full_bore_resize_same_center_and_axis(self):
        old=circle_record(circular_edge(5))
        updated=circle_record(circular_edge(5.75))
        self.assertIsNotNone(old);self.assertTrue(compatible(old,updated))
        self.assertAlmostEqual(updated[2]*2,11.5)
        self.assertFalse(compatible(old,circle_record(circular_edge(6.2))))

    def test_plane_or_center_motion_rejected(self):
        old=circle_record(circular_edge(5))
        self.assertFalse(compatible(old,circle_record(circular_edge(5.3,(10.1,20,0)))))
        self.assertFalse(compatible(old,circle_record(circular_edge(5.3,(10,20,0.1)))))
        self.assertFalse(compatible(old,circle_record(circular_edge(5.3,axis=(0,1,0)))))

    def test_trimmed_arcs_are_never_hole_diameters(self):
        self.assertIsNone(circle_record(circular_edge(5,partial=True)))

    def test_ambiguity_fails_closed(self):
        old=circle_record(circular_edge(5))
        choices=[circle_record(circular_edge(5.3)),circle_record(circular_edge(5.9))]
        self.assertEqual(sum(compatible(old,r) for r in choices),2)

    def test_drawing_only_contract_and_existing_machining_stays_strict(self):
        tree=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text(encoding="utf-8")
        drawing=(ROOT/'src/core/DrawingSheet.cpp').read_text(encoding="utf-8")
        self.assertIn('bool resolveResizedFullCircleForDrawing(',tree)
        self.assertIn('const circular_lineage::Reference candidate',tree)
        self.assertIn('if(matches!=1)',tree)
        self.assertIn('if(annotation.kind=="LENGTH"||',drawing)
        self.assertIn('resolveResizedFullCircleForDrawing(source->shape,annotation.secondEdgeAnchor,',drawing)
        self.assertIn('resolveStableEdgeAnchor(',tree)


if __name__=='__main__':unittest.main()

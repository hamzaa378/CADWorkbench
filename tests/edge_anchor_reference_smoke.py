"""Independent OCCT/OCP reference for the v1.59.1 edge-index fix.

This checks the topology invariant behind the native C++ regression. It is
not a substitute for compiling ShapeFeatureTree.cpp or running CTest on Windows.
Requires cadquery and OCP; run: python tests/edge_anchor_reference_smoke.py
"""
import math
import unittest

import cadquery as cq
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.TopAbs import TopAbs_EDGE
from OCP.TopExp import TopExp, TopExp_Explorer
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopoDS import TopoDS


def descriptor(edge):
    props = GProp_GProps()
    BRepGProp.LinearProperties_s(edge, props)
    p = props.CentreOfMass()
    return (p.X(), p.Y(), p.Z(), props.Mass())


def unique_edges(shape):
    edges = TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(shape, TopAbs_EDGE, edges)
    return [TopoDS.Edge_s(edges.FindKey(i)) for i in range(1, edges.Extent() + 1)]


def raw_edges(shape):
    e = TopExp_Explorer(shape, TopAbs_EDGE)
    result = []
    while e.More():
        result.append(TopoDS.Edge_s(e.Current()))
        e.Next()
    return result


def resolve(edges, anchor):
    x, y, z, old_length = anchor
    if old_length <= 0:
        return None
    matches = []
    for index, edge in enumerate(edges, start=1):
        xx, yy, zz, length = descriptor(edge)
        if length <= 0:
            continue
        dist = math.dist((x, y, z), (xx, yy, zz))
        score = dist / max(1.0, old_length) + abs(math.log(length / old_length))
        matches.append((score, index))
    matches.sort()
    if not matches or matches[0][0] > .5:
        return None
    if len(matches) >= 2 and matches[1][0] - matches[0][0] < .01:
        return None
    return matches[0][1]


class EdgeAnchorReferenceTests(unittest.TestCase):
    def test_box_has_repeated_raw_edges_but_twelve_distinct_edges(self):
        shape = cq.Solid.makeBox(30, 20, 10).wrapped
        self.assertEqual(len(raw_edges(shape)), 24)
        self.assertEqual(len(unique_edges(shape)), 12)

    def test_raw_traversal_fails_but_unique_map_round_trips_every_edge(self):
        shape = cq.Solid.makeBox(30, 20, 10).wrapped
        raw = raw_edges(shape)
        unique = unique_edges(shape)
        # The old code considers a second face occurrence of the same edge
        # as a distinct, equally scored candidate, so even edge #1 fails.
        self.assertIsNone(resolve(raw, descriptor(raw[0])))
        for i, edge in enumerate(unique, start=1):
            self.assertEqual(resolve(unique, descriptor(edge)), i)

    def test_distinct_identically_placed_edges_still_fail_closed(self):
        edge_a = cq.Edge.makeLine(cq.Vector(0, 0, 0), cq.Vector(10, 0, 0))
        edge_b = cq.Edge.makeLine(cq.Vector(0, 0, 0), cq.Vector(10, 0, 0))
        # Separate underlying topology with an identical centroid and length.
        self.assertIsNone(resolve([edge_a.wrapped, edge_b.wrapped], descriptor(edge_a.wrapped)))


if __name__ == '__main__':
    unittest.main(verbosity=2)

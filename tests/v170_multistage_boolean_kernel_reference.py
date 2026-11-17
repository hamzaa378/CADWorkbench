"""Independent OCCT Python checks for two-hop ancestry semantics.

These do not compile the native C++ SceneDocument, reference resolver, or Qt UI.
"""
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse, BRepAlgoAPI_Cut
from OCP.gp import gp_Pnt
from OCP.TopAbs import TopAbs_EDGE
from OCP.TopExp import TopExp
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps


def edge_at(shape, location):
    mapping=TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(shape,TopAbs_EDGE,mapping)
    output=[]
    for i in range(1,mapping.Extent()+1):
        props=GProp_GProps()
        BRepGProp.LinearProperties_s(mapping.FindKey(i),props)
        p=props.CentreOfMass()
        if all(abs(a-b)<1e-6 for a,b in zip((p.X(),p.Y(),p.Z()),location)):
            output.append(mapping.FindKey(i))
    return output


def descendants(builder,result,original):
    available=TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(result,TopAbs_EDGE,available)
    pool=[] if builder.IsDeleted(original) else [original]
    pool+=list(builder.Modified(original))+list(builder.Generated(original))
    found=[]
    for candidate in pool:
        for i in range(1,available.Extent()+1):
            if available.FindKey(i).IsSame(candidate) and i not in found:
                found.append(i)
    return [available.FindKey(i) for i in found]


class TwoHopBooleanKernel(unittest.TestCase):
    def setUp(self):
        self.base=BRepPrimAPI_MakeBox(12,12,10).Shape()
        boss=BRepPrimAPI_MakeBox(gp_Pnt(0,0,8),6,10,5).Shape()
        pocket=BRepPrimAPI_MakeBox(gp_Pnt(0,0,8),3,10,5).Shape()
        self.first=BRepAlgoAPI_Fuse(self.base,boss);self.first.Build()
        self.second=BRepAlgoAPI_Cut(self.first.Shape(),pocket);self.second.Build()
        self.assertTrue(self.first.IsDone() and self.second.IsDone())

    def test_unique_edge_descendant_through_two_real_booleans(self):
        selected=edge_at(self.base,(6,0,0))
        self.assertEqual(len(selected),1)
        after_first=descendants(self.first,self.first.Shape(),selected[0])
        self.assertEqual(len(after_first),1)
        after_second=descendants(self.second,self.second.Shape(),after_first[0])
        self.assertEqual(len(after_second),1)
        self.assertTrue(after_second[0].IsSame(edge_at(self.second.Shape(),(6,0,0))[0]))

    def test_intermediate_edge_has_own_tracked_successor(self):
        intermediate=edge_at(self.first.Shape(),(6,0,0))
        self.assertEqual(len(intermediate),1)
        final=descendants(self.second,self.second.Shape(),intermediate[0])
        self.assertEqual(len(final),1)

    def test_removed_reference_is_not_resurrected(self):
        original=edge_at(self.base,(6,0,0))[0]
        remove=BRepPrimAPI_MakeBox(gp_Pnt(0,-2,-2),12,4,4).Shape()
        third=BRepAlgoAPI_Cut(self.base,remove);third.Build()
        self.assertTrue(third.IsDone())
        self.assertFalse(descendants(third,third.Shape(),original))

if __name__=='__main__':unittest.main()

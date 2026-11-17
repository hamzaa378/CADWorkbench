"""Independent OCP verification of actual unequal-size edge operations.

This checks OCCT geometry on a reference shape, not the C++ SceneDocument
persistence path: only Windows native CTest #45 covers the latter.
"""
import unittest
try:
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
    from OCP.BRepFilletAPI import BRepFilletAPI_MakeFillet, BRepFilletAPI_MakeChamfer
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.TopExp import TopExp
    from OCP.TopAbs import TopAbs_EDGE
    from OCP.TopTools import TopTools_IndexedMapOfShape
    from OCP.TopoDS import TopoDS
except ImportError:
    BRepPrimAPI_MakeBox = None

@unittest.skipIf(BRepPrimAPI_MakeBox is None, "Independent OCCT Python bindings unavailable")
class ActualGroupedEdgeGeometry(unittest.TestCase):
    def setUp(self):
        self.stock = BRepPrimAPI_MakeBox(40,30,20).Shape()
        self.edges = TopTools_IndexedMapOfShape()
        TopExp.MapShapes_s(self.stock, TopAbs_EDGE, self.edges)
        self.assertEqual(self.edges.Extent(),12)

    @staticmethod
    def volume(shape):
        props=GProp_GProps()
        BRepGProp.VolumeProperties_s(shape,props)
        return props.Mass()

    def rebuild(self,constructor,indices,sizes):
        op=constructor(self.stock)
        for index,size in zip(indices,sizes):
            op.Add(size,TopoDS.Edge_s(self.edges.FindKey(index)))
        op.Build()
        self.assertTrue(op.IsDone(),"The actual OCCT kernel rejected a viable blend")
        return self.volume(op.Shape())

    def test_unequal_fillet_changes_limited_stock(self):
        amount=self.rebuild(BRepFilletAPI_MakeFillet,(1,3),(.7,1.3))
        self.assertGreater(amount,23000)
        self.assertLess(amount,24000)

    def test_unequal_chamfer_changes_limited_stock(self):
        amount=self.rebuild(BRepFilletAPI_MakeChamfer,(1,3),(.7,1.3))
        self.assertGreater(amount,23000)
        self.assertLess(amount,24000)

    def test_fillet_smaller_sizes_remove_less_stock(self):
        big=self.rebuild(BRepFilletAPI_MakeFillet,(1,3),(.7,1.3))
        small=self.rebuild(BRepFilletAPI_MakeFillet,(1,3),(.3,.5))
        self.assertGreater(small,big)

    def test_chamfer_smaller_sizes_remove_less_stock(self):
        big=self.rebuild(BRepFilletAPI_MakeChamfer,(1,3),(.7,1.3))
        small=self.rebuild(BRepFilletAPI_MakeChamfer,(1,3),(.3,.5))
        self.assertGreater(small,big)

    def test_three_distinct_fillet_edges(self):
        two=self.rebuild(BRepFilletAPI_MakeFillet,(1,3),(.7,1.3))
        three=self.rebuild(BRepFilletAPI_MakeFillet,(1,3,5),(.7,1.3,.9))
        self.assertLess(three,two)

    def test_three_distinct_chamfer_edges(self):
        two=self.rebuild(BRepFilletAPI_MakeChamfer,(1,3),(.7,1.3))
        three=self.rebuild(BRepFilletAPI_MakeChamfer,(1,3,5),(.7,1.3,.9))
        self.assertLess(three,two)

if __name__=="__main__":unittest.main()

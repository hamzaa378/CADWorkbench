"""Independent OpenCascade test for face-local polygon boss/pocket.
This is not a native application build or a replacement for MSVC CTests.
"""
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox,BRepPrimAPI_MakePrism
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakePolygon,BRepBuilderAPI_MakeFace
from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse,BRepAlgoAPI_Cut
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Pnt,gp_Vec

class FaceSketchReference(unittest.TestCase):
    def mass(self,shape):
        prop=GProp_GProps();BRepGProp.VolumeProperties_s(shape,prop);return prop.Mass()
    def polygon_tool(self,center,normal,x,pocket,depth):
        normal=gp_Vec(*normal);x=gp_Vec(*x)
        x.Subtract(normal.Multiplied(x.Dot(normal)));x.Normalize()
        y=normal.Crossed(x)
        center=gp_Pnt(*center)
        overlap=min(0.01,depth*0.05)
        offset=center.Translated(normal.Multiplied(overlap if pocket else -overlap))
        path=BRepBuilderAPI_MakePolygon()
        for u,v in [(-4,-4),(4,-4),(4,4),(-4,4)]:
            path.Add(offset.Translated(x.Multiplied(u)+y.Multiplied(v)))
        path.Close()
        face=BRepBuilderAPI_MakeFace(path.Wire()).Face()
        maker=BRepPrimAPI_MakePrism(face,normal.Multiplied(-(depth+overlap) if pocket else depth+overlap),True,True)
        maker.Build()
        self.assertTrue(maker.IsDone())
        self.assertTrue(BRepCheck_Analyzer(maker.Shape()).IsValid())
        return maker.Shape()
    def test_three_planar_orientations(self):
        base=BRepPrimAPI_MakeBox(30,30,10).Shape()
        self.assertEqual(self.mass(base),9000)
        for name,center,normal,x in [
            ('top',(15,15,10),(0,0,1),(1,0,0)),
            ('front',(15,0,5),(0,-1,0),(1,0,0)),
            ('side',(30,15,5),(1,0,0),(0,1,0)),
        ]:
            for pocket in (False,True):
                with self.subTest(name=name,pocket=pocket):
                    tool=self.polygon_tool(center,normal,x,pocket,5)
                    operation=BRepAlgoAPI_Cut(base,tool) if pocket else BRepAlgoAPI_Fuse(base,tool)
                    operation.Build()
                    self.assertTrue(operation.IsDone())
                    self.assertTrue(BRepCheck_Analyzer(operation.Shape()).IsValid())
                    expected=9000+(-320 if pocket else 320)
                    self.assertAlmostEqual(self.mass(operation.Shape()),expected,places=4)

if __name__=='__main__':unittest.main(verbosity=2)

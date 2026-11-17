"""Independent Python/OCP reference geometry checks (not a native C++ build)."""
import math
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox,BRepPrimAPI_MakePrism,BRepPrimAPI_MakeRevol
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakePolygon,BRepBuilderAPI_MakeFace
from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections
from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse,BRepAlgoAPI_Cut
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Pnt,gp_Vec,gp_Ax1,gp_Dir

class PolygonGeometry(unittest.TestCase):
    def wire(self,points,z=0.,radial=False):
        maker=BRepBuilderAPI_MakePolygon()
        for x,y in points:maker.Add(gp_Pnt(x,0,y) if radial else gp_Pnt(x,y,z))
        maker.Close();return maker.Wire()
    def volume(self,shape):
        props=GProp_GProps();BRepGProp.VolumeProperties_s(shape,props);return props.Mass()
    def test_four_operations(self):
        rectangle=[(5,5),(12,5),(12,12),(5,12)]
        upper=[(3,3),(14,3),(14,14),(3,14)]
        radial=[(4,4),(8,4),(8,16),(4,16)]
        base=BRepPrimAPI_MakeBox(30,30,10).Shape()
        for op in ('extrude','pocket','revolve','loft'):
            with self.subTest(op=op):
                if op=='revolve':
                    make=BRepPrimAPI_MakeRevol(BRepBuilderAPI_MakeFace(self.wire(radial,radial=True)).Face(),
                        gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),2*math.pi,True)
                    make.Build();self.assertTrue(make.IsDone());tool=make.Shape()
                elif op=='loft':
                    make=BRepOffsetAPI_ThruSections(True,True)
                    make.AddWire(self.wire(rectangle,8));make.AddWire(self.wire(upper,20));make.Build()
                    self.assertTrue(make.IsDone());tool=make.Shape()
                else:
                    z,height=(1,10) if op=='pocket' else (8,12)
                    make=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(self.wire(rectangle,z)).Face(),
                                               gp_Vec(0,0,height),True,True)
                    make.Build();self.assertTrue(make.IsDone());tool=make.Shape()
                self.assertTrue(BRepCheck_Analyzer(tool).IsValid())
                operation=BRepAlgoAPI_Cut(base,tool) if op=='pocket' else BRepAlgoAPI_Fuse(base,tool)
                operation.Build();self.assertTrue(operation.IsDone());result=operation.Shape()
                self.assertTrue(BRepCheck_Analyzer(result).IsValid())
                self.assertLess(self.volume(result),self.volume(base)) if op=='pocket' else self.assertGreater(self.volume(result),self.volume(base))
                self.assertGreater(self.volume(tool),1.0)

if __name__=='__main__':unittest.main(verbosity=2)

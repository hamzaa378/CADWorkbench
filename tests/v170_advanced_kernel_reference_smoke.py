"""Independent OpenCascade reference for new v1.70 geometry operations.
Not a substitute for MSVC and the app's native Qt/OCCT tests.
"""
import math
import unittest
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeRevol
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakePolygon, BRepBuilderAPI_MakeFace, BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeWire
from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections, BRepOffsetAPI_MakePipeShell
from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Pnt,gp_Vec,gp_Dir,gp_Ax1
from OCP.GeomAPI import GeomAPI_Interpolate
from OCP.TColgp import TColgp_HArray1OfPnt

class AdvancedGeometryReference(unittest.TestCase):
    def volume(self, shape):
        v=GProp_GProps();BRepGProp.VolumeProperties_s(shape,v);return v.Mass()
    def wire(self, points):
        w=BRepBuilderAPI_MakePolygon()
        for x,y,z in points:w.Add(gp_Pnt(x,y,z))
        w.Close();return w.Wire()
    def checked(self,tool):
        self.assertTrue(BRepCheck_Analyzer(tool).IsValid())
        self.assertGreater(self.volume(tool),1)
        base=BRepPrimAPI_MakeBox(30,30,10).Shape()
        fused=BRepAlgoAPI_Fuse(base,tool);fused.Build()
        self.assertTrue(fused.IsDone())
        self.assertTrue(BRepCheck_Analyzer(fused.Shape()).IsValid())
        self.assertGreater(self.volume(fused.Shape()),self.volume(base)+1)
    def test_sweep_3d(self):
        spine=BRepBuilderAPI_MakePolygon()
        for p in [(10,10,8),(10,10,22)]:spine.Add(gp_Pnt(*p))
        section=self.wire([(8,8,8),(12,8,8),(12,12,8),(8,12,8)])
        pipe=BRepOffsetAPI_MakePipeShell(spine.Wire());pipe.Add(section)
        pipe.Build();self.assertTrue(pipe.IsDone())
        self.assertTrue(pipe.MakeSolid())
        self.checked(pipe.Shape())
    def test_interpolated_spline_sweep_3d(self):
        stations=[(10,10,8),(10,10,14),(10,12,19),(12,15,24)]
        points=TColgp_HArray1OfPnt(1,len(stations))
        for i,xyz in enumerate(stations):points.SetValue(i+1,gp_Pnt(*xyz))
        interpolation=GeomAPI_Interpolate(points,False,1e-6)
        interpolation.Perform();self.assertTrue(interpolation.IsDone())
        curve=interpolation.Curve()
        derivative=curve.DN(curve.FirstParameter(),1)
        derivative.Normalize()
        reference=gp_Vec(0,0,1) if abs(derivative.Z())<0.9 else gp_Vec(0,1,0)
        ux=reference.Crossed(derivative);ux.Normalize()
        uy=derivative.Crossed(ux);uy.Normalize()
        start=gp_Pnt(*stations[0]);outline=[]
        for x,y in [(-2,-2),(2,-2),(2,2),(-2,2)]:
            p=start.Translated(ux.Multiplied(x)+uy.Multiplied(y))
            outline.append((p.X(),p.Y(),p.Z()))
        spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(curve).Edge()).Wire()
        pipe=BRepOffsetAPI_MakePipeShell(spine)
        pipe.Add(self.wire(outline));pipe.Build()
        self.assertTrue(pipe.IsDone());self.assertTrue(pipe.MakeSolid())
        self.checked(pipe.Shape())
    def test_multi_loft(self):
        maker=BRepOffsetAPI_ThruSections(True,False)
        for z,p in [(8,[(5,5),(12,5),(12,12),(5,12)]),
                    (15,[(3,3),(14,3),(14,14),(3,14)]),
                    (24,[(5,5),(12,5),(12,12),(5,12)])]:
            maker.AddWire(self.wire([(x,y,z) for x,y in p]))
        maker.Build();self.assertTrue(maker.IsDone());self.checked(maker.Shape())
    def test_arbitrary_axis_revolve(self):
        for direction in [gp_Dir(0,0,1),gp_Dir(1,0,0)]:
            with self.subTest(direction=direction):
                along=gp_Vec(direction)
                reference=gp_Vec(0,0,1) if abs(direction.Z())<0.9 else gp_Vec(0,1,0)
                radial=reference.Crossed(along);radial.Normalize()
                center=gp_Pnt(10,10,0) if direction.Z()>0.9 else gp_Pnt(0,10,10)
                points=[]
                for radius,z in [(4,4),(8,4),(8,16),(4,16)]:
                    point=center.Translated(radial.Multiplied(radius)+along.Multiplied(z))
                    points.append((point.X(),point.Y(),point.Z()))
                maker=BRepPrimAPI_MakeRevol(BRepBuilderAPI_MakeFace(self.wire(points)).Face(),
                    gp_Ax1(center,direction),2*math.pi,True)
                maker.Build();self.assertTrue(maker.IsDone());self.checked(maker.Shape())

if __name__=='__main__':unittest.main(verbosity=2)

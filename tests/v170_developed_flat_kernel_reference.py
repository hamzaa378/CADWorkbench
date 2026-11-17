"""Independent OCCT reference for through-thickness paired bend reliefs.

This exercises Python OCCT, not the native CADWorkbench C++ code. Native
Windows CTest and GUI persistence still require independent verification.
"""
import math
import unittest
from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepGProp import BRepGProp
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Pnt


def volume(shape):
    props=GProp_GProps()
    BRepGProp.VolumeProperties_s(shape,props)
    return props.Mass()


def developed(flanges,width,thickness,bends,k,relief):
    positions=[]
    length=0.0
    for i,flange in enumerate(flanges):
        length+=flange
        if i<len(bends):
            angle,radius=bends[i]
            bend_length=abs(angle)*math.pi/180*(radius+k[i]*thickness)
            positions.append(length+bend_length/2)
            length+=bend_length
    w,d=relief
    if bool(w)==bool(d) and 0<=d<width/2 and all(
        w/2<point<length-w/2 for point in positions
    ) and all((b-a)>w for a,b in zip(positions,positions[1:])):
        return length,positions
    raise ValueError('overlapping or invalid relief')


def shape_from_stations(length,positions,width,thickness,relief):
    shape=BRepPrimAPI_MakeBox(length,width,thickness).Shape()
    w,d=relief
    if w:
        for station in positions:
            for lo,hi in [(-thickness,d),(width-d,width+thickness)]:
                tool=BRepPrimAPI_MakeBox(gp_Pnt(station-w/2,lo,-thickness),
                    gp_Pnt(station+w/2,hi,2*thickness)).Shape()
                cut=BRepAlgoAPI_Cut(shape,tool)
                cut.Build()
                if not cut.IsDone() or not BRepCheck_Analyzer(cut.Shape()).IsValid():
                    raise AssertionError('invalid OCCT cut')
                shape=cut.Shape()
    return shape


class DevelopedFlatKernelReference(unittest.TestCase):
    def test_two_bends_four_notches(self):
        length,stations=developed([30,20,30],25,1,[(90,2),(-90,2)],
            [.4,.45],(3,2.5))
        shape=shape_from_stations(length,stations,25,1,(3,2.5))
        self.assertTrue(BRepCheck_Analyzer(shape).IsValid())
        self.assertAlmostEqual(volume(shape),length*25-4*3*2.5,places=5)

    def test_source_edit_and_relief_edit_recalculate(self):
        l1,s1=developed([30,20,30],25,1,[(90,2),(-90,2)],
            [.4,.45],(3,2.5))
        l2,s2=developed([35,20,30],25,1,[(90,2),(-90,2)],
            [.4,.45],(4,2.5))
        self.assertAlmostEqual(l2-l1,5.0)
        v1=volume(shape_from_stations(l1,s1,25,1,(3,2.5)))
        v2=volume(shape_from_stations(l2,s2,25,1,(4,2.5)))
        self.assertAlmostEqual(v2-v1,5*25-4*1*2.5,places=5)

    def test_overlapping_and_overdeep_rejected(self):
        with self.assertRaises(ValueError):
            developed([10,2,10],25,1,[(90,2),(-90,2)],[.4,.45],(10,2.5))
        with self.assertRaises(ValueError):
            developed([30,20,30],25,1,[(90,2),(-90,2)],[.4,.45],(3,13))

if __name__=='__main__':unittest.main()

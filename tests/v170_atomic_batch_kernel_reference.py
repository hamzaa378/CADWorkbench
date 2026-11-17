"""Independent OCCT solid-volume reference for the atomic batch C++ fixture.

Only confirms the geometric values used by native tests. The actual batch,
undo, persistence, and transaction semantics require MSVC CTests.
"""
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse, BRepAlgoAPI_Common
from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
from OCP.gp import gp_Trsf, gp_Vec
from OCP.GProp import GProp_GProps
from OCP.BRepGProp import BRepGProp
from OCP.BRepCheck import BRepCheck_Analyzer


def box(x):
    shape=BRepPrimAPI_MakeBox(20,20,10).Shape()
    trsf=gp_Trsf()
    trsf.SetTranslation(gp_Vec(x,0,0))
    return BRepBuilderAPI_Transform(shape,trsf,True).Shape()


def evaluate(mode,a,b):
    op=(BRepAlgoAPI_Common if mode=='common' else BRepAlgoAPI_Fuse)(a,b)
    op.Build()
    assert op.IsDone() and not op.Shape().IsNull()
    shape=op.Shape()
    assert BRepCheck_Analyzer(shape).IsValid(),"Invalid reference shape"
    props=GProp_GProps()
    BRepGProp.VolumeProperties_s(shape,props)
    return shape,props.Mass()


def close(actual,expected):
    assert abs(actual-expected)<0.01,(actual,expected)


a,b,c=box(0),box(10),box(15)
original_x,volume=evaluate('fuse',a,b);close(volume,6000)
_,volume=evaluate('fuse',original_x,c);close(volume,7000)
# Coordinated final DAG: y=Fuse(a,b), x=Common(a,y), z=Fuse(x,c).
final_y,volume=evaluate('fuse',a,b);close(volume,6000)
final_x,volume=evaluate('common',a,final_y);close(volume,4000)
_,volume=evaluate('fuse',final_x,c);close(volume,7000)
# A correctly rejected attempt uses an isolated tool with no common solid.
_,volume=evaluate('common',a,box(200));close(volume,0)
print('PASS: six independent OCCT volume reference checks for atomic batch fixture')

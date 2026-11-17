"""Independent CAD-kernel volume reference for the retargeted-body native fixture.

Requires OCP; deliberately does not import CADWorkbench sources. The native
SceneDocument DAG and save/undo behavior are tested only by Windows CTests.
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

def operation(mode,left,right):
    algo=(BRepAlgoAPI_Fuse if mode=='fuse' else BRepAlgoAPI_Common)(left,right)
    algo.Build()
    assert algo.IsDone(),f'{mode}: OCCT operation was not completed'
    shape=algo.Shape()
    assert not shape.IsNull() and BRepCheck_Analyzer(shape).IsValid(),f'{mode}: invalid result'
    properties=GProp_GProps()
    BRepGProp.VolumeProperties_s(shape,properties)
    return shape,properties.Mass()

def check(label,actual,expected):
    assert abs(actual-expected)<0.01,f'{label}: {actual} != {expected}'
    print(f'PASS {label}: {actual:.3f} mm³')

A,B,C=box(0),box(10),box(15)
L,vol=operation('fuse',A,B);check('initial linked union',vol,6000)
D,vol=operation('fuse',L,A);check('initial downstream union',vol,6000)
E,vol=operation('common',B,C);check('later independent source',vol,3000)
L,vol=operation('common',A,E);check('retargeted earlier result',vol,1000)
D,vol=operation('fuse',L,A);check('retargeted downstream result',vol,4000)
E,vol=operation('common',B,box(17));check('translated later source',vol,2600)
L,vol=operation('common',A,E);check('indirect earlier regeneration',vol,600)
D,vol=operation('fuse',L,A);check('indirect downstream regeneration',vol,4000)
print('PASS: 8/8 independent OpenCascade reference volumes')

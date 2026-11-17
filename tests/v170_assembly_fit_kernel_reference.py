"""Independent OpenCascade reference for the four static assembly-fit states."""
import math
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepAlgoAPI import BRepAlgoAPI_Common
from OCP.BRepExtrema import BRepExtrema_DistShapeShape
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.gp import gp_Pnt

a = BRepPrimAPI_MakeBox(20,20,10).Shape()
for shift, expected_dist, expected_overlap, state in [
    (28,8,0,"Clear"), (23,3,0,"Below clearance"),
    (20,0,0,"Contact"), (16,0,800,"Interference")
]:
    b = BRepPrimAPI_MakeBox(gp_Pnt(shift,0,0),20,20,10).Shape()
    dist = BRepExtrema_DistShapeShape(a,b)
    dist.Perform()
    assert dist.IsDone()
    common = BRepAlgoAPI_Common(a,b)
    common.Build()
    assert common.IsDone() and not common.Shape().IsNull()
    props=GProp_GProps()
    BRepGProp.VolumeProperties_s(common.Shape(),props)
    actual_dist=dist.Value()
    actual_overlap=props.Mass()
    assert math.isclose(actual_dist,expected_dist,abs_tol=1e-6),(shift,actual_dist)
    assert math.isclose(actual_overlap,expected_overlap,abs_tol=1e-5),(shift,actual_overlap)
    print(f"{state:20}: min distance={actual_dist:.2f} mm, shared volume={actual_overlap:.2f} mm³")
print("4 independent OpenCascade cases passed")

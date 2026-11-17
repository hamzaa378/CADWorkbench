"""Independent OpenCascade local-frame observations, not SceneDocument tests."""
import math
import unittest
from OCP.gp import gp_Trsf,gp_Pnt,gp_Vec


def frame(angle, dx, dy, dz):
    tr=gp_Trsf()
    c,s=math.cos(angle),math.sin(angle)
    tr.SetValues(c,-s,0,dx,s,c,0,dy,0,0,1,dz)
    return tr


class LocalFrameReference(unittest.TestCase):
    def test_world_point_local_roundtrip(self):
        tr=frame(0.37,25,-8,19)
        p=gp_Pnt(34,5,28)
        local=p.Transformed(tr.Inverted())
        self.assertLess(local.Transformed(tr).Distance(p),1e-9)

    def test_rotation_directions_do_not_translate(self):
        tr=frame(-0.82,1200,-500,20)
        vec=gp_Vec(1,0,0)
        local=vec.Transformed(tr.Inverted())
        world=local.Transformed(tr)
        self.assertAlmostEqual(world.X(),vec.X(),places=8)
        self.assertAlmostEqual(world.Y(),vec.Y(),places=8)
        self.assertAlmostEqual(world.Z(),vec.Z(),places=8)

    def test_two_component_frame_points_follow_movement(self):
        parent=frame(0.1,12,8,0)
        child=frame(-0.4,-7,3,0)
        parentWorld=gp_Pnt(13,18,0)
        childWorld=gp_Pnt(-6,13,0)
        localA=parentWorld.Transformed(parent.Inverted())
        localB=childWorld.Transformed(child.Inverted())
        move=frame(0.25,5,1,-2)
        movedParent=move.Multiplied(parent)
        movedWorld=localA.Transformed(movedParent)
        self.assertLess(movedWorld.Distance(parentWorld.Transformed(move)),1e-9)
        self.assertLess(localB.Transformed(child).Distance(childWorld),1e-9)


if __name__=='__main__':unittest.main()

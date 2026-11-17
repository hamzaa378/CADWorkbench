"""Optional independent OpenCascade/CadQuery geometry oracle for additive boss.

Reference test only; not a substitute for the native Windows SceneDocument tests.
"""
import unittest
import cadquery as cq

def block(w=12,h=12,d=10):
    return cq.Workplane('XY').box(w,h,d,centered=(False,False,False)).val()

def cutter(w=10,d=5,offset=8):
    return cq.Workplane('XY').box(w,10,d,centered=(False,False,False)).translate((0,0,offset)).val()

def verify_boss(target, tool, gained, connected=True):
    fused=target.fuse(tool)
    solids=fused.Solids()
    observed=fused.Volume()-target.Volume()
    assert abs(observed-gained)<1e-5,(observed,gained)
    assert (len(solids)==1)==connected,(len(solids),connected)
    return fused

class BossKernelReference(unittest.TestCase):
    def test_connected_addition(self):
        self.assertAlmostEqual(verify_boss(block(),cutter(),300).Volume(),1740)
    def test_sketch_edit(self):
        self.assertAlmostEqual(verify_boss(block(),cutter(8),240).Volume(),1680)
    def test_tool_edit(self):
        self.assertAlmostEqual(verify_boss(block(),cutter(8,4,7),80).Volume(),1520)
    def test_reopened_sketch_edit(self):
        self.assertAlmostEqual(verify_boss(block(),cutter(9),270).Volume(),1710)
    def test_buried_boss_is_ineffective(self):
        result=verify_boss(block(),cutter(10,4,2),0)
        self.assertAlmostEqual(result.Volume(),1440)
    def test_detached_boss_stays_multiple_solids(self):
        result=verify_boss(block(),cutter(10,5,50),500,False)
        self.assertEqual(len(result.Solids()),2)
    def test_touching_face_fuses_into_single_solid(self):
        result=verify_boss(block(),cutter(10,5,10),500)
        self.assertEqual(len(result.Solids()),1)
    def test_multiple_contacts_preserve_volume(self):
        target=cq.Workplane('XY').box(10,10,10,centered=(False,False,False)).val()
        result=verify_boss(target,cutter(10,5,8),300)
        self.assertAlmostEqual(result.Volume(),1300)
if __name__=='__main__':unittest.main()

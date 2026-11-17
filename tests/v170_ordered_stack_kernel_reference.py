"""Independent OpenCascade/CadQuery oracle for the bounded ordered in-place sketch stack.
It does not execute the native Qt SceneDocument implementation.
"""
import unittest
import cadquery as cq


def tool(width, depth=5, offset=8):
    return cq.Workplane('XY').box(width, 10, depth, centered=(False, False, False)).translate((0, 0, offset)).val()


class OrderedStackKernelReference(unittest.TestCase):
    def test_four_stages_and_source_edit(self):
        body=cq.Workplane('XY').box(12, 12, 10, centered=(False, False, False)).val()
        first=body.fuse(tool(10))
        second=first.cut(tool(5))
        third=second.fuse(tool(4,8))
        fourth=third.cut(tool(2,3,11))
        for solid, expected in [(body,1440),(first,1740),(second,1490),(third,1810),(fourth,1750)]:
            self.assertAlmostEqual(solid.Volume(),expected,places=5)
            self.assertEqual(len(solid.Solids()),1)
        revised=second.fuse(tool(3,8))
        edited=revised.cut(tool(2,3,11))
        self.assertAlmostEqual(revised.Volume(),1730,places=5)
        self.assertAlmostEqual(edited.Volume(),1670,places=5)
        self.assertEqual(len(edited.Solids()),1)

    def test_invalid_detached_addition_is_separate_solid(self):
        body=cq.Workplane('XY').box(12,12,10,centered=(False,False,False)).val()
        base=body.fuse(tool(10)).cut(tool(5))
        detached=base.fuse(tool(4,8,100))
        self.assertEqual(len(detached.Solids()),2)


if __name__ == '__main__':unittest.main()

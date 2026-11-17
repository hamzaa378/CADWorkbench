"""Conservative source checks; native OCCT regression supplies geometry evidence."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class DirectionalHoleIntegration(unittest.TestCase):
    def test_public_axis_and_planar_reference_api(self):
        header=(ROOT/'src/core/SceneDocument.h').read_text()
        self.assertIn('gp_Dir inwardAxis',header)
        self.assertIn('prepareHoleOnPlanarFace',header)
    def test_axis_spans_full_arbitrary_bounds(self):
        code=(ROOT/'src/core/SceneDocument.cpp').read_text()
        self.assertIn('directional_holes::throughAllSpan',code)
        self.assertIn('command.inwardAxis',code)
        self.assertIn('command->inwardAxis=gp_Dir(0,0,-1)',code)
        self.assertIn('const gp_Ax2 headAxis(',code)
    def test_commit_rebuilds_descendants_before_checkpoint(self):
        code=(ROOT/'src/core/SceneDocument.cpp').read_text()
        block=code.split('bool SceneDocument::commitHole(',1)[1].split('bool SceneDocument::prepareDovetailGroove',1)[0]
        self.assertLess(block.index('propagateLinkedBodies(&candidate,error)'),block.index('checkpoint()'))
        self.assertIn('parts_=std::move(candidate)',block)
    def test_editable_non_axis_aligned_feature(self):
        tree=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text()
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text()
        self.assertIn('Cylinder cut arbitrary axis',tree)
        self.assertIn('lengthSquared("axis_x","axis_y","axis_z")',tree)
        self.assertIn('Cylinder cut arbitrary axis',ui)
    def test_ui_exposes_selected_face_drilling(self):
        code=(ROOT/'src/ui/MainWindow.cpp').read_text()
        block=code.split('void MainWindow::addMetricFastenerClearanceHole()',1)[1].split('void MainWindow::addParametricThroughHole()',1)[0]
        self.assertIn('selectedFaceReferences()',block)
        self.assertIn('prepareHoleOnPlanarFace(',block)
        self.assertIn('selected planar face',block)
if __name__=='__main__':unittest.main()

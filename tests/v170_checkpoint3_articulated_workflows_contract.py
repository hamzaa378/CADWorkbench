"""Source-integration guards only; native Qt/OpenCascade testing is required."""
import pathlib
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]

class Checkpoint3WorkflowContract(unittest.TestCase):
    def test_public_read_only_trajectory_method(self):
        header=(ROOT/'src/core/SceneDocument.h').read_text(encoding="utf-8")
        self.assertIn('bool previewLiveArticulatedTrajectory(',header)
        self.assertIn('int sampleCount,QString* report',header)

    def test_trajectory_is_staged_and_output_atomic(self):
        code=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        start=code.index('bool SceneDocument::previewLiveArticulatedTrajectory(')
        end=code.index('bool SceneDocument::updateLiveArticulatedCoordinates(',start)
        block=code[start:end]
        self.assertIn('previewLiveArticulatedCoordinates(frame,&frameReport',block)
        self.assertIn('applyArticulatedTargets(',block)
        self.assertIn('*report=lines.join',block)
        self.assertLess(block.index('previewLiveArticulatedCoordinates'),block.index('*report=lines.join'))
        self.assertNotIn('checkpoint()',block)
        self.assertNotIn('parts_=',block)

    def test_editor_uses_sampled_preview(self):
        code=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        start=code.index('void MainWindow::editSavedArticulatedDrives()')
        end=code.index('void MainWindow::auditLiveRigidMateStructure()',start)
        block=code[start:end]
        self.assertIn('previewLiveArticulatedTrajectory(',block)
        self.assertLess(block.index('previewLiveArticulatedTrajectory('),block.index('commitLiveArticulatedCoordinatesChecked('))
        self.assertIn('QCheckBox(',block)
        self.assertIn('frames->setRange(2,12)',block)

    def test_joint_frame_editor_preserves_preview_before_commit(self):
        code=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        start=code.index('void MainWindow::editSavedArticulatedJointDefinition()')
        end=code.index('void MainWindow::editSavedArticulatedDrives()',start)
        block=code[start:end]
        self.assertIn('cad::joint_frame::toEuler(',block)
        self.assertIn('cad::joint_frame::fromEuler(',block)
        self.assertLess(block.index('previewLiveArticulatedJointDefinitionEdit('),block.index('replaceLiveArticulatedJointDefinition('))

    def test_native_regressions_are_extended(self):
        code=(ROOT/'tests/articulated_assembly_scene_v170_tests.cpp').read_text(encoding="utf-8")
        self.assertIn('previewLiveArticulatedTrajectory(',code)
        self.assertIn('editedDefinition.childAtZero.rotation={{',code)
        self.assertIn('invalid sample budget overwrote caller output',code)

    def test_portable_frame_math_is_registered(self):
        script=(ROOT/'scripts/verify_portable_consolidation.sh').read_text(encoding="utf-8")
        self.assertIn('compile_run joint_frame_euler',script)
        self.assertTrue((ROOT/'tests/articulated_frame_euler_portable_tests.cpp').exists())

if __name__=='__main__': unittest.main()

"""Source integration contracts for the unreleased parametric/drawing UI increment.

These tests intentionally do not assert that Qt widgets have been built or
clicked on Windows. Native CTest and manual GUI verification are required.
"""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

class UiWorkstreamIncrementContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sketch=(ROOT/'src/ui/SketchCanvasDialog.cpp').read_text(encoding="utf-8")
        cls.sketch_h=(ROOT/'src/ui/SketchCanvasDialog.h').read_text(encoding="utf-8")
        cls.native=(ROOT/'tests/sketch_constraint_editor_cases.h').read_text(encoding="utf-8")
        cls.sketch_native=(ROOT/'tests/sketch_solver_tests.cpp').read_text(encoding="utf-8")
        cls.cmake=(ROOT/'CMakeLists.txt').read_text(encoding="utf-8")
        cls.draw=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        cls.draw_native=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding="utf-8")
    def test_constraint_manager_calls_transactional_solver_and_updates_radius(self):
        for marker in ('constraintDescriptions() const','changeConstraintDimension(int index,double newValue)',
                       'removeConstraint(int index)'):
            self.assertIn(marker,self.sketch_h)
        for marker in ('trial.constraints[index].value=value;',
                       'return tryRelation(std::move(trial),"Sketch dimension edited")',
                       'return tryRelation(std::move(trial),"Sketch constraint removed")',
                       'trial.circles[i].radius=solved.solvedCircleRadii[i]',
                       'if(closed_&&boundary_.size()>=3)'):
            self.assertIn(marker,self.sketch)
    def test_constraint_ui_and_native_regression_in_existing_ctest(self):
        for marker in ('Manage constraints...', 'Edit dimension...', 'Remove constraint',
                       'QInputDialog::getDouble(&dialog',
                       'if(!canvas_->changeConstraintDimension(index,entry/factor))',
                       'if(!canvas_->removeConstraint(index))'):
            self.assertIn(marker,self.sketch)
        for marker in ('void lineDimensionRoundtrip()', 'void circleRadiusRoundtrip()',
                       'Overconstrained dimension edit incorrectly succeeded',
                       'Radius solver result was not propagated',
                       'view.undoEdit();'):
            self.assertIn(marker,self.native)
        self.assertIn('sketch_editor_test::lineDimensionRoundtrip()',self.sketch_native)
        self.assertIn('sketch_editor_test::circleRadiusRoundtrip()',self.sketch_native)
        self.assertIn('void fixedPointAnchorRoundtrip()',self.native)
        self.assertIn('sketch_editor_test::fixedPointAnchorRoundtrip()',self.sketch_native)
        self.assertIn('Qt6::Widgets',self.cmake)
        self.assertIn('src/ui/SketchCanvasDialog.cpp src/ui/SketchCanvasDialog.h',self.cmake)
        self.assertEqual(self.cmake.count('add_test(NAME '),130)
    def test_gdt_stable_id_inplace_edit_and_revision_reauthoring(self):
        block=self.draw.split('void MainWindow::editDrawingGdtFrames(){',1)[1].split('void MainWindow::editDrawingPartsList()',1)[0]
        for marker in ('"Edit %1 (%2)"', 'const bool editing=actionIndex>0;',
                       'previous.sourceGeometryToken!=body->geometryToken',
                       'QMessageBox::Yes|QMessageBox::No,QMessageBox::No',
                       'if(editing)settings.gdtFrames[editIndex]=frame;',
                       'if(!editing)frame.id=QUuid::createUuid()',
                       'setDrawingSheetSettings(part->id,settings,&error)'):
            self.assertIn(marker,block)
        self.assertIn('Editing geometric tolerance changed stable frame identity',self.draw_native)
        self.assertIn('Undo did not restore original tolerance frame values',self.draw_native)
        self.assertIn('Redo did not restore edited tolerance frame values',self.draw_native)
    def test_release_gate_remains_strict(self):
        gate=(ROOT/'docs/V170_RELEASE_GATE_85_85_65.md').read_text(encoding="utf-8")
        self.assertIn('17/20=85%, 13/20=65%',gate)
        self.assertIn('no ZIP release',gate)

if __name__=='__main__':unittest.main()

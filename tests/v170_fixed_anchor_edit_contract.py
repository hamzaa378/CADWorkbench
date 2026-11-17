"""Static integration checks for the sketch fixed-point anchor editor."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AnchorContract(unittest.TestCase):
    def test_solver_and_rollback(self):
        cpp=(ROOT/'src/ui/SketchCanvasDialog.cpp').read_text(encoding="utf-8")
        hdr=(ROOT/'src/ui/SketchCanvasDialog.h').read_text(encoding="utf-8")
        self.assertIn('changeFixedPointAnchor(int index,double xMm,double yMm)',hdr)
        self.assertIn('trial.constraints[index].fixedX=xMm;',cpp)
        self.assertIn('trial.constraints[index].fixedY=yMm;',cpp)
        self.assertIn('return tryRelation(std::move(trial),"Fixed-point anchor edited")',cpp)
        self.assertIn('!std::isfinite(xMm)||!std::isfinite(yMm)',cpp)
    def test_gui_and_native(self):
        gui=(ROOT/'src/ui/SketchCanvasDialog.cpp').read_text(encoding="utf-8")
        native=(ROOT/'tests/sketch_constraint_editor_cases.h').read_text(encoding="utf-8")
        self.assertIn('Edit fixed anchor...',gui)
        self.assertIn('anchorDialog.exec()!=QDialog::Accepted',gui)
        self.assertIn('changeFixedPointAnchor(index,xField->value(),yField->value())',gui)
        self.assertIn('void fixedPointAnchorRoundtrip()',native)
        self.assertIn('Undo did not restore fixed-point coordinates',native)
if __name__=='__main__':unittest.main()

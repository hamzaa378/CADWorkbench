import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class CouponIntegration(unittest.TestCase):
    def test_real_linked_blank_edit_uses_calibration_and_explicit_commit(self):
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text()
        start=ui.index('void MainWindow::editLinkedFoldedFlatDevelopment()')
        end=ui.index('void MainWindow::editFoldedSheetMetal()',start)
        body=ui[start:end]
        for word in ('inferBendCouponFactors','interpolateCouponKFactors','factors->setText',
                     'document_.editLinkedFoldedFlatDevelopment','measure.exec()','dialog.exec()'):
            self.assertIn(word,body)
        self.assertLess(body.index('factors->setText(values.join'),body.index('document_.editLinkedFoldedFlatDevelopment'))
    def test_portable_runner_includes_calibration(self):
        self.assertIn('bend_coupon_calibration_portable_tests.cpp',(ROOT/'tools/run_portable_regressions.py').read_text())
    def test_no_automatic_acceptance_mutation(self):
        source=(ROOT/'tools/verify_95_acceptance.py').read_text()
        self.assertIn('target_95_verified',source)
        self.assertNotIn('row["status"]="PASS"',source)
if __name__=='__main__':unittest.main()

"""Static integration guard; not a substitute for Windows MSVC/Qt CTests."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class MotionBoundContracts(unittest.TestCase):
    def test_declared_inputs_are_not_inferred(self):
        header=(ROOT/'src/core/AdaptiveMotionScan.h').read_text(encoding="utf-8")
        implementation=(ROOT/'src/core/AdaptiveMotionScan.cpp').read_text(encoding="utf-8")
        self.assertIn('provenRelativeSpeedBoundMmPerSecond = -1.0',header)
        self.assertIn('speedBoundConsistent',implementation)
        self.assertIn('if(!checkBound())return false;',implementation)
        self.assertIn('candidate.clearanceBoundSatisfied=boundEnabled(settings)',implementation)
        self.assertIn('intervalsWithoutClearanceProof',header)
    def test_ui_optin_and_uncertainty(self):
        source=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        self.assertIn('Use an independently justified speed bound',source)
        self.assertIn('speedBoundEnabled->isChecked()?speedBound->value():-1.0',source)
        self.assertIn('No independently established relative surface-speed bound supplied',source)
        self.assertIn('conditional',source.lower())
    def test_native_scene_tracks_atomic_error_and_unsafe_contact(self):
        source=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding="utf-8")
        self.assertIn('stationary speed bound contradicted by actual moving geometry',source)
        self.assertIn('colliding assembly safely separated',source)
        scene=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        self.assertIn('settings.provenRelativeSpeedBoundMmPerSecond=provenRelativeSpeedBoundMmPerSecond',scene)
        self.assertIn('if(clearanceBoundSatisfied)*clearanceBoundSatisfied=result.clearanceBoundSatisfied',scene)
        self.assertIn('result.conservativeClearanceLowerBoundMm',scene)
    def test_native_inventory_unchanged(self):
        import re
        self.assertEqual(len(re.findall(r'add_test\(NAME ',(ROOT/'CMakeLists.txt').read_text(encoding="utf-8"))),130)

if __name__=='__main__': unittest.main()

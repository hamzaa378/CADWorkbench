"""Sprint 9 source integration gates; portable C++ tests check actual behavior."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

def content(path):
    return (ROOT/path).read_text(encoding='utf-8')

class Sprint9FeatureContracts(unittest.TestCase):
    def test_hole_inspection_header_exists_and_registered(self):
        self.assertTrue((ROOT/'src/core/HolePositionInspection.h').exists())
        self.assertIn('src/core/HolePositionInspection.h',content('CMakeLists.txt'))

    def test_rigid_motion_header_exists_and_registered(self):
        self.assertTrue((ROOT/'src/core/RigidMotionSpeedBound.h').exists())
        self.assertIn('src/core/RigidMotionSpeedBound.h',content('CMakeLists.txt'))

    def test_hole_inspection_stages_report_atomically(self):
        body=content('src/core/HolePositionInspection.h')
        self.assertIn('HolePatternInspection staged;',body)
        self.assertIn('*report=std::move(staged);',body)
        self.assertIn('!ids.insert(h.holeId).second',body)

    def test_hole_inspection_uses_diametral_position(self):
        body=content('src/core/HolePositionInspection.h')
        self.assertIn('2*f.maximumCenterDeviationMm',body)
        self.assertIn('PositionMaterialCondition::MaximumMaterialCondition',body)
        self.assertIn('h.actualDiameterMm-h.minimumDiameterMm',body)

    def test_projected_hole_axis_is_validated(self):
        body=content('src/core/HolePositionInspection.h')
        self.assertIn('h.projectedHeightMm==0',body)
        self.assertIn('std::max(datumRadial,projectedRadial)',body)

    def test_motion_speed_bound_includes_rotation_and_translation(self):
        body=content('src/core/RigidMotionSpeedBound.h')
        self.assertIn('maximumAngularSpeedRadPerSecond*staged.firstBodyRadiusMm',body)
        self.assertIn('maximumOriginTranslationSpeedMmPerSecond',body)
        self.assertIn('relativeSurfaceSpeedBoundMmPerSecond',body)

    def test_motion_bound_only_conditionally_proves_clearance(self):
        body=content('src/core/RigidMotionSpeedBound.h')
        self.assertIn('caller MUST',body)
        self.assertIn('scanAdaptive(times,settings,evaluate',body)

    def test_new_portable_regressions_registered(self):
        runner=content('tools/run_portable_regressions.py')
        self.assertIn('hole_position_inspection_portable_tests.cpp',runner)
        self.assertIn('rigid_motion_speed_bound_portable_tests.cpp',runner)
        self.assertIn('src/core/AdaptiveMotionScan.cpp',runner)

    def test_native_workshop_regression_integrated(self):
        body=content('tests/workshop_engineering_v164_tests.cpp')
        self.assertIn('void holePositionInspection()',body)
        self.assertIn('tolerances();verifyManufacturingFitBudget();holePositionInspection();',body)

    def test_native_articulated_regression_integrated(self):
        body=content('tests/articulated_assembly_v170_tests.cpp')
        self.assertIn('calculateRigidPairSpeedBound',body)
        self.assertIn('scanRigidPairWithSpeedBounds',body)
        self.assertIn('clearanceReport.clearanceBoundSatisfied',body)

    def test_no_native_ctest_registration_changes(self):
        self.assertEqual(content('CMakeLists.txt').count('add_test('),130)

    def test_windows_targeted_build_documented(self):
        body=content('RUN_SPRINT9_WINDOWS_TARGETED_BUILD.ps1')
        self.assertIn('articulated_assembly_v170_tests',body)
        self.assertIn('workshop_engineering_v164_tests',body)
        self.assertIn('cadcore_v170_articulated_closed_loop',body)

    def test_native_motion_target_links_adaptive_scan(self):
        cmake=content('CMakeLists.txt')
        target=cmake.split('add_executable(articulated_assembly_v170_tests',1)[1].split('target_include_directories',1)[0]
        self.assertIn('src/core/AdaptiveMotionScan.cpp',target)

if __name__=='__main__':
    unittest.main()

"""Independent source wiring and audit behavior; does not fake native CTest evidence."""
import importlib.util
import math
import re
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def content(rel):return (ROOT/rel).read_text(encoding='utf-8')

class NewBackendRegressions(unittest.TestCase):
    def test_shop_allocator_has_transactional_output(self):
        source=content('src/core/ShopToleranceAllocation.h')
        self.assertIn('*output=std::move(staged)',source)
        self.assertIn('shopFloorFeasible',source)
        self.assertIn('conservativeFallbackUsed',source)
    def test_shop_allocator_never_claims_global_optimum(self):
        source=content('src/core/ShopToleranceAllocation.h')
        self.assertRegex(source,r'NOT a mathematical global cost optimum')
    def test_shop_allocator_checks_directional_deviations(self):
        source=content('src/core/ShopToleranceAllocation.h')
        self.assertIn('t.direction==1?',source)
        self.assertIn('shop[i].minimumDeviationScale',source)
    def test_motion_sweep_has_exact_slab_contact(self):
        source=content('src/core/MotionSweptClearance.h')
        self.assertIn('staged.everAabbContact=begin<=end',source)
        self.assertIn('const double speed=vb[axis]-va[axis]',source)
    def test_motion_sweep_has_continuous_gap_optimization(self):
        source=content('src/core/MotionSweptClearance.h')
        self.assertIn('std::hypot(gaps[0],gaps[1],gaps[2])',source)
        self.assertIn('iteration<100',source)
    def test_motion_sweep_transactional_state(self):
        source=content('src/core/MotionSweptClearance.h')
        self.assertLess(source.index('auto staged=pairs_'),source.index('pairs_=std::move(staged)'))
    def test_motion_sweep_rejects_changed_ids(self):
        source=content('src/core/MotionSweptClearance.h')
        self.assertIn('ids!=ids_',source)
        self.assertIn('!std::isfinite(time-previousTime_)',source)
    def test_tiny_witness_tolerance_uses_hypot(self):
        source=content('src/core/BranchReferenceAgreement.h')
        self.assertIn('std::hypot(dx,dy,dz)',source)
        self.assertNotIn('toleranceMm*toleranceMm',source)
    def test_worst_case_target_center_uses_stable_midpoint(self):
        self.assertIn('std::midpoint(requiredMinimumMm,allowedMaximumMm)',
                      content('src/core/AssemblyToleranceBudget.h'))
    def test_msvc_bigobj_preserved(self):
        self.assertIn('/bigobj',content('CMakeLists.txt'))
    def test_grouped_edge_fix_preserved(self):
        self.assertEqual(content('src/core/ShapeFeatureTree.cpp').count('anchor.remove("size");'),2)
    def test_cmake_references_new_headers(self):
        cmake=content('CMakeLists.txt')
        self.assertIn('src/core/MotionSweptClearance.h',cmake)
        self.assertIn('src/core/ShopToleranceAllocation.h',cmake)
    def test_windows_native_test_count_preserved(self):
        names=re.findall(r'add_test\s*\(\s*NAME\s+([A-Za-z0-9_]+)',content('CMakeLists.txt'))
        self.assertEqual(len(names),130)
        self.assertEqual(len(names),len(set(names)))
    def test_complete_portable_test_registration(self):
        source=content('tools/run_portable_regressions.py')
        for name in ('geometry_numeric_stability_portable_tests.cpp',
                     'shop_tolerance_allocation_portable_tests.cpp',
                     'motion_swept_clearance_portable_tests.cpp'):
            self.assertIn(name,source)
    def test_static_audit_detects_bad_syntax_and_broken_source(self):
        spec=importlib.util.spec_from_file_location('source_audit', ROOT/'tools/audit_full_source.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        result=module.inspect(ROOT)
        self.assertEqual(result['status'],'PASS_STATIC_SOURCE_INTEGRITY',result['problems'])
        self.assertGreater(result['lines_scanned'],60000)
        self.assertGreaterEqual(result['portable_regressions'],35)
    def test_static_audit_rejects_missing_tree(self):
        spec=importlib.util.spec_from_file_location('source_audit', ROOT/'tools/audit_full_source.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as folder:
            report=module.inspect(Path(folder))
        self.assertEqual(report['status'],'FAIL_STATIC_SOURCE_INTEGRITY')
        self.assertIn('Missing source directory: src',report['problems'])
    def test_motion_test_is_not_fake_ctest(self):
        source=content('tests/motion_swept_clearance_portable_tests.cpp')
        self.assertIn('report.minimumGapMm<=witness+1e-8',source)
        self.assertIn('contact interval witness',source)
    def test_shop_test_has_reproducible_random_seed(self):
        source=content('tests/shop_tolerance_allocation_portable_tests.cpp')
        self.assertIn('std::mt19937 engine(41789)',source)
        self.assertIn('iteration<1600',source)

if __name__=='__main__':unittest.main()

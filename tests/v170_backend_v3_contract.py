"""Backend-only integration guards for the CADWorkbench v1.70 core increment.

These tests verify that the new numerical planners remain accessible through
existing project APIs and that the backend-only release does not alter Qt UI.
Real geometry and motion regressions remain in the compiled C++ suites.
"""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]

def read(path):
    return (ROOT/path).read_text(encoding='utf-8')

class BackendOnlyContracts(unittest.TestCase):
    def test_document_graph_uses_shared_scheduler(self):
        code=read('src/core/LinkedBodyGraph.cpp')
        self.assertIn('std::priority_queue<std::size_t',code)
        self.assertIn('bool planAffectedWaves(',code)
        self.assertIn('affectedMany(nodes,changedIds,&ordered,error)',code)
        self.assertNotIn('std::vector<bool> processed(nodes.size()',code)

    def test_rebuild_waves_keep_transactional_contract(self):
        header=read('src/core/LinkedBodyGraph.h')
        self.assertIn('struct RebuildWave',header)
        self.assertIn('bool planAffectedWaves(',header)
        self.assertIn('bool affectedMany(',header)
        tests=read('tests/backend_rebuild_waves_portable_tests.cpp')
        for scenario in ('ancestor and descendant edited','parallel independent branches',
                         'failed plan cannot overwrite output','long chain depths'):
            self.assertIn(scenario,tests)

    def test_mobility_analyzer_is_read_only_and_bounded(self):
        code=read('src/core/AssemblyConstraintMobility.h')
        for required in ('analyzeConstraintMobility','remainingDegreesOfFreedom',
            'whollyRedundantConnections','disconnectedOccurrences','nodeCount>32',
            'if(rank==previousRank)','*output=std::move(next)'):
            self.assertIn(required,code)
        self.assertNotIn('SceneDocument',code)

    def test_quintic_profile_preserves_legacy_motion_modes(self):
        code=read('src/core/JointMotionInterpolation.h')
        self.assertIn('enum class Progression { Linear, Smoothstep, QuinticSmoothstep }',code)
        self.assertIn('fraction*fraction*(3.0-2.0*fraction)',code)
        self.assertIn('10.0+fraction*(-15.0+6.0*fraction)',code)

    def test_quintic_duration_has_independent_envelope_gate(self):
        code=read('src/core/MotionDriveLimitReview.h')
        self.assertIn('planQuinticMotion',code)
        self.assertIn('reviewDriveLimits(drives,duration,Progression::QuinticSmoothstep',code)
        self.assertIn('10.0/std::sqrt(3.0)',code)
        self.assertIn('nextafter(duration',code)

    def test_native_assembly_tests_include_mobility(self):
        test=read('tests/assembly_constraint_network_v170_tests.cpp')
        self.assertIn('core/AssemblyConstraintMobility.h',test)
        self.assertIn('remainingDegreesOfFreedom==3',test)
        self.assertIn('remainingDegreesOfFreedom==0',test)

    def test_native_articulated_tests_include_bounded_motion(self):
        test=read('tests/articulated_assembly_v170_tests.cpp')
        self.assertIn('core/MotionDriveLimitReview.h',test)
        self.assertIn('planQuinticMotion(',test)

    def test_native_linked_graph_tests_include_new_schedule(self):
        test=read('tests/linked_body_graph_v170_tests.cpp')
        self.assertIn('planAffectedWaves(',test)
        self.assertIn('descendant cannot',read('tests/backend_rebuild_waves_portable_tests.cpp'))

    def test_portable_runner_exercises_new_backend(self):
        code=read('tools/run_portable_regressions.py')
        for name in ('backend_mobility_portable_tests.cpp',
                     'backend_quintic_motion_portable_tests.cpp',
                     'backend_rebuild_waves_portable_tests.cpp'):
            self.assertIn(name,code)

    def test_no_native_registration_changes(self):
        cmake=read('CMakeLists.txt')
        matches=re.findall(r'^\s*add_test\(\s*NAME\s+(\S+)',cmake,re.MULTILINE)
        self.assertEqual((len(matches),len(set(matches))),(130,130))

if __name__=='__main__':unittest.main()

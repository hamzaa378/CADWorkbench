"""Source-level wiring checks, NOT a substitute for MSVC/Qt/OCCT CTests."""
import pathlib
import re
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
SCENE=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
NATIVE=(ROOT/'tests/articulated_assembly_scene_v170_tests.cpp').read_text(encoding='utf-8')

class MotionWorkflowContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.body=SCENE.split('bool SceneDocument::buildLiveArticulatedEnvelopeCsv(',1)[1].split('bool SceneDocument::updateLiveArticulatedCoordinates(',1)[0]
    def test_sweep_uses_existing_compiled_cxx_service(self):
        self.assertIn('#include "MotionSweptClearance.h"',SCENE)
        self.assertIn('proximity::OrderInvariantSweptTracker betweenPoses(0.0)',self.body)
        self.assertIn('class OrderInvariantSweptTracker',(ROOT/'src/core/MotionSweptClearance.h').read_text(encoding='utf-8'))
        self.assertIn('betweenPoses.observe(fraction,simultaneous',self.body)
    def test_fixed_reference_is_in_same_time_and_swept_pairs(self):
        self.assertIn('tracked.append(liveArticulatedFixed_)',self.body)
        self.assertIn('for(const QString& id:tracked)',self.body)
        self.assertIn('for(const QString& other:tracked)',self.body)
        self.assertIn('sameTime.observe(simultaneous',self.body)
    def test_rotating_assemblies_cannot_be_mislabeled_continuous_translation(self):
        self.assertIn('joint.kind!=articulated::JointKind::Revolute',self.body)
        self.assertIn('bool sweepAvailable=translationOnly',self.body)
        self.assertIn('UNSUPPORTED_ROTATION',self.body)
    def test_sampled_success_is_independent_of_sweep_support(self):
        self.assertIn('if(sweepAvailable)',self.body)
        self.assertIn('sweepAvailable=false',self.body)
        self.assertIn('UNAVAILABLE_NUMERICS',self.body)
    def test_export_retains_existing_columns(self):
        header=re.search(r'QByteArray generated\("(body_id,body_name,samples[^"\n]*)"\);',self.body)
        self.assertIsNotNone(header)
        for field in ('possible_workspace_overlap_count','nearest_other_body_id',
                      'same_time_aabb_overlap_samples','closest_same_time_sample',
                      'continuous_translation_sweep_status','swept_aabb_contact_segments'):
            self.assertIn(field,header.group(1))
    def test_sweep_does_not_claim_exact_brep_collision(self):
        self.assertIn('neither box overlap proves solid collision',self.body)
        self.assertIn('inter-pose sweep unavailable',self.body)
    def test_csv_and_geometry_remain_transactional(self):
        self.assertIn('*csv=std::move(generated)',self.body)
        self.assertLess(self.body.index('stageLiveArticulatedRegeneration('),self.body.index('*csv=std::move(generated)'))
        self.assertNotIn('checkpoint();',self.body)
    def test_fixture_scene_and_rotating_scene_native_regressions(self):
        self.assertIn('sliderWorkspace.contains("SUPPORTED_TRANSLATION")',NATIVE)
        self.assertIn('sliderWorkspace.contains(sliderFixed.toUtf8())',NATIVE)
        self.assertIn('workspace.contains("UNSUPPORTED_ROTATION")',NATIVE)
        self.assertIn('sliderWorkspace==sliderWorkspaceSnapshot',NATIVE)
    def test_atomic_checked_commit_enforces_translating_swept_clearance(self):
        segment=SCENE.split('bool SceneDocument::commitLiveArticulatedCoordinatesChecked(',1)[1].split('namespace {',1)[0]
        for term in ('if(rejectUnsafe&&std::all_of(', 'buildLiveArticulatedEnvelopeCsv(targets,',
                     'sweptMinimum<=requiredClearanceMm', 'return updateLiveArticulatedCoordinates(targets,error)'):
            self.assertIn(term,segment)
        self.assertLess(segment.index('sweptMinimum<=requiredClearanceMm'),
                        segment.index('return updateLiveArticulatedCoordinates(targets,error)'))
        self.assertIn('slider.commitLiveArticulatedCoordinatesChecked({{0,20.0}},0.0,',NATIVE)
    def test_sweep_structured_results_are_committed_only_after_success(self):
        header=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')
        self.assertIn('bool* translationSweepSupported=nullptr',header)
        self.assertIn('double* closestSweptBoxGapMm=nullptr',header)
        self.assertLess(self.body.index('*csv=std::move(generated)'),
                        self.body.index('if(translationSweepSupported)*translationSweepSupported=sweepAvailable'))
    def test_numeric_gap_uses_overflow_resistant_hypot(self):
        self.assertIn('return std::hypot(dx,dy,dz)',self.body)
    def test_no_silent_native_suite_removal(self):
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        names=re.findall(r'add_test\s*\(\s*NAME\s+([A-Za-z0-9_]+)',cmake)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)

if __name__=='__main__':unittest.main()

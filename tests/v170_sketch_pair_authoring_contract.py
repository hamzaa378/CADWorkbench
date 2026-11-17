"""Source-level integration guards for v1.70 interactive pair constraints.
These tests do NOT replace compilation or native Qt/OCCT regressions.
"""
import pathlib
import re
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
HEADER=(ROOT/'src/ui/SketchCanvasDialog.h').read_text(encoding='utf-8')
SOURCE=(ROOT/'src/ui/SketchCanvasDialog.cpp').read_text(encoding='utf-8')
NATIVE=(ROOT/'tests/sketch_constraint_editor_cases.h').read_text(encoding='utf-8')
RUNNER=(ROOT/'tests/sketch_solver_tests.cpp').read_text(encoding='utf-8')
CMAKE=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')

class TestPairAuthoring(unittest.TestCase):
    def test_control_types_reachable(self):
        for item in ('EqualLength','Angle','EqualRadius','Concentric'):
            self.assertIn('Relation::'+item,SOURCE)
            self.assertRegex(HEADER,r'\b'+item+r'\b')
    def test_pair_authoring_uses_solver_transaction(self):
        for name in ('constrainSegmentPair','constrainCirclePair'):
            self.assertIn('bool SketchCanvasWidget::'+name,SOURCE)
            self.assertIn('bool '+name,HEADER)
        self.assertGreaterEqual(SOURCE.count('return tryRelation(std::move(trial),"Two-'),2)
        self.assertIn('checkpoint();model_=std::move(trial);',SOURCE)
    def test_curved_edge_rejected(self):
        self.assertIn('if(curved(*first)||curved(*second))',SOURCE)
    def test_signed_angle_with_units(self):
        self.assertIn('angleDegrees*std::acos(-1.0)/180.0',SOURCE)
        self.assertIn('Signed angle from first to second segment (degrees):',SOURCE)
        self.assertIn('std::abs(angleDegrees)>180.0',SOURCE)
    def test_circle_radial_hit_test(self):
        self.assertIn('int SketchCanvasWidget::hitCircle',SOURCE)
        self.assertIn('circle.radius*pixelsPerMm_',SOURCE)
    def test_circle_dimensions_preserved(self):
        self.assertIn('c.type==T::Radius&&c.circleA==secondId',SOURCE)
        self.assertIn('c.type==T::FixedPoint&&c.pointA==second->centerPointId',SOURCE)
    def test_native_rollback_undo_tests_registered(self):
        self.assertIn('void interactivePairRelationsRoundtrip()',NATIVE)
        self.assertIn('sketch_editor_test::interactivePairRelationsRoundtrip()',RUNNER)
        for token in ('view.undoEdit();','Conflicting second edge length','Invalid two-circle relation'):
            self.assertIn(token,NATIVE)
    def test_reloading_sketch_clears_in_progress_pair_pick(self):
        self.assertIn('tool_=closed_?Tool::Select:Tool::Polyline;relation_=Relation::None;',SOURCE)
        self.assertGreaterEqual(SOURCE.count('firstRelationCircle_.clear();'),4)
    def test_preserve_existing_ctest_inventory(self):
        self.assertIn('add_test(NAME cadcore_sketch_solver COMMAND sketch_solver_tests)',CMAKE)
        self.assertEqual(len(re.findall(r'\badd_test\s*\(',CMAKE)),130)

if __name__=='__main__':unittest.main()

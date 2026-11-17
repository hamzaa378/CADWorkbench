"""Regression contracts for Sprint 3 grouped-edge defect.

These are static checks, never a substitute for Windows CTest #45.
"""
import re
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
TREE=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text(encoding='utf-8')
NATIVE=(ROOT/'tests/shape_feature_tree_tests.cpp').read_text(encoding='utf-8')

class GroupedEdgeRegression(unittest.TestCase):
    def test_two_distinct_resolver_branches(self):
        self.assertEqual(TREE.count('anchor.remove("size");'),2)
    def test_both_edge_groups_still_supported(self):
        self.assertIn('op.type=="Fillet selected edges"',TREE)
        self.assertIn('op.type=="Chamfer selected edges"',TREE)
    def test_operation_uses_per_edge_size_not_group_default(self):
        self.assertEqual(TREE.count('fillet->Add(memberSize,'),1)
        self.assertEqual(TREE.count('chamfer->Add(memberSize,'),1)
    def test_size_separated_before_strict_anchor_resolve(self):
        self.assertEqual(len(re.findall(r'anchor.remove\("size"\);\s*if\(!resolveStableEdgeAnchor',TREE)),2)
    def test_strict_anchor_contract_still_present(self):
        self.assertIn('anchor.size()!=(hasBody?7:4)',TREE)
        self.assertIn('fail(error,"Incomplete edge anchor.")',TREE)
    def test_no_silent_missing_anchor_fallback(self):
        self.assertEqual(TREE.count('if(!resolveStableEdgeAnchor(input,anchor,&selected,error))return false;'),4)
    def test_duplicate_group_rejected(self):
        self.assertIn('!selectedIndices.insert(selected).second',TREE)
    def test_duplicate_witness_scenario_retained(self):
        self.assertIn('Duplicate physical edges were accepted as a group',NATIVE)
    def test_bad_variable_size_scenario_retained(self):
        self.assertIn('Invalid variable edge size changed an existing solid',NATIVE)
    def test_failed_operation_rollback_retained(self):
        self.assertIn('Failed group operation modified the document',NATIVE)
    def test_group_persistence_retained(self):
        self.assertIn('Grouped blend changed volume across save/reload',NATIVE)
    def test_group_failure_diagnoses_operation_and_message(self):
        self.assertIn('operation.toStdString()',NATIVE)
        self.assertIn('error.toStdString()',NATIVE)

if __name__=='__main__':unittest.main()

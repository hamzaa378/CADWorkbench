"""Source-level integration contracts; NOT native Windows compilation."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def read(path):
    return (ROOT/path).read_text(encoding='utf-8')

class NextThreeAreasContracts(unittest.TestCase):
    def test_per_pattern_stable_references_have_a_v5_schema(self):
        engine=read('src/core/SceneDocument.cpp')
        header=read('src/core/SceneDocument.h')
        for key in ('referencePatternOccurrences', 'dag-interleaved-history-v5',
                    'reference_occurrences', 'earlierPatternIds.contains(it.key())',
                    'selectedPatternStages.contains(it.key())',
                    'step.referencePatternOccurrences.value(descendant,0)',
                    'withOccurrences&&(!nestedPatternHistory'):
            self.assertIn(key,engine if key!='referencePatternOccurrences' else header)
        self.assertIn('Legacy v4',header)
        self.assertIn('referencePatternOccurrence!=0&&patternHops!=1',engine)

    def test_consumed_subshapes_do_not_silently_become_new_references(self):
        reference=read('src/core/BooleanReferenceLineage.cpp')
        for key in ('rebaseFeatureThroughSurvivingModifier(',
                    'newEdges.FindKey(i).IsSame(oldEdges.FindKey(original))',
                    'Selected edge was consumed or replaced',
                    'Referenced face was modified by fillet/chamfer',
                    'validateShapeFeatureNodes({changed},error)'):
            self.assertIn(key,reference)
        engine=read('src/core/SceneDocument.cpp')
        self.assertIn('addParents(survivingModifierSources)',engine)
        self.assertIn('supportsSurvivingModifierLineage(step.feature)',engine)
        self.assertIn('rebaseFeatureThroughSurvivingModifier(evaluated.value(ancestor)',engine)

    def test_mixed_ui_edits_loft_and_sweep_without_pretending_topology_survives(self):
        ui=read('src/ui/MainWindow.cpp')
        for key in ('"Sweep circle path","Loft rectangles"',
                    'next.feature.parameters={{"width0",loftWidth0->value()}',
                    'next.feature.parameters={{"radius",sweepRadius->value()}',
                    'A direct-stage witness does not establish ancestry through another operation',
                    'auto* scroll=new QScrollArea(&editor)',
                    'auto* occurrenceTable=new QTableWidget(&editor)',
                    'occurrenceSpins.insert(steps[i].id,instance)'):
            self.assertIn(key,ui)
        self.assertIn('Loft rectangles',read('src/core/ShapeFeatureTree.cpp'))
        self.assertIn('Sweep circle path',read('src/core/ShapeFeatureTree.cpp'))

    def test_native_windows_integration_scenarios_written_and_gate_preserved(self):
        tests=read('tests/interleaved_history_v170_tests.cpp')
        for key in ('nested pattern nonseed occurrence rebuilt',
                    'save v5 nested selections','unknown v5 pattern key rejected',
                    'unaffected edge survives a prior selective fillet',
                    'already consumed source edge cannot be retargeted',
                    'loft and sweep regenerate in linked mixed history'):
            self.assertIn(key,tests)
        cmake=read('CMakeLists.txt')
        import re
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cmake)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)

if __name__=='__main__':unittest.main()

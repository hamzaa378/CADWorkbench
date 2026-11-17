"""Source integration gate for dual-source in-place modifiers and mate UI.
Windows C++ compilation remains the final acceptance gate.
"""
import re
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def read(path): return (ROOT/path).read_text(encoding='utf-8')
class V170TwoStagePreflight(unittest.TestCase):
    def test_two_stage_source_and_persistence(self):
        core=read('src/core/SceneDocument.cpp')
        for key in ('bool SceneDocument::appendInPlaceSketchFeature(',
                    'bool SceneDocument::updateInPlaceSketchFollowUp(',
                    'bool SceneDocument::removeInPlaceSketchFollowUp(',
                    'part.hasInPlaceSketchFollowUp=true;',
                    'linked.inPlaceFollowSourceToken=followSource->geometryToken;',
                    'link.insert("follow_up",second);',
                    'if(link.contains("follow_up"))',
                    'Saved second sketch source is stale.',
                    'expected=finalShape;',
                    'part.hasInPlaceSketchFollowUp&&requested.contains(part.inPlaceFollowSourceId)'):
            self.assertIn(key,core)
    def test_dependency_uses_both_sources_in_all_three_places(self):
        core=read('src/core/SceneDocument.cpp')
        # The three independently copied graphs have been consolidated into
        # one source of truth, called by regeneration, previews and batch edit.
        self.assertEqual(core.count('node.tool=part.inPlaceFollowSourceId.toStdString();'),1)
        self.assertEqual(core.count('(part.hasInPlaceSketchFeature&&!part.hasInPlaceSketchFollowUp)'),1)
        for call in ('linkedBodyGraphNodes(parts)', 'linkedBodyGraphNodes(parts_)',
                     'linkedBodyGraphNodes(staged)'):
            self.assertIn(call,core)
    def test_header_declares_two_stage_data_and_methods(self):
        header=read('src/core/SceneDocument.h')
        for key in ('hasInPlaceSketchFollowUp','inPlaceFollowSourceId',
                    'appendInPlaceSketchFeature','updateInPlaceSketchFollowUp',
                    'removeInPlaceSketchFollowUp'):
            self.assertIn(key,header)
    def test_ui_allows_stage_edit_and_three_mate_formats(self):
        ui=read('src/ui/MainWindow.cpp')
        for key in ('Append second modifier','Edit second modifier',
                    'document_.appendInPlaceSketchFeature(',
                    'document_.updateInPlaceSketchFollowUp(',
                    'document_.removeInPlaceSketchFollowUp(',
                    'One point and two oriented axes',
                    'One point, one axis and one plane',
                    'cad::mates::Kind::CoincidentPlane'):
            self.assertIn(key,ui)
    def test_native_ctest_count_and_regression_modes(self):
        cm=read('CMakeLists.txt')
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        for name in ('cadcore_v170_inplace_two_stage',
                     'cadcore_v170_inplace_two_stage_persistence',
                     'cadcore_v170_inplace_two_stage_corruption'):
            self.assertIn(name,names)
        self.assertIn('ExpectedTests = 130',read('scripts/verify_release_windows.ps1'))
        reg=read('tests/inplace_sketch_v170_tests.cpp')
        for key in ('two-stage-persistence','two-stage-corruption',
                    'updateInPlaceSketchFollowUp','linkedBodyImpact(second',
                    'saved second source did not propagate'):
            self.assertIn(key,reg)
    def test_no_original_interface_removed(self):
        core=read('src/core/SceneDocument.cpp')
        for key in ('setInPlaceSketchFeature','removeInPlaceSketchFeature',
                    'linkedBooleanResult','solveAssemblyMates','updateLinkedBooleanBatch'):
            self.assertIn(key,core)
if __name__=='__main__': unittest.main()

"""Source contracts for the six Windows-native failures reported on 2026-11-06.

These checks are NOT substitutes for compiling the Qt/OpenCascade code with MSVC.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

def source(relative):
    return (ROOT / relative).read_text(encoding='utf-8')

class NativeRoundThreeContracts(unittest.TestCase):
    def test_inplace_and_shape_tree_can_coexist(self):
        code=source('src/core/SceneDocument.cpp')
        begin=code.index('if(item.contains("in_place_sketch_feature")){')
        end=code.index('const QJsonObject link=item.value("in_place_sketch_feature").toObject()',begin)
        self.assertNotIn('item.contains("shape_feature_tree")',code[begin:end])
        self.assertIn('child.hasShapeFeatureTree?child.shapeFeatureBase:child.shape',code)

    def test_inplace_save_has_integrity_checksum(self):
        code=source('src/core/SceneDocument.cpp')
        self.assertIn('QCryptographicHash::hash(linkedBytes,QCryptographicHash::Sha256)',code)
        self.assertIn('link.insert("integrity_sha256"',code)

    def test_inplace_load_checks_checksum_before_replay(self):
        code=source('src/core/SceneDocument.cpp')
        load=code.index('if(item.contains("in_place_sketch_feature")){')
        verify=code.index('Saved in-place feature parameters have changed since export.',load)
        replay=code.index('derived.inPlaceSketchSourceId=link.value("source")',load)
        self.assertLess(verify,replay)
        self.assertIn('payload.remove("integrity_sha256")',code[load:replay])
        self.assertIn('int(link.contains("integrity_sha256"))',code[load:replay])

    def test_freeform_does_not_follow_unverified_translation(self):
        code=source('src/core/ShapeFeatureTree.cpp')
        self.assertIn('const int trials=(type==5.0||type==6.0)?1:2;',code)

    def test_closed_loop_fixture_has_valid_and_invalid_cases(self):
        test=source('tests/assembly_mate_document_v170_tests.cpp')
        self.assertIn('makeTranslationMate(bodies[1],bodies[2],35,35)',test)
        self.assertIn('makeTranslationMate(bodies[1],bodies[2],19,35)',test)
        self.assertIn('rejected inconsistent loop must not mutate the assembly',test)

    def test_branched_reference_diamond_actually_disagrees(self):
        test=source('tests/interleaved_history_v170_tests.cpp')
        self.assertIn('conflicting[1].feature.type="Translate"',test)
        self.assertIn('"x",3.0',test)
        self.assertIn('disagreeing Boolean ancestry routes must reject',test)

    def test_six_ctest_names_are_in_focus_launcher(self):
        launcher=source('RUN_PREVIOUS_FAILURES.ps1')
        for value in ('interleaved_history','assembly_mate_document','mixed_feature_history',
                      'curved_face_lineage','inplace_sketch_persistence',
                      'inplace_two_stage_corruption'):
            self.assertIn(value,launcher)

if __name__=='__main__':
    unittest.main()

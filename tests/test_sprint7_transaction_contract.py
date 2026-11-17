"""Static integration contracts for native CAD transaction changes.

These tests catch accidental bypasses; Windows Qt/OCCT tests are separate.
"""
import pathlib
import re
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
SCENE=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
HEADER=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')
CMAKE=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
TOOLS=(ROOT/'tools/audit_full_source.py').read_text(encoding='utf-8')


def body(name):
    marker=f'bool SceneDocument::{name}('
    begin=SCENE.index(marker)
    match=re.search(r'\nbool SceneDocument::|\nnamespace \{',SCENE[begin+len(marker):])
    return SCENE[begin:begin+len(marker)+match.start()] if match else SCENE[begin:]

class Sprint7TransactionContracts(unittest.TestCase):
    def test_single_transaction_helper_declared(self):
        self.assertIn('bool commitMachiningShape(',HEADER)

    def test_machining_result_is_geometry_checked(self):
        self.assertIn('if(!validShape(result,error))return false;',body('commitMachiningShape'))

    def test_machining_result_must_remove_material(self):
        self.assertIn('sourceVolume-resultVolume<=std::max(',body('commitMachiningShape'))

    def test_machining_rejects_direct_edits_of_linked_parts(self):
        code=body('commitMachiningShape')
        for phrase in ('hasLinkedBoolean','hasLinkedSketchExtrusion',
                       'hasInPlaceSketchFeature','linkedFoldedFlatSourceId'):
            self.assertIn(phrase,code)

    def test_machining_commit_checks_stale_preview(self):
        self.assertIn('original->geometryToken!=expectedRevision',body('commitMachiningShape'))

    def test_machining_propagates_before_checkpoint(self):
        code=body('commitMachiningShape')
        self.assertLess(code.index('propagateLinkedBodies(&staged,error)'),code.index('checkpoint()'))
        self.assertLess(code.index('checkpoint()'),code.index('parts_=std::move(staged)'))

    def test_five_direct_machining_commits_share_gate(self):
        for name in ('commitDovetailGroove','commitTSlotGroove','commitRoundedSlot',
                     'commitKeyholeSlot','commitRoundedSlotPattern'):
            self.assertIn('return commitMachiningShape(command.partId,command.geometryToken,preview,error);',body(name))

    def test_seven_patterned_machining_commits_share_gate(self):
        for name in ('holePattern','circularHolePattern','circularCountersinkPattern',
                     'circularCounterborePattern','linearCounterborePattern',
                     'linearCountersinkPattern','linearKeyholeSlotPattern'):
            code=body(name)
            self.assertRegex(code,r'return commitMachiningShape\(id,(token|geometryToken|sourceToken),working,error\);')

    def test_preview_still_uses_detached_document(self):
        code=body('previewShapeFeatureTreesBatch')
        self.assertIn('SceneDocument staged;',code)
        self.assertIn('stageShapeFeatureTreesBatch(',code)
        self.assertNotIn('checkpoint()',code)

    def test_preview_reports_newly_stale_drawing_sheets(self):
        code=body('previewShapeFeatureTreesBatch')
        self.assertIn('newly_stale_sheets=',code)
        self.assertIn('newlyStaleSheets',code)
        self.assertIn('before.state==DrawingSourceState::Current',code)

    def test_preview_reports_newly_stale_annotations(self):
        code=body('previewShapeFeatureTreesBatch')
        self.assertIn('newly_stale_annotations=',code)
        self.assertIn('before.staleAnnotationIds.contains(annotation)',code)

    def test_native_machining_scenarios_embedded_in_registered_modes(self):
        cpp=(ROOT/'tests/cadcore_tests.cpp').read_text()
        self.assertIn('machining_transaction_v170_tests.h',cpp)
        for name in ('commitMachiningVariants()','rejectionPreservesScene()',
                     'patternsUpdateDescendants()'):
            self.assertIn('cad::sprint7_test::'+name,cpp)

    def test_baseline_native_names_preserved(self):
        previous=(ROOT/'tests/BASELINE_130_NATIVE_CTEST_NAMES.txt').read_text().splitlines()
        current=re.findall(r'\badd_test\s*\(\s*NAME\s+([A-Za-z0-9_]+)',CMAKE)
        self.assertEqual(len(previous),130)
        self.assertEqual(len(current),130)
        self.assertTrue(set(previous).issubset(current))
        self.assertEqual(len(current),len(set(current)))

    def test_source_audit_checks_previous_and_added_tests(self):
        self.assertIn('BASELINE_130_NATIVE_CTEST_NAMES.txt',TOOLS)
        self.assertIn('len(ctest_names)!=130',TOOLS)

    def test_no_direct_checkpoint_in_five_machining_commits(self):
        for name in ('commitDovetailGroove','commitTSlotGroove','commitRoundedSlot',
                     'commitKeyholeSlot','commitRoundedSlotPattern'):
            self.assertNotIn('checkpoint()',body(name))

    def test_no_direct_checkpoint_in_seven_machining_patterns(self):
        for name in ('holePattern','circularHolePattern','circularCountersinkPattern',
                     'circularCounterborePattern','linearCounterborePattern',
                     'linearCountersinkPattern','linearKeyholeSlotPattern'):
            self.assertNotIn('checkpoint()',body(name))

if __name__=='__main__':unittest.main()

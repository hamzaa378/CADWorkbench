"""Source-contract checks, not evidence of a native Windows compilation."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / 'src/core/SceneDocument.h').read_text(encoding='utf-8')
CORE = (ROOT / 'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
UI = (ROOT / 'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
TESTS = (ROOT / 'tests/cadcore_tests.cpp').read_text(encoding='utf-8')
TREE = (ROOT / 'tests/shape_feature_tree_tests.cpp').read_text(encoding='utf-8')


class Rc5IntegrationContract(unittest.TestCase):
    def test_configuration_catalog_is_persisted_and_optional(self):
        for token in ('struct SavedDesignConfiguration', 'designConfigurations',
                      'applyDesignConfiguration', 'activeDesignConfiguration'):
            self.assertIn(token, HEADER)
        for token in ('item.insert("design_configurations",catalog)',
                      'item.insert("active_design_configuration"',
                      'if(item.contains("design_configurations")',
                      'validateDesignConfigurations(candidate.last(),parsed,error)'):
            self.assertIn(token, CORE)

    def test_configuration_selection_is_atomic(self):
        for token in ('selectedConfiguration',
                      'candidate[index].activeDesignConfiguration=selected',
                      'if(!propagateLinkedBodies(&candidate,error))return false',
                      'checkpoint();parts_=std::move(candidate)'):
            self.assertIn(token, CORE)
        self.assertIn('testV170Rc5NativeConfigurations()', TESTS)

    def test_configuration_editor_is_project_native(self):
        for token in ('document_.designConfigurations(id)',
                      'document_.setDesignConfigurations(id,definitions,&error)',
                      'document_.applyDesignConfiguration(id,selectedName,&error)',
                      'Save presets in project'):
            self.assertIn(token, UI)

    def test_feature_note_source_status_is_fail_closed(self):
        for token in ('struct FeatureInspectionNote', 'featureInspectionNotes',
                      'reviewedGeometryToken'):
            self.assertIn(token, HEADER)
        for token in ('MISSING_FEATURE', 'REVIEW_REQUIRED', 'CURRENT_REVISION',
                      'A new inspection annotation must reference an existing stable feature ID',
                      'New notes cannot claim review of an old geometry revision.'):
            self.assertIn(token, CORE)

    def test_feature_note_persistence_and_report(self):
        for token in ('feature_inspection_notes',
                      'validFeatureNotes(notes,error)',
                      'buildFeatureInspectionCsv', 'exportFeatureInspectionCsv'):
            self.assertIn(token, CORE)
        for token in ('editFeatureInspectionNotes()', 'exportFeatureInspectionNotes()',
                      'Review selected for current geometry',
                      'document_.setFeatureInspectionNotes(id,notes,&error)'):
            self.assertIn(token, UI)
        self.assertIn('RC5 feature annotation did not survive project save/reopen', TREE)

    def test_document_audit_blocks_stale_inspection_signoff(self):
        audit=(ROOT/'src/core/DocumentAudit.cpp').read_text(encoding='utf-8')
        tests=(ROOT/'tests/document_audit_tests.cpp').read_text(encoding='utf-8')
        for token in ('FEATURE_NOTE_REVIEW_REQUIRED','FEATURE_NOTE_SOURCE_MISSING',
                      'document.inspectionFeatureIds(part.id)',
                      'AuditSeverity::Error,"Feature inspection"'):
            self.assertIn(token,audit)
        self.assertIn('RC5 stale inspection failed to block document preflight',tests)

    def test_native_release_registration_preserved(self):
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('NAME cadcore_universal_feature_tree COMMAND shape_feature_tree_tests',cmake)
        self.assertIn('NAME cadcore_parametric_foundation COMMAND cadcore_tests --parametric-foundation',cmake)
        self.assertEqual(cmake.count('add_test('),130)


if __name__=='__main__':
    unittest.main()

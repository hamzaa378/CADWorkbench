"""Source integration checks; native MSVC/OCCT acceptance is separate."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
def content(path):return (ROOT/path).read_text(encoding='utf-8')

class LinkedBodyDagPreflight(unittest.TestCase):
    def test_no_missing_cmake_sources_or_duplicate_tests(self):
        c=content('CMakeLists.txt')
        names=re.findall(r'add_test\(NAME\s+(\S+)',c)
        self.assertEqual(len(names),130)
        self.assertEqual(len(names),len(set(names)))
        for name in ('cadcore_v170_linked_body_graph_planner',
                     'cadcore_v170_linked_body_edit','cadcore_v170_linked_body_persistence'):
            self.assertIn(name,names)
        for path in ('src/core/LinkedBodyGraph.h','src/core/LinkedBodyGraph.cpp',
                     'tests/linked_body_graph_v170_tests.cpp',
                     'tests/linked_body_edit_v170_tests.cpp'):
            self.assertTrue((ROOT/path).is_file(),path)
        self.assertIn('[int]$ExpectedTests = 130',content('scripts/verify_release_windows.ps1'))
    def test_portable_graph_checks_strict_and_nondestructive(self):
        graph=content('src/core/LinkedBodyGraph.cpp')
        self.assertIn('bool plan(',graph)
        self.assertIn('bool affected(',graph)
        self.assertIn('Linked-body dependency graph contains a cycle',graph)
        self.assertIn('*order=std::move(result);',graph)
        self.assertIn('*indices=std::move(result);',graph)
    def test_document_uses_topological_rebuilds(self):
        scene=content('src/core/SceneDocument.cpp')
        self.assertIn('for(int i:order){',scene)
        self.assertIn('if(!planLinkedBodyOrder(*staged,&order,error))return false',scene)
        self.assertIn('bool SceneDocument::updateLinkedBooleanBody(',scene)
        self.assertIn('bool SceneDocument::updateLinkedBooleanBatch(',scene)
        self.assertIn('result.linkedPrimaryToken.clear();',scene)
        self.assertIn('if(!propagateLinkedBodies(&candidate,error))return false;',scene)
        self.assertIn('checkpoint();parts_=std::move(candidate);',scene)
    def test_saved_links_can_reference_later_sources(self):
        scene=content('src/core/SceneDocument.cpp')
        self.assertIn('if(!planLinkedBodyOrder(candidate,&savedOrder,error))return false;',scene)
        self.assertNotIn('a>=i||b>=i',scene)
        self.assertIn('Saved linked Boolean geometry is inconsistent',scene)
    def test_interactive_edit_and_impact_actions(self):
        ui=content('src/ui/MainWindow.cpp')
        for phrase in ('Edit associative Boolean result...',
                       'Inspect associative body dependencies...',
                       'Preview associative rebuild impact...',
                       'updateLinkedBooleanBody(', 'linkedBodyImpact('):
            self.assertIn(phrase,ui)
    def test_native_regression_guards_are_present(self):
        native=content('tests/linked_body_edit_v170_tests.cpp')
        for phrase in ('Retarget to a later source failed','Dependency cycle accepted',
                       'Rollback modified linked result',
                       'Retargeted DAG rejected on load','Saved graph cycle accepted'):
            self.assertIn(phrase,native)

if __name__=='__main__':unittest.main(verbosity=2)

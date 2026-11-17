"""Integration checks for the atomic linked-body batch increment.

These checks do not replace native Windows CTest acceptance.
"""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
def read(name):return (ROOT/name).read_text(encoding='utf-8')

class AtomicBatchIntegration(unittest.TestCase):
    def test_cmake_complete_and_test_count(self):
        cm=read('CMakeLists.txt')
        self.assertIn('project(CADWorkbench VERSION 1.70.0 LANGUAGES CXX)',cm)
        self.assertIn('tests/linked_body_batch_v170_tests.cpp',cm)
        names=re.findall(r'add_test\(NAME\s+(\S+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        for test in ('cadcore_v170_atomic_batch_edit','cadcore_v170_atomic_batch_preview',
                     'cadcore_v170_atomic_batch_persistence'):
            self.assertIn(test,names)
        self.assertIn('[int]$ExpectedTests = 130',read('scripts/verify_release_windows.ps1'))

    def test_transactional_integrity(self):
        cpp=read('src/core/SceneDocument.cpp')
        self.assertIn('bool SceneDocument::updateLinkedBooleanBatch(',cpp)
        self.assertIn('bool SceneDocument::previewLinkedBooleanBatch(',cpp)
        batch=cpp.split('bool SceneDocument::updateLinkedBooleanBatch(',1)[1].split('bool SceneDocument::updateLinkedBooleanBody(',1)[0]
        self.assertLess(batch.index('if(!stageLinkedEdits('),batch.index('if(!propagateLinkedBodies('))
        self.assertLess(batch.index('if(!propagateLinkedBodies('),batch.index('checkpoint();'))
        self.assertLess(batch.index('checkpoint();'),batch.index('parts_=std::move(candidate);'))
        self.assertIn('if(changed.isEmpty())return true',batch)
        self.assertIn('if(!planLinkedBodyOrder(*candidate,&graphOrder,error))return false;',cpp)
        self.assertIn('if(!linkedgraph::affectedMany(',cpp)

    def test_old_single_edit_uses_atomic_engine(self):
        cpp=read('src/core/SceneDocument.cpp')
        single=cpp.split('bool SceneDocument::updateLinkedBooleanBody(',1)[1].split('bool SceneDocument::rebaseShapeFeatureTree(',1)[0]
        self.assertIn('return updateLinkedBooleanBatch(',single)

    def test_ui_connected_to_real_operations(self):
        ui=read('src/ui/MainWindow.cpp')
        header=read('src/ui/MainWindow.h')
        self.assertIn('Edit multiple associative results (atomic)...',ui)
        self.assertIn('void MainWindow::editLinkedBooleanBatch()',ui)
        self.assertIn('void editLinkedBooleanBatch();',header)
        self.assertIn('document_.previewLinkedBooleanBatch(edits()',ui)
        self.assertIn('document_.updateLinkedBooleanBatch(edits()',ui)
        self.assertIn('QTableWidget',ui)

    def test_regressions_cover_hard_cases(self):
        test=read('tests/linked_body_batch_v170_tests.cpp')
        for phrase in ('Atomic coordinated retarget failed','Single-step edit unexpectedly allowed temporary cycle',
                       'Cyclic batch was accepted','Empty Boolean was committed',
                       'No-op unnecessarily rebuilt geometry','Undo batch failed',
                       'Redo batch failed','Read-only preview mutated committed document',
                       'Loaded batch X volume wrong'):
            self.assertIn(phrase,test)

if __name__=='__main__': unittest.main(verbosity=2)

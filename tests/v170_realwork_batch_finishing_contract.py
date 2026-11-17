"""Integration checks for multi-body atomic finishing, not native substitutes."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class RealWorldBatchFinishingContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header=(ROOT/'src/core/SceneDocument.h').read_text()
        cls.core=(ROOT/'src/core/SceneDocument.cpp').read_text()
        cls.ui=(ROOT/'src/ui/MainWindow.cpp').read_text()
        cls.native=(ROOT/'tests/shape_feature_tree_tests.cpp').read_text()

    def test_atomic_public_ownership_and_document_persistence(self):
        self.assertIn('setShapeFeatureTreesBatch(const QMap<QString,QVector<ShapeFeatureNode>>& proposals',self.header)
        for marker in ('SceneDocument staged;', 'staged->parts_=parts_;',
                       'staged->setShapeFeatureTree(it.key(),it.value(),&issue)',
                       'if(!changed){if(error)error->clear();return true;}',
                       'checkpoint();', 'parts_=std::move(staged.parts_);'):
            self.assertIn(marker,self.core)
    def test_transactional_limits_and_rejected_dependency(self):
        self.assertIn('transaction::validateRebuildCommit(',self.core)
        self.assertIn('previewShapeFeatureTreesBatch(',self.core)
        for marker in ('proposals.size()<2||proposals.size()>16',
                       'Batch feature source is missing:',
                       'Batch feature edit failed on %1: %2',
                       'setShapeFeatureTree(it.key(),it.value(),&issue)'):
            self.assertIn(marker,self.core)
    def test_gui_is_an_actual_command(self):
        for marker in ('Batch finishing of selected bodies (atomic fillet/chamfer)',
                       'document_.setShapeFeatureTreesBatch(proposals,&issue)',
                       'nodes.append(finish);',
                       'Apply %1 (%2 mm) to %3 selected bodies?',
                       'QMessageBox::Yes|QMessageBox::No'):
            self.assertIn(marker,self.ui)
    def test_native_regression_records_all_or_none_undo_redo_persistence(self):
        for marker in ('!job.setShapeFeatureTreesBatch(invalid,&error)',
                       'job.setShapeFeatureTreesBatch(proposals,&error)',
                       'job.undo()', 'job.redo()', 'job.saveAs(path,&error)',
                       'reopened.load(path,&error)'):
            self.assertIn(marker,self.native)
if __name__=='__main__': unittest.main()

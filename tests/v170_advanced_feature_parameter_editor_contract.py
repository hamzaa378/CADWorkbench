import pathlib,unittest
R=pathlib.Path(__file__).resolve().parents[1]
s=(R/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
class AdvancedEditor(unittest.TestCase):
    def setUp(self):
        self.editor=s.split('void MainWindow::editInterleavedFeatureHistory(){',1)[1].split('void MainWindow::',1)[0]
        self.advanced=self.editor.split('if(existing&&!operations.contains(selectedType)){',1)[1].split('QDialog editor(&dialog)',1)[0]
    def test_advanced_feature_gui_is_editable_and_previewed(self):
        for k in ['QTableWidget(&advanced)','parameterTable->setItem(rowIndex,1,value)','proposed[index].feature.parameters=','previewInterleavedFeatureHistory(target,proposed','BRepCheck_Analyzer(finalShape).IsValid()','steps=std::move(proposed)','redraw(index)']:
            self.assertIn(k,self.advanced)
        self.assertLess(self.advanced.index('previewInterleavedFeatureHistory'),self.advanced.index('steps=std::move(proposed)'))
    def test_witnesses_and_identity_protected(self):
        for k in ['original.feature.parameters','topologyKey(key)','value->setFlags(value->flags()&~Qt::ItemIsEditable)','proposed[index].feature.parameters']:
            self.assertIn(k,self.advanced)
        for marker in ['key.contains("anchor_")','key.contains("witness_")',
                       'key.startsWith("curved_")','key.startsWith("face_")',
                       'key.startsWith("edge_")']:
            self.assertIn(marker,self.editor)
        self.assertNotIn('setInterleavedFeatureHistory',self.advanced)
if __name__=='__main__':unittest.main()

"""v1.63 integration contracts; static checks do NOT replace the MSVC/Qt/OCCT build."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
def get(name): return (ROOT/name).read_text(encoding='utf-8')

class V163InertiaPreflight(unittest.TestCase):
    def test_cmake_has_full_targets_and_correct_count(self):
        cm=get('CMakeLists.txt')
        self.assertIn('project(CADWorkbench VERSION 1.70.0 LANGUAGES CXX)',cm)
        self.assertEqual(len(re.findall(r'add_test\(NAME ',cm)),130)
        for filename in ('MassTensorAnalysis.h','MassTensorAnalysis.cpp'):
            self.assertEqual(cm.count('src/core/'+filename),2 if filename.endswith('.cpp') else 1)
        self.assertIn('add_test(NAME cadcore_v163_inertia_math',cm)
        self.assertIn('add_test(NAME cadcore_v163_inertia_scene',cm)
        self.assertIn('ExpectedTests = 130',get('scripts/verify_release_windows.ps1'))
    def test_document_methods_do_not_mutate(self):
        header=get('src/core/SceneDocument.h')
        cpp=get('src/core/SceneDocument.cpp')
        for method in ('measureMassTensor','measureAssemblyMassTensor'):
            self.assertIn('bool '+method+'(',header)
            self.assertEqual(cpp.count('bool SceneDocument::'+method+'('),1)
        start=cpp.index('bool SceneDocument::measureMassTensor(')
        stop=cpp.index('bool SceneDocument::measurePlanarSection(',start)
        region=cpp[start:stop]
        for fragment in ('BRepCheck_Analyzer','BRepGProp::VolumeProperties',
                         'volume.MatrixOfInertia()','tensor::analyze','tensor::aggregate',
                         'part->materialDensityKgPerM3'):
            self.assertIn(fragment,region)
        for unsafe in ('checkpoint()','part->shape=','parts_.append(','undoStack_'):
            self.assertNotIn(unsafe,region)
    def test_ui_connected_to_real_measurements_and_export(self):
        cpp=get('src/ui/MainWindow.cpp')
        h=get('src/ui/MainWindow.h')
        for method in ('showMassProperties','showAssemblyMassProperties'):
            self.assertIn('void '+method+'();',h)
            self.assertEqual(cpp.count('void MainWindow::'+method+'()'),1)
            self.assertIn('&MainWindow::'+method,cpp)
        self.assertIn('document_.measureMassTensor',cpp)
        self.assertIn('document_.measureAssemblyMassTensor',cpp)
        self.assertIn('Overlapping bodies count their mass twice',cpp)
        self.assertIn('QSaveFile output',cpp)
    def test_portable_module_and_native_regression(self):
        engine=get('src/core/MassTensorAnalysis.cpp')
        for fragment in ('std::isfinite','const double factor=input.densityKgPerM3*1e-9',
                         'std::sort(indices.begin()','part.massKg*((i==j?squared:0.0)',
                         'return analyze(collective,output,error)'):
            self.assertIn(fragment,engine)
        self.assertIn('void testPhysicalAssembly()',get('tests/mass_tensor_math_v163_tests.cpp'))
        native=get('tests/mass_tensor_scene_v163_tests.cpp')
        for fragment in ('setShapeFeatureTree','doc.undo()','doc.redo()','saveAs(',
                         'loaded.load(','setMaterial(', 'measureAssemblyMassTensor('):
            self.assertIn(fragment,native)

if __name__=='__main__':unittest.main(verbosity=2)

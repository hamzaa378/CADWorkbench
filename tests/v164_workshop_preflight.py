"""v1.64 static integration preflight, not a replacement for native MSVC/Qt/OCCT build."""
from pathlib import Path
import re, unittest
ROOT=Path(__file__).resolve().parents[1]
def source(name): return (ROOT/name).read_text(encoding='utf-8')
class V164WorkshopPreflight(unittest.TestCase):
    def test_version_and_native_registration(self):
        cm=source('CMakeLists.txt')
        self.assertIn('project(CADWorkbench VERSION 1.70.0 LANGUAGES CXX)',cm)
        names=re.findall(r'add_test\(NAME\s+([^\s)]+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        for name in ('cadcore_v164_section_properties','cadcore_v164_machining_estimates',
                     'cadcore_v164_tolerance_stacks','cadcore_v164_cylindrical_inventory'):
            self.assertIn(name,names)
        self.assertIn('ExpectedTests = 130',source('scripts/verify_release_windows.ps1'))
    def test_cmake_files_exist(self):
        cm=source('CMakeLists.txt')
        for name in ('WorkshopEngineering.h','WorkshopEngineering.cpp','WorkshopGeometry.h','WorkshopGeometry.cpp'):
            self.assertIn('src/core/'+name,cm)
            self.assertTrue((ROOT/'src/core'/name).is_file())
        for name in ('WorkshopPlanningDialog.h','WorkshopPlanningDialog.cpp'):
            self.assertIn('src/ui/'+name,cm)
            self.assertTrue((ROOT/'src/ui'/name).is_file())
        for name in ('workshop_geometry_v164_tests.cpp','workshop_engineering_v164_tests.cpp'):
            self.assertIn('tests/'+name,cm)
    def test_ui_wiring(self):
        h=source('src/ui/MainWindow.h');cpp=source('src/ui/MainWindow.cpp')
        self.assertIn('void showWorkshopPlanning();',h)
        self.assertEqual(cpp.count('void MainWindow::showWorkshopPlanning()'),1)
        self.assertIn('&MainWindow::showWorkshopPlanning',cpp)
        self.assertIn('#include "WorkshopPlanningDialog.h"',cpp)
        d=source('src/ui/WorkshopPlanningDialog.cpp')
        for marker in ('calculateSection(', 'estimateMachining(', 'calculateToleranceStack(',
                'inspectCylindricalSurfaces(', 'QSaveFile', 'cylinderCsv() const',
                'QMessageBox::information', 'body->materialDensityKgPerM3'):
            self.assertIn(marker,d)
    def test_no_document_mutation_or_machine_commands(self):
        d=source('src/ui/WorkshopPlanningDialog.cpp')
        self.assertNotIn('checkpoint()',d)
        self.assertNotIn('document_.',d)
        self.assertNotIn('M03',d)
        self.assertNotIn('G01',d)
        self.assertIn('NOT automatically classified as through-holes',d)
    def test_native_geometry_uses_unique_faces_and_no_silent_classification(self):
        c=source('src/core/WorkshopGeometry.cpp')
        self.assertIn('TopExp::MapShapes(shape,TopAbs_FACE,faceMap)',c)
        self.assertIn('BRepCheck_Analyzer analyzer',c)
        self.assertIn('cylinder.Radius()',c)
        self.assertIn('cylinder.Axis()',c)
        self.assertIn('Cylindrical faces include shafts and bosses',c)
    def test_engineering_methods_have_explicit_limits(self):
        c=source('src/core/WorkshopEngineering.cpp')
        for needle in ('unknown','std::isfinite','s.passes<1','milling','estimateMachining',
                       'calculateToleranceStack','RSS','calculateSection','specificCuttingEnergyJMm3'):
            self.assertIn(needle.lower(),c.lower())
        self.assertIn('NOT actual removed stock',c)
    def test_existing_inertia_path_preserved(self):
        s=source('src/core/SceneDocument.cpp')
        self.assertIn('bool SceneDocument::measureMassTensor(',s)
        self.assertIn('bool SceneDocument::measureAssemblyMassTensor(',s)
        c=source('src/ui/MainWindow.cpp')
        self.assertIn('void MainWindow::showAssemblyMassProperties()',c)
if __name__=='__main__':unittest.main()

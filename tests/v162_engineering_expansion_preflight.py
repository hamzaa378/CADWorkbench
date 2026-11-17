"""Static integration guards, not a replacement for Windows MSVC/CTest."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
def src(path):return (ROOT/path).read_text(encoding='utf-8')
class V162Preflight(unittest.TestCase):
    def test_new_standalone_sources_registered(self):
        cm=src('CMakeLists.txt')
        for name in ('ParametricStudy','GuidedEngineeringStudies'):
            for ext in ('.h','.cpp'):
                self.assertIn('src/core/'+name+ext,cm)
        for name in ('GuidedStudyDialog.h','GuidedStudyDialog.cpp'):
            self.assertIn('src/ui/'+name,cm)
        for name in ('parametric_study_v170_tests','guided_engineering_studies_v170_tests'):
            self.assertIn('add_executable('+name,cm)
        self.assertEqual(len(re.findall(r'add_test\(NAME ',cm)),130)
    def test_main_window_wiring(self):
        code=src('src/ui/MainWindow.cpp')
        header=src('src/ui/MainWindow.h')
        for operation in ('showConfigurationStudy','showEngineeringDesignReview','showGuidedEngineeringStudies'):
            self.assertIn('void '+operation+'();',header)
            self.assertEqual(code.count('void MainWindow::'+operation+'()'),1)
            self.assertIn('&MainWindow::'+operation,code)
        self.assertIn('cad::study::evaluateParameterExpressions',code)
        self.assertIn('document_.setNamedParameterFormulas(id,formulas,&error)',code)
        self.assertIn('cad::runDocumentAudit(document_)',code)
        self.assertIn('document_.billOfMaterials(&issue)',code)
    def test_windows_release_gate(self):
        gate=src('scripts/verify_release_windows.ps1')
        for marker in ('ExpectedTests = 130','build_windows.ps1','--show-only=json-v1',
                       '--output-on-failure','--output-junit','Get-FileHash'):
            self.assertIn(marker,gate)
    def test_config_validation_has_limits(self):
        code=src('src/ui/MainWindow.cpp')
        for x in ('Maximum 32 configurations','file.size()>65536','item.expressions.contains(name)',
                  'links.contains(name)','document_.setNamedParameters(id,changes,&error)'):
            self.assertIn(x,code)
    def test_baseline_engineering_unchanged(self):
        self.assertEqual(src('src/core/EngineeringDesignTools.cpp').count('        add("'),50)
        self.assertIn('void MainWindow::showEngineeringWorkbench()',src('src/ui/MainWindow.cpp'))
if __name__=='__main__':unittest.main(verbosity=2)

"""Static wiring checks for the preliminary engineering workbench, not native Qt compilation."""
from pathlib import Path
import re
import unittest
from pygments import lex
from pygments.lexers import CppLexer
from pygments.token import Punctuation, Error
ROOT=Path(__file__).resolve().parents[1]
def source(path):return (ROOT/path).read_text(encoding="utf-8")

class EngineeringPreflight(unittest.TestCase):
    def test_source_and_test_targets(self):
        cm=source("CMakeLists.txt")
        for name in ("src/core/EngineeringDesignTools.cpp", "src/core/EngineeringDesignTools.h",
                     "src/ui/EngineeringWorkbenchDialog.cpp", "src/ui/EngineeringWorkbenchDialog.h",
                     "tests/engineering_design_tools_tests.cpp"):
            self.assertEqual(cm.count(name),1)
            self.assertTrue((ROOT/name).is_file())
        self.assertIn("add_test(NAME cadcore_engineering_preliminary_design",cm)
        self.assertGreaterEqual(cm.count("add_test(NAME "),79)
        self.assertRegex(cm,r"project\(CADWorkbench VERSION 1\.(?:61|62|63|64|65|66|67|70)\.0 LANGUAGES CXX\)")
    def test_registered_menu_and_dialog(self):
        cpp=source("src/ui/MainWindow.cpp")
        h=source("src/ui/MainWindow.h")
        dialog=source("src/ui/EngineeringWorkbenchDialog.cpp")
        self.assertIn("void showEngineeringWorkbench();",h)
        self.assertIn("void MainWindow::showEngineeringWorkbench()",cpp)
        self.assertIn("&MainWindow::showEngineeringWorkbench",cpp)
        self.assertIn("BRepGProp::VolumeProperties(part->shape,properties)",cpp)
        for marker in ("filterTools", "showSelectedTool", "updateCalculation", "fillFromSelectedBody",
                       "evaluateEngineeringTool", "QSaveFile", "clipboard()->setText"):
            self.assertIn(marker,dialog)
        self.assertNotIn("Q_OBJECT",source("src/ui/EngineeringWorkbenchDialog.h"))
    def test_added_cpp_balanced_punctuation(self):
        for path in ("src/core/EngineeringDesignTools.cpp", "src/core/EngineeringDesignTools.h",
                     "src/ui/EngineeringWorkbenchDialog.cpp", "src/ui/EngineeringWorkbenchDialog.h",
                     "tests/engineering_design_tools_tests.cpp"):
            stack=[]
            for kind,text in lex(source(path),CppLexer()):
                self.assertNotIn(kind, Error, path)
                if kind not in Punctuation: continue
                for ch in text:
                    if ch in "([{":stack.append(ch)
                    elif ch in ")]}":
                        self.assertTrue(stack, path+" unmatched "+ch)
                        self.assertEqual(stack.pop(), {")":"(","]":"[","}":"{"}[ch],path)
            self.assertFalse(stack,path+" leftover punctuation")

    def test_independent_core_contract(self):
        core=source("src/core/EngineeringDesignTools.cpp")
        self.assertEqual(core.count('        add("'),50)
        self.assertIn("std::isfinite(answer)",core)
        self.assertIn("std::floor(values[i]) != values[i]",core)
        self.assertIn("std::isfinite(values[i])",core)
        self.assertIn("Invalid geometry or unsuitable parameter combination",core)
        test=source("tests/engineering_design_tools_tests.cpp")
        self.assertIn("expected 50",test.lower())
        self.assertIn("goldens.size()==50",test)
        self.assertIn("!cad::evaluateEngineeringTool",test)

if __name__=="__main__": unittest.main(verbosity=2)

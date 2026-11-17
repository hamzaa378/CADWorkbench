"""Static integration safeguards for v1.70 associative pockets/source retargeting."""
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def read(path):return (ROOT/path).read_text(encoding="utf-8")

class SketchPocketPreflight(unittest.TestCase):
    def test_staged_pocket_is_atomic(self):
        core=read("src/core/SceneDocument.cpp")
        section=core.split("bool SceneDocument::addLinkedSketchPocket(",1)[1].split("bool SceneDocument::linkedSketchExtrusionCurrent",1)[0]
        self.assertIn("rebuildLinkedSketchSolid",section)
        self.assertIn("linkedBooleanResult",section)
        self.assertIn("linkedSketchCutEffective",section)
        self.assertIn("linkedSketchCutEffective(primary.shape,result,error)",core)
        self.assertIn("cutter.visible=false",section)
        self.assertIn("candidate.append(std::move(cutter))",section)
        self.assertIn("candidate.append(std::move(result))",section)
        self.assertIn("if(!propagateLinkedBodies(&candidate,error))return false;",section)
        self.assertIn("checkpoint();parts_=std::move(candidate);return true;",section)
        self.assertNotIn("parts_.append",section)
    def test_retarget_rebuilds_descendants_and_checks_source(self):
        core=read("src/core/SceneDocument.cpp")
        section=core.split("bool SceneDocument::retargetLinkedSketchExtrusion(",1)[1].split("bool SceneDocument::addLinkedSketchPocket",1)[0]
        for clause in ("source->hasConstrainedPolygonDefinition","constrainedPolygonGeometryToken",
                       "changed.linkedSketchSourceId=sourceId", "changed.linkedSketchSourceToken.clear()",
                       "if(!propagateLinkedBodies(&candidate,error))return false;"):
            self.assertIn(clause,section)
    def test_edit_commands_and_native_regressions(self):
        ui=read("src/ui/MainWindow.cpp")
        head=read("src/ui/MainWindow.h")
        test=read("tests/linked_sketch_extrusion_v170_tests.cpp")
        for phrase in ("void MainWindow::createLinkedSketchPocket", "retargetLinkedSketchExtrusion(",
                       "document_.addLinkedSketchPocket("):
            self.assertIn(phrase,ui)
        self.assertIn("void createLinkedSketchPocket();",head)
        for mode in ("retarget", "pocket", "pocket-persistence"):
            self.assertIn('mode=="'+mode+'"',test)
            self.assertIn('COMMAND linked_sketch_extrusion_v170_tests '+mode,read("CMakeLists.txt"))
    def test_111_unique_ctests(self):
        cm=read("CMakeLists.txt")
        names=re.findall(r"add_test\s*\(\s*NAME\s+(\S+)",cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        self.assertIn("[int]$ExpectedTests = 130",read("scripts/verify_release_windows.ps1"))

if __name__=="__main__": unittest.main()

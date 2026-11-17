"""v1.70 additive external-sketch boss: native integration and invariants."""
import re
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(path):return (ROOT/path).read_text(encoding="utf-8")
class BossSourceContract(unittest.TestCase):
    def test_guard_rejects_detached_and_ineffective_fuses(self):
        source=read("src/core/SceneDocument.cpp")
        self.assertIn("bool linkedSketchFuseEffective(",source)
        self.assertIn("gained<=std::max(1e-7,before.Mass()*1e-9)",source)
        self.assertIn("TopExp_Explorer explorer(fused,TopAbs_SOLID)",source)
        self.assertIn("if(solids!=1)",source)
        self.assertIn("!linkedSketchFuseEffective(primary.shape,result,error)",source)
        self.assertIn("!linkedSketchFuseEffective(candidate[a].shape,expected,error)",source)
    def test_atomic_two_body_creation_and_ui(self):
        core=read("src/core/SceneDocument.cpp")
        section=core.split("bool SceneDocument::addLinkedSketchBoss(",1)[1].split("bool SceneDocument::linkedSketchExtrusionCurrent",1)[0]
        for expected in ('rebuildLinkedSketchSolid','linkedBooleanResult','linkedSketchFuseEffective',
                         'tool.visible=false','result.hasLinkedBoolean=true',
                         'candidate.append(std::move(tool))','candidate.append(std::move(result))',
                         'if(!propagateLinkedBodies(&candidate,error))return false;',
                         'checkpoint();parts_=std::move(candidate);return true;'):
            self.assertIn(expected,section)
        self.assertIn('void createLinkedSketchBoss();',read('src/ui/MainWindow.h'))
        self.assertIn('document_.addLinkedSketchBoss(',read('src/ui/MainWindow.cpp'))
    def test_native_test_modes_and_gate(self):
        cm=read('CMakeLists.txt')
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cm)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        test=read('tests/linked_sketch_extrusion_v170_tests.cpp')
        for case in ('boss','boss-persistence'):
            self.assertIn('mode=="'+case+'"',test)
            self.assertIn('COMMAND linked_sketch_extrusion_v170_tests '+case,cm)
        self.assertIn('ExpectedTests = 130',read('scripts/verify_release_windows.ps1'))
if __name__=='__main__':unittest.main()

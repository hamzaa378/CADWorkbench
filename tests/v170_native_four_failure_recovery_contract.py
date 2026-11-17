import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCENE = (ROOT / "src/core/SceneDocument.cpp").read_text(encoding="utf-8")
MATE = (ROOT / "tests/assembly_mate_document_v170_tests.cpp").read_text(encoding="utf-8")
FOLDED = (ROOT / "tests/cadcore_tests.cpp").read_text(encoding="utf-8")

class NativeFailureRecoveryContracts(unittest.TestCase):
    def test_suppressed_hole_ignored_only_for_inactive_sketch(self):
        section = SCENE.split('QStringList parsedSuppressedFeatures;', 1)[1].split('candidate.last().hasParametricDefinition=true;', 1)[0]
        for phrase in ('const bool activeHole=', 'const bool activePattern=', 'if(activeHole&&',
                       'furthestX', 'furthestY', 'activePattern&&spacing<'):
            self.assertIn(phrase, section)
        self.assertIn('candidate.last().suppressedParametricFeatures=std::move(parsedSuppressedFeatures)', SCENE)
        self.assertIn('Saved active parametric hole is outside', SCENE)

    def test_sampled_workspace_reports_actual_brep_provenance(self):
        section = SCENE.split('bool SceneDocument::buildLiveArticulatedEnvelopeCsv(',1)[1].split('bool SceneDocument::updateLiveArticulatedCoordinates(',1)[0]
        for phrase in ('stageLiveArticulatedRegeneration(', 'BRepBndLib::Add(',
                       'sampled pose AABB', 'sameTime.observe(', 'simultaneousOverlapSamples'):
            self.assertIn(phrase,section)
        self.assertIn('const int intervals=baseIntervals*refinement;',section)

    def test_clearance_fixture_requires_unambiguous_shortfall(self):
        self.assertIn('&marginReport,&issue,18.0)',MATE)
        self.assertIn('2 below-required-clearance pairs',MATE)
        self.assertIn('required 18 mm',MATE)
        self.assertIn('PAIR:',MATE)
        self.assertIn('fit::statusName(pair.status)',SCENE)

    def test_folded_crash_keeps_test_enabled_and_emits_markers(self):
        self.assertIn('if(argc==2&&std::string(argv[1])=="--folded-sheet-metal")',FOLDED)
        for phrase in ('initial folded model', 'signed neutral development', 'linked blank regeneration',
                       'structural linked flange change', 'relief cut B-rep and exports',
                       'self-intersection rejection'):
            self.assertIn('[folded phase] '+phrase,FOLDED)
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('add_test(NAME cadcore_folded_sheet_metal COMMAND cadcore_tests --folded-sheet-metal)',cmake)

if __name__ == "__main__": unittest.main()

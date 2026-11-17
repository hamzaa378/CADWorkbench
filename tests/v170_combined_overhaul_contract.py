"""Source integration contracts for the combined v1.70 recovery pass.

These are not native C++/Qt/OpenCascade execution tests. The separate native
regressions in interleaved_history, articulated_assembly_scene and cadcore_tests
must pass on the Windows toolchain.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
HEADER=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')
UI=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')

class CombinedOverhaulContract(unittest.TestCase):
    def test_msvc_undeclared_seterror_c3861_repaired_in_free_helper(self):
        helper=CORE.split('bool validArticulatedRebind(',1)[1].split('bool SceneDocument::previewLiveArticulatedJointRebind(',1)[0]
        self.assertIn('if(error)*error=',helper)
        self.assertNotIn('setError(error',helper)
        self.assertIn('parent==child',helper)

    def test_dependency_preflight_uses_real_commit_planner_in_disposable_document(self):
        body=CORE.split('bool SceneDocument::previewInterleavedHistoryWithDependents(',1)[1].split('bool SceneDocument::interleavedFeatureHistoryCurrent(',1)[0]
        self.assertIn('staged.parts_=parts_',body)
        self.assertIn('staged.setInterleavedFeatureHistory(id,steps,&issue)',body)
        self.assertIn('BRepCheck_Analyzer(after.shape).IsValid()',body)
        self.assertLess(body.index('staged.setInterleavedFeatureHistory'),body.index('*report=lines.join'))
        self.assertIn('previewInterleavedHistoryWithDependents',HEADER)

    def test_histories_revalidate_dependents_before_save(self):
        block=UI.split('void MainWindow::editInterleavedFeatureHistory()',1)[1].split('void MainWindow::',1)[0]
        self.assertIn('previewInterleavedHistoryWithDependents(target,steps',block)
        self.assertLess(block.index('previewInterleavedHistoryWithDependents(target,steps'),block.index('setInterleavedFeatureHistory(target,steps'))

    def test_core_strict_assembly_commit_does_not_trust_gui_previews(self):
        body=CORE.split('bool SceneDocument::commitLiveArticulatedCoordinatesChecked(',1)[1].split('namespace {\n// An edited joint stays',1)[0]
        for phrase in ('previewLiveArticulatedCoordinates(targets',
                       'previewLiveArticulatedTrajectory(targets',
                       'rejectUnsafe&&unsafe',
                       'updateLiveArticulatedCoordinates(targets,error)'):
            self.assertIn(phrase,body)
        self.assertLess(body.index('previewLiveArticulatedTrajectory(targets'),body.index('updateLiveArticulatedCoordinates(targets,error)'))
        self.assertIn('commitLiveArticulatedCoordinatesChecked(targets',UI)
        self.assertIn('commitLiveArticulatedCoordinatesChecked',HEADER)

    def test_linked_sheet_bundle_has_atomic_callers_outputs(self):
        body=CORE.split('bool SceneDocument::buildLinkedFlatDevelopmentSet(',1)[1].split('bool SceneDocument::editFoldedSheetMetal(',1)[0]
        for phrase in ('linkedFoldedFlatSourceToken',
                       'buildFoldedSheetFlatPatternSvg(',
                       'buildFoldedSheetFlatPatternDxf(',
                       'buildFoldedSheetBendTableCsv(',
                       'equivalentFoldedSheetDevelopment(',
                       '*svg=std::move(stagedSvg)'):
            self.assertIn(phrase,body)
        self.assertLess(body.index('equivalentFoldedSheetDevelopment('),body.index('*svg=std::move(stagedSvg)'))
        self.assertIn('buildLinkedFlatDevelopmentSet(flat->id',UI)
        self.assertIn('buildLinkedFlatDevelopmentSet',HEADER)

    def test_native_regression_files_cover_all_areas(self):
        self.assertIn('full dependent-feature preview must validate',
                      (ROOT/'tests/interleaved_history_v170_tests.cpp').read_text(encoding="utf-8"))
        self.assertIn('strict core commit accepted unsafe articulated clearance',
                      (ROOT/'tests/articulated_assembly_scene_v170_tests.cpp').read_text(encoding="utf-8"))
        self.assertIn('associated flat bundle was not regenerated',
                      (ROOT/'tests/cadcore_tests.cpp').read_text(encoding="utf-8"))

if __name__=='__main__':unittest.main()

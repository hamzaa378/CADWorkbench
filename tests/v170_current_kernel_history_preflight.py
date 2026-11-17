"""Source integration checks for current-build kernel history and intermediate witnesses.

These do not replace MSVC/Qt/OpenCascade compilation or the native CTests.
"""
import re
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def load(name):return (ROOT/name).read_text(encoding='utf-8')

class CurrentKernelHistoryContracts(unittest.TestCase):
    def test_true_kernel_history_lifetime_and_transactional_publish(self):
        header=load('src/core/ShapeFeatureTree.h')
        core=load('src/core/ShapeFeatureTree.cpp')
        doc=load('src/core/SceneDocument.cpp')
        self.assertIn('std::shared_ptr<BRepBuilderAPI_MakeShape>* kernelHistory = nullptr',header)
        self.assertIn('std::shared_ptr<BRepBuilderAPI_MakeShape> producedHistory;',core)
        self.assertIn('if(kernelHistory)*kernelHistory=std::move(producedHistory);',core)
        self.assertIn('producedHistory=fillet;',core)
        self.assertIn('producedHistory=chamfer;',core)
        self.assertIn('producedHistory=fuse;',core)
        self.assertIn('producedHistory=cut;',core)
        self.assertIn('Recording kernel history requires exactly one feature stage.',core)
        self.assertIn('!boolean.Shape().IsSame(result)',load('src/core/BooleanReferenceLineage.cpp'))
        self.assertIn('QMap<QString,std::shared_ptr<BRepBuilderAPI_MakeShape>> modifierHistory;',doc)
        self.assertIn('modifierHistory.insert(step.id,std::move(recordedModifier));',doc)
        self.assertIn('addParents(modifierSources);',doc)
        self.assertIn('if(!rebuildShapeFeatureTree(input,false,{executable},&next,error,',doc)

    def test_new_history_is_used_before_exact_only_fallback(self):
        doc=load('src/core/SceneDocument.cpp')
        segment=doc.split('for(const std::string& stage:chain){')[1].split('candidate=std::move(rebound);')[0]
        self.assertLess(segment.find('}else if(modifierHistory.contains(descendant)){'),
                        segment.find('}else if(survivingModifierSources.contains(descendant)){'))
        self.assertIn('*modifierHistory.value(descendant)',segment)
        lineage=load('src/core/BooleanReferenceLineage.cpp')
        self.assertIn('BRepBuilderAPI_MakeShape& boolean',lineage)
        self.assertIn('boolean.Modified(original)',lineage)
        self.assertIn('boolean.Generated(original)',lineage)
        self.assertIn('collect(original);',lineage)
        self.assertNotIn('if(!boolean.IsDeleted(original))collect(original);',lineage)
        self.assertIn('const auto candidates=successors(result',lineage)

    def test_direct_stage_witness_and_ui_cannot_claim_ancestor(self):
        doc=load('src/core/SceneDocument.cpp')
        self.assertIn('const bool directStage=step.referenceSourceId==step.inputs.first();',doc)
        self.assertIn('routes.emplace_back();',doc)
        self.assertIn('if(!rebaseFeatureOnCurrentStage(evaluated.value(ancestor)',doc)
        ui=load('src/ui/MainWindow.cpp')
        self.assertIn('if(parentId==sourceId)',ui)
        self.assertIn('Direct-stage references cannot include upstream pattern selections.',ui)
        self.assertIn('reference source and parent',ui)
        lineage=load('src/core/BooleanReferenceLineage.cpp')
        self.assertIn('resolveCurvedFaceAnchor(current,anchor',lineage)
        self.assertIn('resolveStableEdgeAnchor(current,anchor',lineage)
        self.assertIn('resolvePlanarFaceAnchor(current,anchor',lineage)
        self.assertIn('The saved machining location left the updated face plane.',lineage)
        self.assertIn('validateShapeFeatureNodes({changed},error)',lineage)

    def test_native_cases_written_and_ctest_gate_unchanged(self):
        native=load('tests/face_lineage_v170_tests.cpp')
        for case in ('recorded fillet builder is retained',
                     'history from the wrong geometry transaction is rejected',
                     'face witness follows recorded fillet Modified descendants',
                     'additive loft fusion builder is retained',
                     'intermediate generated face re-resolves on current loft result',
                     'generated loft witness cannot attach to unrelated edited geometry'):
            self.assertIn(case,native)
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',load('CMakeLists.txt'))
        self.assertEqual(130,len(names))
        self.assertEqual(130,len(set(names)))

if __name__=='__main__':unittest.main()

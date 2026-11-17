"""Source-level integration contract; native CTest still required on Windows."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class MachiningHistoryContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text(encoding="utf-8")
        cls.lineage=(ROOT/'src/core/BooleanReferenceLineage.cpp').read_text(encoding="utf-8")
        cls.native=(ROOT/'tests/interleaved_history_v170_tests.cpp').read_text(encoding="utf-8")
    def test_face_operations_retain_kernel_builder(self):
        for typ in ('BRepAlgoAPI_Cut','BRepAlgoAPI_Fuse'):
            self.assertIn('std::make_shared<'+typ+'>(input,tool)',self.model)
            self.assertIn('std::make_shared<'+typ+'>(input,extrusion.Shape())',self.model)
        self.assertIn('producedHistory=cut;',self.model)
        self.assertIn('producedHistory=fuse;',self.model)
        self.assertIn('producedHistory=fused;',self.model)
    def test_lineage_supports_face_machining(self):
        for typ in ('Face normal ', 'Face rectangle ', 'Sketch polygon face boss', 'Sketch polygon face pocket'):
            self.assertIn(typ,self.lineage)
        self.assertIn('modifierHistory.insert(step.id', (ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8"))
    def test_unique_edge_successor_still_requires_witness(self):
        self.assertIn('if(!resolveStableEdgeAnchor(result,oldAnchor,&chosen,error)||!candidates.contains(chosen))',self.lineage)
        self.assertIn('clipped edge witness cannot be reassigned',self.native)
    def test_machining_center_checks_plane_and_trim(self):
        self.assertIn('surface.Plane().Distance(point)>1e-5',self.lineage)
        self.assertGreaterEqual(self.lineage.count('insideTrimmedPlanarFace('),5)
        self.assertIn('face survived but removed machining center cannot be reattached',self.native)
        self.assertIn('face witness and real machining center survive an unrelated pocket',self.native)
    def test_atomic_multi_branch_source_edit_and_undo_are_exercised(self):
        for contract in (
            'one invalid branch rejects entire source edit',
            'failed fan-out rebuild preserves all three geometry tokens',
            'failed fan-out rebuild preserves every body volume',
            'failed fan-out rebuild keeps both links current',
            'valid fan-out source edit regenerates both siblings',
            'undo fan-out restores both branch identity tokens',
            'fan-out redo reapplies both dependent branches',
        ):
            self.assertIn(contract,self.native)
        self.assertIn('bool insideTrimmedPlanarFace(const TopoDS_Face& face,const gp_Pnt& point);',self.lineage)
    def test_legacy_face_replay_drops_both_witness_groups(self):
        # A secondary witness without the primary must never be accepted.
        self.assertIn('"orphaned secondary face witness must be rejected"',self.native)
        self.assertIn('"legacy face anchor without either witness group is valid"',self.native)
        self.assertIn('QStringLiteral("face_anchor_witness_")',self.native)
        self.assertIn('QStringLiteral("face_anchor_witness2_")',self.native)
        self.assertIn('check(!cad::validateShapeFeatureNodes({faceCut}',self.native)
        self.assertIn('check(cad::validateShapeFeatureNodes({faceCut}',self.native)
    def test_production_validation_stays_fail_closed(self):
        self.assertIn('(hasFaceWitness2&&!hasFaceWitness)',self.model)
        self.assertIn('!completeOptionalFields(node.parameters,kFaceWitness2Fields)',self.model)
        self.assertIn('machiningDiagnostic.constData()',self.native)
    def test_no_native_test_count_changed(self):
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding="utf-8")
        import re
        self.assertEqual(len(re.findall(r'(?m)^\s*add_test\s*\(',cmake)),130)
if __name__=='__main__': unittest.main()

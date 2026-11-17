"""Source integration contracts for 85%-target follow-up (not native acceptance)."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def code(name): return (ROOT/name).read_text(encoding='utf-8')

class FourFeatureFollowup(unittest.TestCase):
    def test_f01_rigid_curved_original_witness_preserved_and_native_exercised(self):
        cpp=code('src/core/BooleanReferenceLineage.cpp')
        tests=code('tests/curved_face_lineage_v170_tests.cpp')
        self.assertIn('if(moved.contains(key))fresh.insert(key,moved.value(key));',cpp)
        self.assertIn('two rigid curved stages drifted the original design witness',tests)
    def test_f02_two_stage_spline_edge_lineage(self):
        cpp=code('tests/selected_edge_v160_tests.cpp')
        self.assertIn('two-stage spline edge lineage',cpp)
        self.assertIn('two-stage spline witness drifted',cpp)
        self.assertIn('rigid spline transform changed local edge curvature',cpp)
    def test_f05_selected_planar_face_observations_and_nonplanar_rejection(self):
        header=code('src/core/PlanarMateSelection.h')
        native=code('tests/assembly_mate_document_v170_tests.cpp')
        self.assertIn('adaptor.GetType()!=GeomAbs_Plane',header)
        self.assertIn('normal.Reverse()',header)
        self.assertIn('guide.Normalize()',header)
        self.assertIn('cylinder must not be misidentified as a planar mate',native)
    def test_f05_ui_does_not_assume_planar_face_fixes_all_dof(self):
        ui=code('src/ui/MainWindow.cpp')
        self.assertIn('Prefill from two selected planar faces...',ui)
        self.assertIn('Two planar faces alone permit sliding and twisting',ui)
        self.assertIn('it->geometryToken,it->faceIndex,TopAbs_FACE',ui)
        self.assertIn('QMessageBox::No)!=QMessageBox::Yes',ui)
    def test_f08_pdf_snapshot_is_always_unissued(self):
        ui=code('src/ui/MainWindow.cpp')
        self.assertIn('exportOrthographicDrawingPdf(staged[index].svg,pdf',ui)
        self.assertIn('QDir(path).removeRecursively()',ui)
        self.assertIn('issue_status,pdf_file,current_geometry_token',ui)
        self.assertIn('UNISSUED REVIEW REQUIRED',ui)
        self.assertIn('newFolder',ui)
    def test_native_test_inventory_not_silently_reduced(self):
        import re
        cm=code('CMakeLists.txt')
        ids=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cm)
        self.assertEqual(len(ids),130)
        self.assertEqual(len(set(ids)),130)

if __name__=='__main__':unittest.main()

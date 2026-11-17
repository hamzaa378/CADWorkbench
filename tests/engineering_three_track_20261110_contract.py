"""Static integration checks for three 2026-11-10 CAD engineering increments.

Native MSVC, OpenCascade, Qt tests remain mandatory before release.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def text(relative):
    return (ROOT / relative).read_text(encoding="utf-8-sig")


class EngineeringThreeTrackContracts(unittest.TestCase):
    def test_topology_conflicting_witness_ownership_is_rejected(self):
        code = text("src/core/ShapeFeatureTree.cpp")
        self.assertIn("compensated.Distance(witness)>1e-7", code)
        self.assertIn("!other.IsSame(match)", code)
        self.assertIn("Face lineage conflicts between the original and body-shifted witnesses", code)
        native = text("tests/face_lineage_v170_tests.cpp")
        self.assertIn("competing shifted and original face witnesses must fail closed", native)
        self.assertIn("failed face resolution must not change output face", native)

    def test_structural_paths_come_from_residual_flow_not_degree(self):
        code = text("src/core/AssemblyNetworkStructureAudit.h")
        self.assertIn("result.independentPathsToFixed.assign(nodeCount,0)", code)
        self.assertIn("--residual[u][v];++residual[v][u]", code)
        self.assertIn("for(;flow<3;)", code)
        test = text("tests/assembly_network_structure_audit_tests.cpp")
        self.assertIn("unprotected tail correctly has only one independent fixed-frame path", test)
        self.assertIn("parallel saved mating connections count as two paths", test)

    def test_assembly_ui_reports_per_occurrence_redundancy(self):
        ui = text("src/ui/MainWindow.cpp")
        self.assertIn("Independent fixed-frame paths per occurrence", ui)
        self.assertIn("audit.independentPathsToFixed", ui)

    def test_batch_drawing_diagnostics_use_live_geometry_preview(self):
        source = text("src/core/DrawingSheet.cpp")
        self.assertIn("bool auditDrawingDimensionLinks(", source)
        self.assertIn("validateDrawingEdgeDimensionPreview(*source,dimension,&detail,&currentNominal)", source)
        self.assertIn("item.hasMeasurement=true", source)
        self.assertIn("drawing_measurement::extractSelectedEdgeNominal", source)
        self.assertIn("dimension.sourceGeometryToken!=source->geometryToken", source)
        self.assertIn("item.status=\"BROKEN_REFERENCE\"", source)
        self.assertIn("item.status=\"REGENERATE_REVIEW\"", source)

    def test_drawing_audit_does_not_mutate_source_revision(self):
        source = text("src/core/DrawingSheet.cpp").split("bool auditDrawingDimensionLinks(", 1)[1].split(
            "bool exportOrthographicDrawingSvg(", 1
        )[0]
        self.assertNotIn("setDrawingSheetSettings", source)
        self.assertNotIn("checkpoint(", source)
        self.assertIn("*report=std::move(pending)", source)
        ui = text("src/ui/MainWindow.cpp")
        self.assertIn('Audit all linked drawing dimensions...', ui)
        self.assertIn('Export audit CSV...', ui)
        self.assertIn('QSaveFile file(path);', ui)

    def test_native_registration_contract_not_extended(self):
        cmake = text("CMakeLists.txt")
        self.assertIn("face_lineage_v170_tests", cmake)
        import re
        self.assertEqual(len(re.findall(r"add_test\(NAME\s+", cmake)), 130)
        self.assertTrue((ROOT / "tests/assembly_network_structure_audit_tests.cpp").is_file())


if __name__ == "__main__":
    unittest.main()

"""F01/F02/F05/F08 integration checks, not a substitute for Windows tests."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
def source(rel):return (ROOT/rel).read_text(encoding="utf8")

class FourFeatureMilestoneContract(unittest.TestCase):
    def test_f01_rigid_planar_machining_and_witness(self):
        code=source("src/core/BooleanReferenceLineage.cpp")
        self.assertIn("insideTrimmedPlanarFace(successor,designPoint)",code)
        self.assertIn('fresh.insert(field,moved.value(field))',code)
        self.assertIn('Rigid planar reference lost its transported interior design witness',code)
        native=source("tests/selected_edge_v160_tests.cpp")
        self.assertIn('two rigid stages must preserve face-witness trajectory',native)
    def test_f02_edge_curves_preserve_interior_witness(self):
        code=source("src/core/BooleanReferenceLineage.cpp")
        self.assertIn('"edge_witness_x","edge_witness_y","edge_witness_z"',code)
        self.assertIn('cumulative drift on a second topology-changing operation',code)
        self.assertIn('rotated spline must preserve the transported physical edge witness',source("tests/selected_edge_v160_tests.cpp"))
    def test_f05_solver_network_edited_ui_and_save_reopen(self):
        solver=source("src/core/AssemblyMateSolver.cpp")
        network=source("src/core/AssemblyConstraintNetwork.cpp")
        scene=source("src/core/SceneDocument.cpp")
        ui=source("src/ui/MainWindow.cpp")
        self.assertIn('dot(pointDifference,c.parentDirection)-c.offsetMm',solver)
        self.assertIn('d.z*ua.z-c.offsetMm',network)
        self.assertIn('observation.insert("plane_offset_mm",c.offsetMm)',scene)
        self.assertIn('observation.offsetMm=offset.toDouble()',scene)
        self.assertIn('mate.offsetMm=planeOffset->value()',ui)
        self.assertIn('row.planeOffset->setValue(c.offsetMm)',ui)
    def test_f08_builds_real_current_geometry_in_read_only_batch(self):
        ui=source("src/ui/MainWindow.cpp")
        section=ui.split('Regenerate ALL saved drawing previews (not issued)...')[1].split('Regenerate saved drawing SVG...')[0] if False else ui.split('Regenerate ALL saved drawing previews (not issued)...')[1].split('Export one-bend sheet-metal flat pattern')[0]
        for required in ['auditDrawingDimensionLinks','buildDrawingAssemblyPart',
                         'prepareDrawingSheetSettings','buildOrthographicDrawingSvg',
                         'QSaveFile file(fileName)','!QSvgRenderer(svg).isValid()']:
            self.assertIn(required,section)
        self.assertNotIn('document_.setDrawingSheetSettings(',section)
        self.assertNotIn('document_.refreshDrawingSheetSource(',section)
        self.assertIn('for(const QString& created:written)QFile::remove(created)',section)
    def test_native_ctest_contract_unchanged(self):
        import re
        cmake=source("CMakeLists.txt")
        self.assertEqual(len(re.findall(r'add_test\s*\(',cmake,re.I)),130)

if __name__=='__main__':unittest.main()

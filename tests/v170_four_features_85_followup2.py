"""New source integration contracts. These are not a substitute for native/GUI acceptance."""
import csv
import re
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def source(p):return (ROOT/p).read_text(encoding='utf-8')
class FurtherFourFeatureContract(unittest.TestCase):
    def test_cylindrical_brep_observations(self):
        h=source('src/core/PlanarMateSelection.h')
        self.assertIn('describeSelectedCylindricalFace(',h)
        for symbol in ['GeomAbs_Cylinder','adaptor.Cylinder()','cylinder.Radius()',
                       'SurfaceProperties(face,props)','origin.Translated(',
                       '*output=measured']:
            self.assertIn(symbol,h)
    def test_cylinder_mate_ui_requires_explicit_remaining_locks(self):
        ui=source('src/ui/MainWindow.cpp')
        for item in ['Prefill from two selected cylindrical faces...',
                     'Concentric cylinders alone allow axial sliding and rotation',
                     'mateMode->setCurrentIndex(1)',
                     'describeSelectedCylindricalFace(',
                     'reference.geometryToken,reference.faceIndex,TopAbs_FACE']:
            self.assertIn(item,ui)
        self.assertIn('QMessageBox::No)!=QMessageBox::Yes',ui)
    def test_native_rejection_and_radius(self):
        native=source('tests/assembly_mate_document_v170_tests.cpp')
        self.assertIn('cylindrical mate observation lost B-rep radius',native)
        self.assertIn('plane must not be misidentified as a cylindrical mating surface',native)
        self.assertIn('failed cylindrical observation changed existing measurement',native)
    def test_drawing_digest_manifest(self):
        ui=source('src/ui/MainWindow.cpp')
        self.assertIn('svg_sha256,pdf_sha256',ui)
        self.assertIn('QCryptographicHash::Sha256',ui)
        self.assertIn('pdfContents.readAll()',ui)
        self.assertIn('QDir(path).removeRecursively()',ui)
        self.assertIn('UNISSUED REVIEW REQUIRED',ui)
    def test_topology_checks_not_removed(self):
        native=source('tests/selected_edge_v160_tests.cpp')
        cpp=source('src/core/BooleanReferenceLineage.cpp')
        self.assertIn('two-stage spline witness drifted',native)
        self.assertIn('Boolean successor no longer owns the original interior design witness.',cpp)
    def test_native_inventory(self):
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',source('CMakeLists.txt'))
        self.assertEqual(len(names),130)
        self.assertEqual(len(names),len(set(names)))
if __name__=='__main__':unittest.main()

"""Source integration contracts for three engineering improvements.

These checks never substitute for the native Qt/OpenCascade Windows build.
"""
import pathlib
import re
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
def source(relative):return (ROOT/relative).read_text(encoding="utf-8")

class ThreeEngineeringImprovements(unittest.TestCase):
    def test_witnessed_drawing_annotations_have_persistence(self):
        schema=source('src/core/SceneDocument.h')
        body=source('src/core/SceneDocument.cpp')
        self.assertIn('QMap<QString,double> edgeAnchor;',schema)
        self.assertIn('annotation.insert("edge_anchor",witness);',body)
        self.assertIn('dimension.contains("edge_anchor")',body)
        self.assertIn('validDrawingEdgeWitness(witness)',body)
        self.assertIn('dimension.edgeAnchor.isEmpty()',body)
        self.assertIn('Persisted drawing edge witness is invalid.',body)
        self.assertIn('validDrawingEdgeWitness(dimension.edgeAnchor)',body)

    def test_model_drawing_remeasures_current_witness_not_old_index(self):
        draw=source('src/core/DrawingSheet.cpp')
        ui=source('src/ui/MainWindow.cpp')
        self.assertIn('resolveStableEdgeAnchor(source->shape,annotation.edgeAnchor',draw)
        self.assertIn('remeasured.sourceGeometryToken=source->geometryToken;',draw)
        self.assertIn('Legacy index-only drawing dimension is stale',draw)
        self.assertIn('TopExp::MapShapes(part->shape,TopAbs_EDGE,uniqueEdges);',ui)
        self.assertIn('captureStableEdgeAnchor(part->shape,*uniqueIndex',ui)
        self.assertIn('validDrawingEdgeWitness(annotation.edgeAnchor)',draw)
        self.assertIn('view.edgeLengths.append({remeasured,center,measured})',draw)

    def test_coaxial_generated_face_is_bounded_and_fail_closed(self):
        lineage=source('src/core/BooleanReferenceLineage.cpp')
        self.assertIn('findDirectStageCoaxialCylinder(',lineage)
        self.assertIn('std::abs(nextRadius-radius)>0.20*radius',lineage)
        self.assertIn('classification.State()!=TopAbs_IN',lineage)
        self.assertIn('if(matches!=1)',lineage)
        self.assertIn('!findDirectStageCoaxialCylinder(current,anchor,&selected,error)',lineage)
        self.assertIn('Direct-stage generated cylinder',source('tests/curved_face_lineage_v170_tests.cpp'))

    def test_multibranch_recovers_more_routes_but_never_unbounded(self):
        lineage=source('src/core/BooleanLineagePath.h')
        native=source('tests/boolean_lineage_path_v170_tests.cpp')
        self.assertIn('maxPaths=32',lineage)
        self.assertIn('std::set<std::string> uniqueParents',lineage)
        self.assertIn('routes.size()==20',native)
        self.assertIn('error.find("32 routes")',native)
        self.assertIn('routes==deduplicatedRoutes',native)

    def test_native_regression_stays_in_registered_suites(self):
        suites=source('tests/cadcore_tests.cpp')
        self.assertIn('Save/reopen discarded selected-edge witness',suites)
        self.assertIn('Drawing falsely accepted a missing geometric witness',suites)
        self.assertIn('Witnessed dimension did not follow a translated source body',suites)
        cmake=source('CMakeLists.txt')
        names=re.findall(r'add_test\s*\(\s*NAME\s+(\S+)',cmake)
        self.assertEqual(130,len(names))
        self.assertEqual(len(names),len(set(names)))

if __name__=='__main__':unittest.main()

"""Source integration checks for v1.57. These are NOT a C++ build or CAD kernel tests."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
def src(file):return (ROOT/file).read_text(encoding="utf-8")

class DependencySketchV157Contract(unittest.TestCase):
    def test_schema_is_versioned_but_accepts_legacy(self):
        s=src("src/core/SceneDocument.cpp")
        self.assertIn('root.insert("schema",kSchema24)',s)
        self.assertIn('const QString kSchema23',s)
        self.assertIn('schema!=kSchema23&&schema!=kSchema24',s)
        self.assertIn('sketchPlaneToJson(part.constrainedPolygonPlane)',s)
        self.assertIn('sketchPlaneFromJson(',s)

    def test_curves_have_distinct_persistent_entities(self):
        data=src("src/core/SketchSolver.h")
        core=src("src/core/SketchProfile.cpp")
        json=src("src/core/SketchSolver.cpp")
        for name in ("SketchArc","SketchSpline","midpointId","control1PointId","control2PointId"):
            self.assertIn(name,data)
        for name in ("GC_MakeArcOfCircle","Geom_BezierCurve","BRepBuilderAPI_MakeWire","BRepCheck_Analyzer"):
            self.assertIn(name,core)
        serializer=json[json.index('QJsonObject sketchModelToJson('):json.index('bool sketchModelFromJson(')]
        solver=json[json.index('SketchSolveResult SketchSolver::solve('):json.index('QJsonObject sketchModelToJson(')]
        for name in ('for(const SketchArc& arc:model.arcs)', 'for(const SketchSpline& spline:model.splines)',
                     'result.insert("arcs",arcs)', 'result.insert("splines",splines)'):
            self.assertIn(name,serializer)
            self.assertNotIn(name,solver)
        self.assertIn('parsed.arcs.append',json)
        self.assertIn('parsed.splines.append',json)

    def test_plane_is_applied_to_exact_geometry(self):
        core=src("src/core/SketchProfile.cpp")
        for name in ('normal.Crossed','toWorld.SetValues','BRepBuilderAPI_Transform',
                     'validSketchPlane','pointSegmentDistance'):
            self.assertIn(name,core)

    def test_graph_validate_before_commit(self):
        core=src("src/core/ShapeFeatureTree.cpp")
        doc=src("src/core/SceneDocument.cpp")
        for name in ('"@base"','identities.contains','"Fuse branches"','"Subtract branches"',
                     'outputs.insert(op.id,next)','"Face normal pocket"','"Face normal boss"'):
            self.assertIn(name,core)
        segment=doc[doc.index('bool SceneDocument::setShapeFeatureTree('):doc.index('bool SceneDocument::rebaseShapeFeatureTree(')]
        self.assertLess(segment.index('rebuildShapeFeatureTree('),segment.index('checkpoint();'))
        self.assertIn('target.constrainedPolygonGeometryToken=target.geometryToken',segment)

    def test_sketch_regeneration_transactional(self):
        doc=src("src/core/SceneDocument.cpp")
        segment=doc[doc.index('bool SceneDocument::updateConstrainedPolygonExtrusion('):doc.index('bool SceneDocument::addParametricRectangularExtrusionAt(')]
        for name in ('buildSketchExtrusion','rebuildShapeFeatureTree','checkpoint()',
                     'shapeFeatureBase=base','shapeFeatureGeometryToken=part.geometryToken'):
            self.assertIn(name,segment)
        self.assertLess(segment.index('rebuildShapeFeatureTree'),segment.index('checkpoint();'))

    def test_visual_editor_commands_integrated(self):
        ui=src('src/ui/MainWindow.cpp')
        canvas=src('src/ui/SketchCanvasDialog.cpp')
        for name in ('createSketchOnSelectedFace','Sketch on selected planar face','Edit sources',
                     'Face normal pocket','Fuse branches'):
            self.assertIn(name,ui)
        for name in ('Circular arc','Bezier spline','setPlane','Sketch datum plane',
                     'nearestBoundaryEdge','completeCurvePoint'):
            self.assertIn(name,canvas)

    def test_native_test_registered(self):
        cmake=src('CMakeLists.txt')
        self.assertRegex(cmake,r'project\(CADWorkbench VERSION 1\.(?:5[7-9]|[6-9][0-9])\.')
        for name in ('SketchProfile.cpp',
                     'dependency_sketch_v157_tests.cpp','cadcore_dependency_sketch_v157'):
            self.assertIn(name,cmake)
        cases=src('tests/dependency_sketch_v157_tests.cpp')
        for name in ('Fuse branches','Subtract branches','Face normal pocket',
                     'updateConstrainedPolygonExtrusion','undo()','redo()','saveAs','!branchLoaded.load'):
            self.assertIn(name,cases)

if __name__=='__main__':unittest.main(verbosity=2)

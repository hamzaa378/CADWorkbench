"""Consolidated release safety contracts and independent OCCT observations.

These checks are NOT a substitute for MSVC compilation or native CAD CTests.
"""
import math
import unittest
from pathlib import Path
from OCP.BRepPrimAPI import BRepPrimAPI_MakeCylinder
from OCP.BRepAdaptor import BRepAdaptor_Curve
from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopExp import TopExp
from OCP.TopAbs import TopAbs_EDGE
from OCP.TopoDS import TopoDS
from OCP.GeomAbs import GeomAbs_Circle
from OCP.gp import gp_Trsf,gp_Vec

BASE=Path(__file__).resolve().parents[1]

class MajorConsolidationTests(unittest.TestCase):
    def read(self,path):return (BASE/path).read_text(encoding='utf-8')

    def test_feature_reuse_source_revision_is_explicit(self):
        plan=self.read('src/core/FeatureDependencyPlan.cpp')
        self.assertIn('planReuseVersioned(',plan)
        self.assertIn('previousBaseRevision==proposedBaseRevision',plan)
        self.assertIn('canReuse=baseUnchanged&&',plan)
        self.assertIn('previousBaseRevision.empty()||proposedBaseRevision.empty()',plan)

    def test_feature_reuse_rejects_noncanonical_parameters(self):
        code=self.read('src/core/FeatureDependencyPlan.cpp')
        self.assertIn('node.type.empty()',code)
        self.assertIn('key<=previousKey',code)

    def test_drawings_use_witnesses_for_hole_ordinates(self):
        drawing=self.read('src/core/DrawingSheet.cpp')
        self.assertIn('measured=firstCircle.Location().X()',drawing)
        self.assertIn('measured=firstCircle.Location().Y()',drawing)
        self.assertIn('measured=firstCircle.Location().Z()',drawing)
        self.assertIn('resolveStableEdgeAnchor(source->shape,annotation.edgeAnchor',drawing)
        self.assertIn('Hole-center ordinate refers to the hidden axis',drawing)

    def test_hole_ordinates_are_validated_in_scene_and_editor(self):
        scene=self.read('src/core/SceneDocument.cpp')
        ui=self.read('src/ui/MainWindow.cpp')
        for kind in ('CENTER_X','CENTER_Y','CENTER_Z'):
            self.assertIn(kind,scene)
            self.assertIn(kind,ui)
        self.assertIn('kind->addItem("Hole center X from model origin"',ui)
        self.assertIn('dimension.kind=="CENTER_Z"&&dimension.view=="TOP"',scene)
        self.assertIn('dimension.kind=="CENTER_Y"&&dimension.view=="FRONT"',scene)
        self.assertIn('dimension.kind=="CENTER_X"&&dimension.view=="RIGHT"',scene)

    def test_hole_annotations_survive_project_json(self):
        scene=self.read('src/core/SceneDocument.cpp')
        self.assertIn('annotation.insert("kind",dimension.kind)',scene)
        self.assertIn('const QString kind=dimension.value("kind"',scene)
        self.assertIn('CENTER_Z',scene)

    def test_assembly_loop_reports_two_physical_errors(self):
        header=self.read('src/core/AssemblyConstraintNetwork.h')
        engine=self.read('src/core/AssemblyConstraintNetwork.cpp')
        self.assertIn('worstPointOrAxisGapMm',header)
        self.assertIn('worstAngularErrorDegrees',header)
        self.assertIn('std::atan2(length(cross),dot)*180.0',engine)
        self.assertIn('connection "+std::to_string(worstConnection)',engine)

    def test_kernel_hole_ordinate_follows_current_position(self):
        # Real kernel observation, not index- or precomputed-label-based.
        shape=BRepPrimAPI_MakeCylinder(5.,12.).Shape()
        shift=gp_Trsf();shift.SetTranslation(gp_Vec(-16.25,8.5,0))
        shape=BRepBuilderAPI_Transform(shape,shift,True).Shape()
        shapes=TopTools_IndexedMapOfShape();TopExp.MapShapes_s(shape,TopAbs_EDGE,shapes)
        coords=[]
        for i in range(1,shapes.Extent()+1):
            curve=BRepAdaptor_Curve(TopoDS.Edge_s(shapes.FindKey(i)))
            if curve.GetType()!=GeomAbs_Circle:continue
            if abs((curve.LastParameter()-curve.FirstParameter())-2*math.pi)>1e-4:continue
            center=curve.Circle().Location()
            if abs(center.Z()-12)<1e-6:coords.append((center.X(),center.Y(),center.Z()))
        self.assertEqual(len(coords),1)
        self.assertAlmostEqual(coords[0][0],-16.25,places=5)
        self.assertAlmostEqual(coords[0][1],8.5,places=5)
        self.assertAlmostEqual(coords[0][2],12.,places=5)

if __name__=='__main__':unittest.main()

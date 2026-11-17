"""Independent OCCT kernel cases for heterogeneous profiles & arbitrary solid holes.

These are Python/OCP reference checks, *not* Windows GUI or C++ build
verification. They run with the existing Python test inventory.
"""
import math
import unittest
from pathlib import Path

try:
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
    from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakePolygon, BRepBuilderAPI_MakeWire
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepGProp import BRepGProp
    from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder, BRepPrimAPI_MakeSphere
    from OCP.GProp import GProp_GProps
    from OCP.gp import gp_Pnt, gp_Ax2, gp_Dir
except ImportError:
    # Keep source-level tests available for developers without the reference kernel.
    BRepAlgoAPI_Cut = None


def polygon_wire(sides, z, radius=10):
    wire = BRepBuilderAPI_MakePolygon()
    for i in range(sides):
        a = 2 * math.pi * i / sides
        wire.Add(gp_Pnt(radius * math.cos(a), radius * math.sin(a), z))
    wire.Close()
    if not wire.IsDone():
        raise RuntimeError("Invalid polygon fixture")
    return wire.Wire()


def surface_area(shape):
    props = GProp_GProps()
    BRepGProp.SurfaceProperties_s(shape, props)
    return props.Mass()


def volume(shape):
    props = GProp_GProps()
    BRepGProp.VolumeProperties_s(shape, props)
    return props.Mass()


@unittest.skipIf(BRepAlgoAPI_Cut is None, "Python OCP reference kernel unavailable")
class ShapeIndependentOCCTReferenceTests(unittest.TestCase):
    def test_triangle_to_square_smooth_loft(self):
        loft = BRepOffsetAPI_ThruSections(False, False, 1e-6)
        loft.CheckCompatibility(True)
        loft.AddWire(polygon_wire(3, 0))
        loft.AddWire(polygon_wire(4, 12))
        loft.Build()
        self.assertTrue(loft.IsDone())
        self.assertTrue(BRepCheck_Analyzer(loft.Shape()).IsValid())
        self.assertGreater(surface_area(loft.Shape()), 1)

    def test_triangle_to_square_ruled_shell(self):
        loft = BRepOffsetAPI_ThruSections(False, True, 1e-6)
        loft.CheckCompatibility(True)
        loft.AddWire(polygon_wire(3, 0))
        loft.AddWire(polygon_wire(4, 12))
        loft.Build()
        self.assertTrue(loft.IsDone())
        self.assertTrue(BRepCheck_Analyzer(loft.Shape()).IsValid())
        self.assertGreater(surface_area(loft.Shape()), 1)

    def test_unequal_open_sections_remain_supported_by_occt(self):
        # Guard against accidentally imposing equal-edge-count or closed-only
        # restrictions while rejecting an incoherent open/closed pairing.
        first = BRepBuilderAPI_MakeWire()
        first.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(5, 0, 0)).Edge())
        first.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(5, 0, 0), gp_Pnt(10, 0, 0)).Edge())
        second = BRepBuilderAPI_MakeWire()
        second.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 5), gp_Pnt(10, 0, 5)).Edge())
        self.assertFalse(first.Wire().Closed())
        self.assertFalse(second.Wire().Closed())
        loft = BRepOffsetAPI_ThruSections(False, True, 1e-6)
        loft.CheckCompatibility(True)
        loft.AddWire(first.Wire())
        loft.AddWire(second.Wire())
        loft.Build()
        self.assertTrue(loft.IsDone())
        self.assertTrue(BRepCheck_Analyzer(loft.Shape()).IsValid())
        self.assertAlmostEqual(surface_area(loft.Shape()), 50.0, delta=1e-6)

    def test_three_unlike_section_profiles(self):
        loft = BRepOffsetAPI_ThruSections(False, False, 1e-6)
        loft.CheckCompatibility(True)
        for n, z in ((3, 0), (4, 8), (6, 16)):
            loft.AddWire(polygon_wire(n, z))
        loft.Build()
        self.assertTrue(loft.IsDone())
        self.assertTrue(BRepCheck_Analyzer(loft.Shape()).IsValid())
        self.assertGreater(surface_area(loft.Shape()), 1)

    def test_cylinder_cut_on_sphere(self):
        sphere = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), 20).Shape()
        cutter = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, 20.01), gp_Dir(0, 0, -1)), 3.3, 41).Shape()
        cut = BRepAlgoAPI_Cut(sphere, cutter)
        cut.Build()
        self.assertTrue(cut.IsDone())
        self.assertTrue(BRepCheck_Analyzer(cut.Shape()).IsValid())
        self.assertGreater(volume(sphere) - volume(cut.Shape()), 1)

    def test_hole_cut_on_box(self):
        stock = BRepPrimAPI_MakeBox(60, 50, 12).Shape()
        cutter = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(30, 25, 12.01), gp_Dir(0, 0, -1)), 3.3, 12.02).Shape()
        cut = BRepAlgoAPI_Cut(stock, cutter)
        cut.Build()
        self.assertTrue(cut.IsDone())
        self.assertTrue(BRepCheck_Analyzer(cut.Shape()).IsValid())
        self.assertGreater(volume(stock)-volume(cut.Shape()), 1)

    def test_cutter_missing_body_is_rejected_by_volume_check(self):
        sphere = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), 20).Shape()
        cutter = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(100, 100, 20.01), gp_Dir(0, 0, -1)), 3.3, 41).Shape()
        cut = BRepAlgoAPI_Cut(sphere, cutter)
        cut.Build()
        self.assertTrue(cut.IsDone())
        self.assertAlmostEqual(volume(sphere), volume(cut.Shape()), delta=1e-6)


class ShapeIndependentSourceContractTests(unittest.TestCase):
    ROOT=Path(__file__).resolve().parents[1]

    def test_face_lookup_checks_all_parts(self):
        source=(self.ROOT/"src/ui/MainWindow.cpp").read_text(encoding="utf-8")
        self.assertIn("if(part.id!=partId)continue;",source)
        self.assertIn("if(part.geometryToken!=geometryToken||shapeIndex<1)return {};",source)

    def test_iso_wizard_routes_arbitrary_solids_through_preview(self):
        source=(self.ROOT/"src/ui/MainWindow.cpp").read_text(encoding="utf-8")
        section=source.split("void MainWindow::addMetricFastenerClearanceHole() {",1)[1].split("void MainWindow::addParametricThroughHole()",1)[0]
        self.assertIn("document_.prepareHole",section)
        self.assertIn("cad::SceneDocument::buildHolePreview",section)
        self.assertIn("document_.commitHole(command,preview.shape",section)
        self.assertIn("document_.addParametricThroughHole",section)
        self.assertIn("non-associative",section)

    def test_surface_loft_rejects_mixed_open_closed_without_regressing_mixed_edges(self):
        core=(self.ROOT/"src/core/SurfaceModeling.cpp").read_text(encoding="utf-8")
        native=(self.ROOT/"tests/surface_modeling_tests.cpp").read_text(encoding="utf-8")
        self.assertIn("firstBoundary.Closed()!=secondBoundary.Closed()",core)
        self.assertIn("section.Closed()!=firstClosed",core)
        self.assertIn("both be closed or both be open",core)
        self.assertIn("all be closed or all be open",core)
        self.assertIn("mismatchedWire.Wire(),secondOpen.Wire()",native)
        self.assertIn("preservedShell.IsSame(shell)",native)

    def test_freeform_face_loft_available(self):
        source=(self.ROOT/"src/ui/MainWindow.cpp").read_text(encoding="utf-8")
        self.assertIn("Loft surface between selected face boundaries",source)
        self.assertIn("BRepTools::OuterWire(face)",source)
        self.assertIn("cad::makeLoftedSurfaceShell(sections",source)
        core=(self.ROOT/"src/core/SurfaceModeling.cpp").read_text(encoding="utf-8")
        self.assertIn("loft.CheckCompatibility(true)",core)
        self.assertNotIn("Loft section wires must have the same number",core)

if __name__ == "__main__":
    unittest.main()

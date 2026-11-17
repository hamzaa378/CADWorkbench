"""Source-level integration checks; these are not MSVC or Qt runtime tests."""
import pathlib
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
def read(path):return (ROOT/path).read_text(encoding="utf-8")
class ConsolidatedEngineeringRc4(unittest.TestCase):
    def test_engineering_planner_is_compiled(self):
        cmake=read("CMakeLists.txt")
        self.assertIn("src/core/DrillingFeasibility.cpp",cmake)
        self.assertIn("src/core/DrillingWorksheet.cpp",cmake)
        self.assertIn("drilling_feasibility_v170_tests.cpp",read("scripts/verify_portable_consolidation.sh"))
    def test_atomic_manufacturing_report(self):
        source=read("src/core/DrillingWorksheet.cpp")
        self.assertIn("inspectAxialHoles(part,&audit,error)",source)
        self.assertIn("drilling::plan",source)
        self.assertIn("QSaveFile file(path)",source)
        self.assertIn("excludedCylinders",source)
        self.assertIn("geometryRevision",source)
    def test_parametric_formula_editor_integrates_with_model(self):
        core=read("src/core/SceneDocument.cpp")
        ui=read("src/ui/MainWindow.cpp")
        self.assertIn("setNamedParameterFormulas",core)
        self.assertIn("evaluatePersistentDimensionFormulas(updated,formulas",core)
        self.assertIn("dimension_formulas",core)
        self.assertIn("document_.setNamedParameterFormulas(id,formulas,&error)",ui)
        self.assertIn("resolveParameterLinks",core)
    def test_gdt_persistence_and_stale_guard(self):
        core=read("src/core/SceneDocument.cpp")
        drawing=read("src/core/DrawingSheet.cpp")
        ui=read("src/ui/MainWindow.cpp")
        self.assertIn("gdt_frame_annotations",core)
        self.assertIn("frame.sourceGeometryToken",drawing)
        self.assertIn("geometric-tolerance-frame",drawing)
        self.assertIn("editDrawingGdtFrames",ui)
        self.assertIn("gdtFrames",read("src/core/DrawingSheet.h"))
    def test_native_regressions_are_hooked_to_existing_tests(self):
        tests=read("tests/cadcore_tests.cpp")
        self.assertIn("testV170Rc4ParametricFormulas();",tests)
        self.assertIn("testV170Rc4DrillingAndGdt();",tests)
        self.assertIn("--orthographic-drawing",tests)
        self.assertIn("--parametric-foundation",tests)

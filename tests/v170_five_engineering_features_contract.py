"""Source wiring tests for five engineering developments, not GUI acceptance."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]

class FiveFeatureSourceContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui=(ROOT/"src/ui/MainWindow.cpp").read_text(encoding="utf-8")
        cls.runner=(ROOT/"tools/run_portable_regressions.py").read_text(encoding="utf-8")

    def test_cylinder_fit_core_and_gui(self):
        code=(ROOT/"src/core/CylindricalFitReview.h").read_text()
        self.assertIn("reviewCylindricalFit",code)
        self.assertIn("*result=proposed",code)
        self.assertIn("Review selected cylindrical fit (read only)",self.ui)
        self.assertIn("describeSelectedCylindricalFace",self.ui)
        self.assertIn("user identification, not inferred",self.ui)

    def test_hierarchy_is_explicit_and_read_only(self):
        code=(ROOT/"src/core/AssemblyHierarchyAudit.h").read_text()
        self.assertIn("auditHierarchy",code)
        self.assertIn("Hierarchy contains a parent cycle",code)
        self.assertIn("*result=std::move(report)",code)
        self.assertIn("Audit explicit occurrence hierarchy CSV",self.ui)
        self.assertIn("componentDefinitionId.isEmpty()",self.ui)
        self.assertIn("NOT inferred from mate links",self.ui)

    def test_motion_review_is_truthful_about_current_linear_track(self):
        code=(ROOT/"src/core/MotionDriveLimitReview.h").read_text()
        self.assertIn("reviewDriveLimits",code)
        self.assertIn("endpoint velocity jumps",code)
        self.assertIn("*result=std::move(staged)",code)
        self.assertIn("Review selected joint drive limits",self.ui)
        self.assertIn("Saved piecewise-LINEAR motion",self.ui)
        self.assertIn("Saved linear speed policy",self.ui)
        self.assertIn("Hypothetical SMOOTHSTEP trajectory",self.ui)

    def test_gdt_review_advisory_not_mutating(self):
        code=(ROOT/"src/core/GdtDatumReview.h").read_text()
        self.assertIn("reviewGdtDatumPolicy",code)
        self.assertIn("staged.push_back",code)
        self.assertIn("*result=std::move(staged)",code)
        self.assertIn("MANUAL GD&T DATUM REVIEW",self.ui)
        self.assertIn("owner->drawingSettings.gdtFrames",self.ui)

    def test_bend_relief_project_policy_not_geometry(self):
        code=(ROOT/"src/core/BendReliefPlanning.h").read_text()
        self.assertIn("reviewEndReliefs",code)
        self.assertIn("requestedWidthMm*2>=flatSheetWidthMm",code)
        self.assertIn("*output=std::move(staged)",code)
        self.assertIn("Proposed end-relief width",self.ui)
        self.assertIn("No relief cut was made",self.ui)

    def test_native_registration_unchanged_and_five_feature_compiled(self):
        cmake=(ROOT/"CMakeLists.txt").read_text(encoding="utf-8")
        names=re.findall(r"\badd_test\s*\(\s*NAME\s+([a-zA-Z0-9_\-]+)",cmake)
        self.assertEqual(len(names),130)
        self.assertEqual(len(set(names)),130)
        self.assertIn("five_feature_engineering_portable_tests.cpp",self.runner)
        self.assertTrue((ROOT/"tests/five_feature_engineering_portable_tests.cpp").is_file())

if __name__ == "__main__": unittest.main()

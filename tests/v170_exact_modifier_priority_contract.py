"""Source-level guard plus optional independent OpenCascade geometry fixture.

This does not replace the native Windows interleaved-history CTest.
"""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCENE = ROOT / "src" / "core" / "SceneDocument.cpp"


class ExactModifierPriorityContract(unittest.TestCase):
    def test_exact_successor_precedes_recorded_kernel_fallback(self):
        text = SCENE.read_text(encoding="utf-8")
        start = text.index("}else if(modifierHistory.contains(descendant)){")
        finish = text.index("}else if(survivingModifierSources.contains(descendant)){", start)
        branch = text[start:finish]
        self.assertLess(branch.index("rebaseFeatureThroughSurvivingModifier("),
                        branch.index("rebaseFeatureThroughBoolean("))
        self.assertIn("if(!exactSurvivor)", branch)
        self.assertIn("if(!rebaseFeatureThroughBoolean(", branch)
        self.assertIn("if(supportsSurvivingModifierLineage(step.feature))", text)
        self.assertNotIn("!modifierHistory.contains(step.id)",
                         text[text.index("// Register exact-identity ancestry"):text.index("tokens.append(QString());", start)])

    def test_failures_include_rebase_diagnostics(self):
        test = (ROOT / "tests" / "interleaved_history_v170_tests.cpp").read_text(encoding="utf-8")
        self.assertIn("unaffected edge survives a prior selective fillet:", test)
        self.assertIn("survivalError.toStdString()", test)

    def test_independent_fillet_preserves_opposite_edge(self):
        try:
            from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
            from OCP.BRepFilletAPI import BRepFilletAPI_MakeFillet
            from OCP.TopExp import TopExp
            from OCP.TopAbs import TopAbs_EDGE
            from OCP.TopTools import TopTools_IndexedMapOfShape
            from OCP.TopoDS import TopoDS
        except ImportError:
            self.skipTest("Independent OpenCascade Python bindings not installed")
        box = BRepPrimAPI_MakeBox(12, 12, 10).Shape()
        before = TopTools_IndexedMapOfShape()
        TopExp.MapShapes_s(box, TopAbs_EDGE, before)
        first = BRepFilletAPI_MakeFillet(box)
        first.Add(0.15, TopoDS.Edge_s(before.FindKey(1)))
        first.Build()
        self.assertTrue(first.IsDone())
        after = TopTools_IndexedMapOfShape()
        TopExp.MapShapes_s(first.Shape(), TopAbs_EDGE, after)
        matching = [i for i in range(1, after.Extent() + 1)
                    if after.FindKey(i).IsSame(before.FindKey(6))]
        self.assertEqual(len(matching), 1, "The opposite edge must remain identical")
        self.assertFalse(any(after.FindKey(i).IsSame(before.FindKey(1))
                             for i in range(1, after.Extent() + 1)),
                         "The selected rounded edge must be consumed")
        second = BRepFilletAPI_MakeFillet(first.Shape())
        second.Add(0.12, TopoDS.Edge_s(after.FindKey(matching[0])))
        second.Build()
        self.assertTrue(second.IsDone(), "The untouched edge must remain filletable")


if __name__ == "__main__":
    unittest.main()

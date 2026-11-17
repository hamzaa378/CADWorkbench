"""Regression guard for witnessed-edge retargeting and one-shot execution.

Static checks are not native Windows tests. The optional OCP geometry example
independently checks the actual two-fillet, downstream-boss fixture.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCENE = ROOT / 'src/core/SceneDocument.cpp'
TEST = ROOT / 'tests/interleaved_history_v170_tests.cpp'


class TrackedEdgeExecutionContract(unittest.TestCase):
    def test_ephemeral_edge_execution_retains_persisted_witness(self):
        code = SCENE.read_text(encoding='utf-8')
        replay = code.split('bool replayInterleavedHistory(', 1)[1].split('// Apply a persisted', 1)[0]
        for required in (
            'ShapeFeatureNode resolved=step.feature;',
            'resolved=std::move(referenceResult);',
            'ShapeFeatureNode executable=resolved;',
            'const bool trackedEdge=!step.referenceSourceId.isEmpty()',
            'if(!resolved.parameters.contains(QStringLiteral("edge_anchor_version"))',
            'indexValue>inputEdges.Extent()',
            'it.key().startsWith(QStringLiteral("edge_"))',
            'it.key()!=QStringLiteral("edge_index")',
            'rebuildShapeFeatureTree(input,false,{executable}',
        ):
            self.assertIn(required, replay)
        self.assertLess(replay.index('resolved=std::move(referenceResult);'),
                        replay.index('ShapeFeatureNode executable=resolved;'))
        self.assertLess(replay.index('ShapeFeatureNode executable=resolved;'),
                        replay.index('rebuildShapeFeatureTree(input,false,{executable}'))
        # The saved project keeps the full witnessed step; the temporary index
        # execution path must not erase anything from the persisted record.
        self.assertNotIn('step.feature.parameters.erase(', replay)

    def test_native_regression_reports_underlying_error(self):
        source = TEST.read_text(encoding='utf-8')
        self.assertIn('unaffected edge survives a prior selective fillet: ', source)
        self.assertIn('survivalError.toStdString()', source)
        self.assertIn('already consumed source edge cannot be retargeted', source)

    def test_two_fillets_then_boss_is_kernel_valid(self):
        try:
            from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
            from OCP.BRepFilletAPI import BRepFilletAPI_MakeFillet
            from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse
            from OCP.BRepCheck import BRepCheck_Analyzer
            from OCP.TopExp import TopExp
            from OCP.TopTools import TopTools_IndexedMapOfShape
            from OCP.TopAbs import TopAbs_EDGE
            from OCP.TopoDS import TopoDS
            from OCP.gp import gp_Pnt
        except ImportError:
            self.skipTest('Independent OCCT Python bindings unavailable')
        base = BRepPrimAPI_MakeBox(12, 12, 10).Shape()
        old = TopTools_IndexedMapOfShape()
        TopExp.MapShapes_s(base, TopAbs_EDGE, old)
        self.assertGreaterEqual(old.Extent(), 6)
        rounded = BRepFilletAPI_MakeFillet(base)
        rounded.Add(0.15, TopoDS.Edge_s(old.FindKey(1)))
        rounded.Build()
        self.assertTrue(rounded.IsDone())
        after = rounded.Shape()
        fresh = TopTools_IndexedMapOfShape()
        TopExp.MapShapes_s(after, TopAbs_EDGE, fresh)
        survivors = [i for i in range(1, fresh.Extent()+1)
                     if fresh.FindKey(i).IsSame(old.FindKey(6))]
        consumed = [i for i in range(1, fresh.Extent()+1)
                    if fresh.FindKey(i).IsSame(old.FindKey(1))]
        self.assertEqual(len(survivors), 1)
        self.assertEqual(consumed, [])
        second = BRepFilletAPI_MakeFillet(after)
        second.Add(0.12, TopoDS.Edge_s(fresh.FindKey(survivors[0])))
        second.Build()
        self.assertTrue(second.IsDone())
        boss = BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 8), gp_Pnt(8, 10, 13)).Shape()
        joined = BRepAlgoAPI_Fuse(second.Shape(), boss)
        joined.Build()
        self.assertTrue(joined.IsDone())
        self.assertTrue(BRepCheck_Analyzer(joined.Shape()).IsValid())


if __name__ == '__main__':
    unittest.main()

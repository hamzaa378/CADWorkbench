"""Integration checks for real editable multi-edge manufacturing blends.

These checks are NOT a substitute for native Windows/Qt/OpenCascade CTests.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class RealWorldEdgeGroupContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.core = (ROOT/'src/core/ShapeFeatureTree.cpp').read_text(encoding='utf-8')
        cls.ui = (ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
        cls.lineage = (ROOT/'src/core/BooleanReferenceLineage.cpp').read_text(encoding='utf-8')
        cls.native = (ROOT/'tests/shape_feature_tree_tests.cpp').read_text(encoding='utf-8')

    def test_real_editable_operations_not_only_audits(self):
        for operation in ('Fillet selected edges', 'Chamfer selected edges'):
            self.assertIn(f'{{"{operation}",{{"edge_count",', self.core)
            self.assertIn(f'node->type=="{operation}"', self.ui)
            self.assertIn(f'operation.type==QStringLiteral("{operation}")', self.lineage)

    def test_members_are_persisted_and_resolved_through_geometry_witness(self):
        for snippet in ('member_%1_', 'captureStableEdgeAnchor(body->shape',
                        'resolveStableEdgeAnchor(input,anchor,&selected,error)',
                        'if(selected<1||selected>edges.Extent()||!selectedIndices.insert(selected).second)',
                        'for(int i=0;i<expected;++i)', 'attempt.parameters.insert(prefix+"edge_index"',
                        'node->parameters=attempt.parameters;return true;'):
            self.assertTrue(snippet in self.core or snippet in self.ui, snippet)

    def test_atomic_feature_history_and_transactional_save_coverage(self):
        for snippet in ('setShapeFeatureTree(bodyId,{group}',
                        'setShapeFeatureTree(bodyId,{duplicate}',
                        'setShapeFeatureTree(bodyId,{corrupt}',
                        'setShapeFeatureTree(bodyId,{resized}',
                        'grouped.undo()', 'grouped.redo()',
                        'grouped.saveAs(saved', 'reloaded.load(saved',
                        'member_1_size', 'member_1_edge_witness_z'):
            self.assertIn(snippet,self.native)

    def test_grouped_edge_size_validation_and_fail_closed(self):
        for snippet in ('rawCount<2||rawCount>32',
                        'Individual edge sizes must be finite and positive.',
                        'Edge group members no longer refer to distinct edges.',
                        'Chamfer group members no longer refer to distinct edges.',
                        'const double memberSize=anchor.value("size",v("radius"))',
                        'const double memberSize=anchor.value("size",v("distance"))',
                        'anchor.remove("size");', 'fillet->Add(memberSize,',
                        'chamfer->Add(memberSize,'):
            self.assertIn(snippet,self.core)

    def test_independent_kernel_selected_edge_variants(self):
        try:
            from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
            from OCP.BRepFilletAPI import BRepFilletAPI_MakeFillet, BRepFilletAPI_MakeChamfer
            from OCP.TopAbs import TopAbs_EDGE
            from OCP.TopExp import TopExp
            from OCP.TopTools import TopTools_IndexedMapOfShape
            from OCP.TopoDS import TopoDS
            from OCP.BRepGProp import BRepGProp
            from OCP.GProp import GProp_GProps
        except ImportError:
            self.skipTest('OCP reference kernel unavailable')
        source=BRepPrimAPI_MakeBox(40,30,20).Shape()
        edges=TopTools_IndexedMapOfShape()
        TopExp.MapShapes_s(source,TopAbs_EDGE,edges)
        sourceProps=GProp_GProps()
        BRepGProp.VolumeProperties_s(source,sourceProps)
        self.assertAlmostEqual(sourceProps.Mass(),24000,places=5)
        for constructor in (BRepFilletAPI_MakeFillet,BRepFilletAPI_MakeChamfer):
            maker=constructor(source)
            maker.Add(.7,TopoDS.Edge_s(edges.FindKey(1)))
            maker.Add(1.3,TopoDS.Edge_s(edges.FindKey(3)))
            maker.Build()
            self.assertTrue(maker.IsDone())
            measure=GProp_GProps()
            BRepGProp.VolumeProperties_s(maker.Shape(),measure)
            self.assertGreater(measure.Mass(),23000)
            self.assertLess(measure.Mass(),sourceProps.Mass())

if __name__=='__main__':unittest.main()

"""Regressions covering the consolidated engineering-review architecture.

Source contracts supplement, never replace, native Windows tests.
"""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]

def code(name):
    return (ROOT/name).read_text(encoding="utf-8")

class ArchitectureReviewContracts(unittest.TestCase):
    def test_single_graph_builder_is_used_in_all_affected_workflows(self):
        src=code("src/core/SceneDocument.cpp")
        self.assertEqual(src.count("linkedgraph::Node node;"),1)
        self.assertEqual(src.count("node.additionalSources.push_back(stage.sourceId.toStdString());"),1)
        self.assertIn("const auto nodes=linkedBodyGraphNodes(parts);",src)
        self.assertIn("const auto nodes=linkedBodyGraphNodes(parts_);",src)
        self.assertIn("const auto nodes=linkedBodyGraphNodes(staged);",src)
        self.assertIn("explainAffectedMany(nodes,{sourceId.toStdString()}",src)

    def test_cause_tracing_is_transactional_and_is_visible(self):
        graph=code("src/core/LinkedBodyGraph.cpp")
        ui=code("src/ui/MainWindow.cpp")
        self.assertIn("proposed.pathIds<candidate.pathIds",graph)
        self.assertIn("*causes=std::move(result)",graph)
        self.assertIn("document_.linkedBodyImpact(ids.first(),&affected,&error,&explanations)",ui)
        self.assertIn("dependency: %4",ui)

    def test_mate_reinforcement_is_simulated_not_committed(self):
        core=code("src/core/AssemblyResiliencePlanner.h")
        ui=code("src/ui/MainWindow.cpp")
        self.assertIn("auditMateStructure(nodeCount,fixedNode,simulated,&updated",core)
        self.assertIn("vulnerableFixedFrameLinksAfter!=0",core)
        self.assertIn("*output=std::move(pending)",core)
        self.assertIn("GRAPH-ONLY REINFORCEMENT CANDIDATES",ui)
        self.assertIn("Do not create redundant duplicate observations",ui)
        self.assertNotIn("checkpoint()",core)

    def test_drawing_measurement_reuses_export_value_and_fails_closed(self):
        measure=code("src/core/DrawingDimensionMeasurement.h")
        core=code("src/core/DrawingSheet.cpp")
        ui=code("src/ui/MainWindow.cpp")
        self.assertIn('data-drawing-annotation',measure)
        self.assertIn('data-value-mm',measure)
        self.assertIn('if(occurrences!=1)',measure)
        self.assertIn('drawing_measurement::extractSelectedEdgeNominal(preview,dimension.id',core)
        self.assertIn('item.nominalMm=currentNominal',core)
        self.assertIn('current_nominal_mm,manual_min_mm,manual_max_mm',ui)
        self.assertIn('if(entry.hasMeasurement)',ui)

    def test_audit_registrations_and_evidence_gate_preserved(self):
        cmake=code('CMakeLists.txt')
        self.assertEqual(len(re.findall(r'add_test\(NAME\s+',cmake)),130)
        audit=code('RUN_V170_COMPREHENSIVE_AUDIT.ps1')
        self.assertIn('manual acceptance',audit.lower())
        portable=code('tools/run_portable_regressions.py')
        self.assertIn('linked_body_graph_v170_tests.cpp',portable)

if __name__=='__main__':
    unittest.main()

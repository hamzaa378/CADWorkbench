"""Sprint 10 structural integration gates; compiled regressions exercise behavior."""
import re
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def source(filename): return (ROOT/filename).read_text(encoding='utf-8')
class Sprint10Workflows(unittest.TestCase):
    def test_design_to_metrology_adapter_registered(self):
        self.assertIn('src/core/HoleInspectionWorkflow.h',source('CMakeLists.txt'))
        self.assertIn('#include "HolePositionInspection.h"',source('src/core/HoleInspectionWorkflow.h'))
    def test_revision_is_required_from_both_sources(self):
        code=source('src/core/HoleInspectionWorkflow.h')
        self.assertIn('it->second->geometryToken!=d.geometryToken',code)
        self.assertIn('!designKeys.emplace(key,true).second',code)
        self.assertIn('!measurements.emplace(Key{m.bodyId,m.holeNumber},&m).second',code)
    def test_nominal_coordinates_come_only_from_design(self):
        code=source('src/core/HoleInspectionWorkflow.h')
        self.assertIn('h.nominalXmm=d.nominalXmm;',code)
        self.assertIn('h.nominalYmm=d.nominalYmm;',code)
        self.assertIn('h.actualDiameterMm=m.actualDiameterMm;',code)
    def test_failed_metrology_is_atomic(self):
        code=source('src/core/HoleInspectionWorkflow.h')
        self.assertIn('return inspectHolePositionPattern(joined,report,error);',code)
    def test_signed_handoff_contains_unmeasured_template(self):
        code=source('src/core/ManufacturingHandoff.cpp')
        self.assertIn('hole_position_inspection_TEMPLATE.csv',code)
        self.assertIn('"UNMEASURED"',code)
        self.assertIn('measurementTemplate<<QStringList{safeCsv(part.id)',code)
    def test_no_csv_reparse_in_export_adapter(self):
        code=source('src/core/ManufacturingHandoff.cpp')
        self.assertNotIn('holeRows[i].split',code)
        self.assertIn('number(hole.diameterMm)',code)
    def test_python_integrity_checks_never_equate_template_to_measurement(self):
        code=source('tools/verify_manufacturing_handoff.py')
        self.assertIn('validate_inspection_template(folder)',code)
        self.assertIn('row["inspection_status"] != "UNMEASURED"',code)
        self.assertIn('fabricated or entered measurement data',code)
    def test_imported_assembly_pose_order_is_normalized(self):
        code=source('src/core/SimultaneousProximity.h')
        self.assertIn('class OrderInvariantTracker',code)
        self.assertIn('std::sort(canonical.begin()',code)
        self.assertIn('tracker_.observe(canonical,error)',code)
        self.assertIn('proximity::OrderInvariantTracker sameTime;',source('src/core/SceneDocument.cpp'))
    def test_native_behavior_scenarios_remain_in_existing_targets(self):
        code=source('tests/workshop_engineering_v164_tests.cpp')
        self.assertIn('revisionBoundHoleWorkflows();',code)
        self.assertIn('geometryToken="geom-rev-8"',code)
        self.assertIn('hole_position_inspection_TEMPLATE.csv',source('tests/document_audit_tests.cpp'))
    def test_130_historical_ctest_names_preserved(self):
        tests=re.findall(r'^\s*add_test\(\s*NAME\s+(\S+)',source('CMakeLists.txt'),re.M)
        self.assertEqual(len(tests),130)
        self.assertEqual(len(set(tests)),130)
    def test_portable_suites_cover_two_integrations(self):
        code=source('tools/run_portable_regressions.py')
        self.assertIn('hole_inspection_workflow_portable_tests.cpp',code)
        self.assertIn('simultaneous_proximity_portable_tests.cpp',code)
    def test_staged_build_compiles_dependencies_before_app(self):
        code=source('RUN_SPRINT10_WINDOWS_TARGETED_BUILD.ps1')
        for name in ('cadcore','document_audit_tests','workshop_engineering_v164_tests',
                     'articulated_assembly_scene_v170_tests','[switch]$FullAudit'):
            self.assertIn(name,code)
if __name__=='__main__': unittest.main()

"""Read-only saved-history audit tests; intentionally independent of Qt/OCCT."""
import importlib.util
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location('audit_persistent_references', ROOT/'tools/audit_persistent_references.py')
mod=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(mod)

def fixture(stage="Uniform scale", witness=True):
    p = {"edge_witness_x":5., "edge_witness_y":0., "edge_witness_z":0.} if witness else {}
    return {"units":"mm","parts":[{"id":"body","name":"Stock", "interleaved_history":{
        "steps":[{"id":"scale","kind":"Shape","operation":stage,"parameters":{"factor":2}},
                 {"id":"drill","kind":"Shape","operation":"Face normal pocket",
                  "reference_source":"@base","inputs":["scale"],"parameters":p}]}}]}

class SavedReferenceAuditTests(unittest.TestCase):
    def test_scale_has_valid_static_path_but_needs_kernel(self):
        result=mod.audit(fixture())
        self.assertEqual(result["checked_references"],1)
        self.assertEqual(result["blocked_references"],0)
        self.assertEqual(result["references"][0]["status"],"TRACKED_TRANSFORM")
        self.assertEqual(result["references"][0]["paths"],[["scale"]])
    def test_mirror_ancestry_is_tracked(self):
        self.assertEqual(mod.audit(fixture("Mirror XY"))["blocked_references"],0)
    def test_unsupported_stage_is_blocked(self):
        self.assertEqual(mod.audit(fixture("Unsupported geometry"))["blocked_references"],1)
    def test_missing_witness_is_blocked(self):
        self.assertEqual(mod.audit(fixture(witness=False))["blocked_references"],1)
    def test_missing_source_fails_closed(self):
        obj=fixture();obj["parts"][0]["interleaved_history"]["steps"][1]["reference_source"]="deleted"
        self.assertEqual(mod.audit(obj)["blocked_references"],1)
    def test_direct_stage_no_route_required(self):
        obj=fixture();obj["parts"][0]["interleaved_history"]["steps"][1]["reference_source"]="scale"
        self.assertEqual(mod.audit(obj)["references"][0]["paths"],[[]])
    def test_never_overwrites_earlier_report_on_bad_scene(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);scene=root/'bad.cadscene';scene.write_text('{broken')
            report=root/'result.json';report.write_text('previous valid report')
            proc=subprocess.run([sys.executable,str(ROOT/'tools/audit_persistent_references.py'),
                    str(scene),'--output',str(report)],capture_output=True,text=True)
            self.assertNotEqual(proc.returncode,0)
            self.assertEqual(report.read_text(),'previous valid report')
    def test_valid_cli_and_strict_status(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);scene=root/'sample.cadscene'
            scene.write_text(json.dumps(fixture(stage='unsupported')))
            run=subprocess.run([sys.executable,str(ROOT/'tools/audit_persistent_references.py'),
                  str(scene),'--strict'],capture_output=True,text=True)
            self.assertEqual(run.returncode,1)
            self.assertEqual(json.loads(run.stdout)['blocked_references'],1)
    def test_rejects_duplicate_stages(self):
        obj=fixture();obj['parts'][0]['interleaved_history']['steps'][1]['id']='scale'
        with self.assertRaises(ValueError):mod.audit(obj)

if __name__=='__main__':unittest.main()

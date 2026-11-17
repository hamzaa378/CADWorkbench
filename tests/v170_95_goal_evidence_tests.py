import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location("acceptance_95",ROOT/"tools/verify_95_acceptance.py")
MOD=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MOD)
class EvidenceGateTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        with (ROOT/"docs/V170_ACCEPTANCE_RESULTS.csv").open(newline="",encoding="utf8") as source:
            self.rows=list(csv.DictReader(source))
        self.f=self.root/"checks.csv"
        self._write()
    def _write(self):
        with self.f.open("w",encoding="utf8",newline="") as f:
            writer=csv.DictWriter(f,fieldnames=["id","workstream","critical","scenario","status","evidence_path"])
            writer.writeheader();writer.writerows(self.rows)
    def _run(self):return MOD.verify(self.f,self.root)
    def test_untested_is_not_95(self):
        report=self._run()
        self.assertFalse(report["all_three_at_95"])
        self.assertEqual([s["passed"] for s in report["workstreams"].values()],[0,0,0])
    def test_verified_19_per_stream_gate(self):
        for stream in "PAD":
            ids=[r for r in self.rows if r["workstream"]==stream]
            uncritical=next(x for x in ids if x["critical"]=="false")
            for row in ids:
                if row is uncritical:continue
                row["status"]="PASS";row["evidence_path"]=row["id"]+".log"
                (self.root/row["evidence_path"]).write_text("independently witnessed scenario "+row["id"])
        self._write()
        report=self._run();self.assertTrue(report["all_three_at_95"])
        self.assertEqual([s["passed"] for s in report["workstreams"].values()],[19,19,19])
        self.assertEqual(len(report["evidence"]),57)
    def test_19_but_open_critical_fails(self):
        for row in self.rows:
            if row["id"]=="P01":continue
            if row["workstream"]=="P":
                row["status"]="PASS";row["evidence_path"]=row["id"]+".log"
                (self.root/row["evidence_path"]).write_text("test")
        self._write()
        self.assertFalse(self._run()["workstreams"]["P"]["target_95_verified"])
    def test_false_pass_without_evidence_rejected(self):
        self.rows[0]["status"]="PASS";self._write()
        with self.assertRaisesRegex(ValueError,"without evidence"):self._run()
    def test_invalid_evidence_path_rejected(self):
        self.rows[0]["status"]="PASS";self.rows[0]["evidence_path"]="../outside.txt";self._write()
        with self.assertRaisesRegex(ValueError,"Unsafe"):self._run()
    def test_critical_inconsistency_rejected(self):
        self.rows[0]["critical"]="false";self._write()
        with self.assertRaisesRegex(ValueError,"Critical flag"):self._run()
    def test_duplicate_scenario_rejected(self):
        self.rows[1]["id"]=self.rows[0]["id"];self._write()
        with self.assertRaisesRegex(ValueError,"duplicate"):self._run()
    def test_missing_scenario_rejected(self):
        self.rows.pop();self._write()
        with self.assertRaisesRegex(ValueError,"exactly 60"):self._run()
    def test_unknown_status_rejected(self):
        self.rows[0]["status"]="ALMOST";self._write()
        with self.assertRaisesRegex(ValueError,"Invalid acceptance status"):self._run()
    def test_empty_evidence_rejected(self):
        self.rows[0]["status"]="PASS";self.rows[0]["evidence_path"]="empty.txt"
        (self.root/"empty.txt").touch();self._write()
        with self.assertRaisesRegex(ValueError,"Empty acceptance"):self._run()
if __name__=="__main__":unittest.main()

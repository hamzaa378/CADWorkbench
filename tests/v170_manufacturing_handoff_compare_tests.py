"""Revision comparison tests using synthesized valid handoff fixtures."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from compare_manufacturing_handoffs import compare

HEADER = b"Item,Body ID,Body name,Part number,Revision,Quantity\n"


class CompareHandoffsTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.old=self.root/"old"
        self.new=self.root/"new"
        self.old.mkdir();self.new.mkdir()
        self.make(self.old, "A")
        self.make(self.new, "A")

    def make(self,folder,revision,extra=None,body_id="body-1",step=b"STEP",source=b"SOURCE"):
        files={
            "assembly.step":step,
            "bill_of_materials.csv":b"Item,Part number,Quantity\n1,P-01,1\n",
            "occurrence_parts.csv":HEADER+f"1,{body_id},Bracket,P-01,{revision},1\n".encode(),
            "body_schedule.csv":f"Body ID,Name\n{body_id},Bracket\n".encode(),
            "document_audit.csv":b"Severity,Area,Code,Body ID,Message,Suggested action\n",
            "procurement_exceptions.csv":b"item_number,part_number,revision,procurement_type,quantity,exception_code,next_action\n",
            "feature_inspection_requirements.csv":b"body_id,body_name,feature_id,category,review_status,requirement\n",
            "READ_ME_BEFORE_FABRICATION.txt":b"UNISSUED\n",
            "source_design.cadworkbench":source,
        }
        if extra:files.update(extra)
        records=[]
        for name,data in files.items():
            (folder/name).write_bytes(data)
            records.append(dict(path=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
        manifest={"schema_version":1,"application":"CADWorkbench",
          "status":"UNISSUED_REVIEW_REQUIRED","generated_utc":"2026-11-15T00:00:00Z",
          "body_count":1,"bom_lines":1,"audit_errors":0,"audit_warnings":0,
          "procurement_exceptions":len(files["procurement_exceptions.csv"].splitlines())-1,
          "inspection_requirements":len(files["feature_inspection_requirements.csv"].splitlines())-1,
          "files":records}
        (folder/"handoff_manifest.json").write_text(json.dumps(manifest))

    def test_identical(self):
        report=compare(self.old,self.new)
        self.assertFalse(report["modified_occurrences"])
        self.assertFalse(report["step_changed"])
        self.assertFalse(report["source_changed"])

    def test_revision_change(self):
        self.make(self.new,"B",source=b"SOURCE2")
        report=compare(self.old,self.new)
        self.assertEqual(report["modified_occurrences"][0]["fields"],["Revision"])
        self.assertTrue(report["source_changed"])

    def test_added_removed_by_stable_id(self):
        self.make(self.new,"A",body_id="body-2")
        report=compare(self.old,self.new)
        self.assertEqual(report["removed_body_ids"],["body-1"])
        self.assertEqual(report["added_body_ids"],["body-2"])

    def test_step_change_without_bom_change(self):
        self.make(self.new,"A",step=b"NEWMODEL")
        report=compare(self.old,self.new)
        self.assertTrue(report["step_changed"])
        self.assertEqual(report["modified_occurrences"],[])

    def test_corrupted_previous_rejected(self):
        (self.old/"assembly.step").write_bytes(b"WRONG")
        with self.assertRaises(ValueError): compare(self.old,self.new)

    def test_corrupted_new_rejected(self):
        (self.new/"source_design.cadworkbench").unlink()
        with self.assertRaises(ValueError): compare(self.old,self.new)

    def test_duplicate_body_rejected(self):
        self.make(self.new,"A",extra={"occurrence_parts.csv": HEADER+b"1,body-1,Bracket,P-01,A,1\n2,body-1,Bracket,P-01,A,1\n"})
        with self.assertRaisesRegex(ValueError,"Duplicate"):
            compare(self.old,self.new)


    def test_procurement_added_and_resolved(self):
        problems=b"item_number,part_number,revision,procurement_type,quantity,exception_code,next_action\n1,P-01,A,Purchased,1,SUPPLIER_MISSING,Find supplier\n"
        self.make(self.old,"A",extra={"procurement_exceptions.csv":problems})
        report=compare(self.old,self.new)
        self.assertEqual(report["procurement_issues_resolved"][0]["exception_code"],"SUPPLIER_MISSING")
        self.assertIn("PROCUREMENT_AND_BOM_REVIEW",report["review_triggers"])
        self.make(self.new,"A",extra={"procurement_exceptions.csv":problems})
        report=compare(self.old,self.new)
        self.assertEqual(report["procurement_issues_added"],[])

    def test_changed_inspection_review_status(self):
        before=(b"body_id,body_name,feature_id,category,review_status,requirement\n"
                b"body-1,Bracket,F1,GEOMETRY,SOURCE_GEOMETRY_CURRENT,Inspect radius\n")
        after=(b"body_id,body_name,feature_id,category,review_status,requirement\n"
               b"body-1,Bracket,F1,GEOMETRY,REVIEW_REQUIRED,Inspect radius\n")
        self.make(self.old,"A",extra={"feature_inspection_requirements.csv":before})
        self.make(self.new,"A",extra={"feature_inspection_requirements.csv":after})
        report=compare(self.old,self.new)
        self.assertEqual(report["inspection_geometry_review_required"],1)
        self.assertEqual(len(report["inspection_requirements_added_or_changed"]),1)
        self.assertIn("INSPECTION_PLAN_REVIEW",report["review_triggers"])

    def test_bom_change_detected_even_if_occurrence_text_same(self):
        self.make(self.new,"A",extra={"bill_of_materials.csv":b"Item,Part number,Quantity,Description\n1,P-01,1,New finish\n"})
        report=compare(self.old,self.new)
        self.assertTrue(report["bom_changed"])
        self.assertIn("PROCUREMENT_AND_BOM_REVIEW",report["review_triggers"])

    def test_failing_native_source_comparison(self):
        self.make(self.new,"A",source=b"other")
        report=compare(self.old,self.new)
        self.assertIn("DESIGN_GEOMETRY_REVIEW",report["review_triggers"])

if __name__=="__main__":unittest.main()

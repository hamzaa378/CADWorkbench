"""Engineering action-list generator tests; no Windows GUI or release approval."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from review_manufacturing_handoff import review, render_csv, safe_output_path
from verify_manufacturing_handoff import verify


class ReviewTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.package=self.root/"design"
        self.package.mkdir()
        self.files={
            "assembly.step":b"STEP\n",
            "bill_of_materials.csv":b"Item,Part number,Quantity\n1,P01,1\n",
            "occurrence_parts.csv":b"Item,Body ID,Quantity\n1,O1,1\n",
            "body_schedule.csv":b"Body ID,Name\nO1,Bracket\n",
            "document_audit.csv":b"Severity,Body ID,Code,Suggested action\n",
            "procurement_exceptions.csv":b"item_number,part_number,revision,procurement_type,quantity,exception_code,next_action\n",
            "feature_inspection_requirements.csv":b"body_id,body_name,feature_id,category,review_status,requirement\n",
 "READ_ME_BEFORE_FABRICATION.txt":b"UNISSUED NOT FOR MANUFACTURE\n",
 "source_design.cadworkbench":b"{}",
        }
        self.save()

    def save(self,warnings=0):
        for name, contents in self.files.items():
            (self.package/name).write_bytes(contents)
        manifest={"schema_version":1,"application":"CADWorkbench", "status":"UNISSUED_REVIEW_REQUIRED",
                  "generated_utc":"2026-11-15T00:00:00Z","body_count":1,"bom_lines":1,"audit_errors":0,
                  "audit_warnings":warnings,
                  "procurement_exceptions":len(self.files["procurement_exceptions.csv"].splitlines())-1,
                  "inspection_requirements":len(self.files["feature_inspection_requirements.csv"].splitlines())-1,
                  "files":[dict(path=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
                           for name,data in self.files.items()]}
        (self.package/"handoff_manifest.json").write_text(json.dumps(manifest))

    def test_clean_package_is_still_unissued(self):
        report=review(self.package)
        self.assertEqual(report["open_action_count"],0)
        self.assertFalse(report["release_approved"])
        self.assertIn("UNISSUED",report["status"])

    def test_warnings_procurement_and_inspection_are_actionable(self):
        self.files["document_audit.csv"]+=b"WARNING,O1,LOOSE_MATE,Inspect assembly\n"
        self.files["procurement_exceptions.csv"]+=b"1,P01,A,Purchased,1,SUPPLIER_MISSING,Confirm source\n"
        self.files["feature_inspection_requirements.csv"]+=b"O1,Bracket,F1,DIMENSION,REVIEW_REQUIRED,Inspect diameter\n"
        self.save(warnings=1)
        result=review(self.package)
        self.assertEqual(result["open_action_count"],3)
        self.assertEqual(result["action_counts"],{"MODEL_AUDIT":1,"PROCUREMENT":1,"INSPECTION":1})
        self.assertIn("REVALIDATE_GEOMETRY",{item["code"] for item in result["actions"]})

    def test_current_inspection_still_needs_planning(self):
        self.files["feature_inspection_requirements.csv"]+=b"O1,Bracket,F1,GEOMETRY,SOURCE_GEOMETRY_CURRENT,Inspect\n"
        self.save()
        self.assertEqual(review(self.package)["actions"][0]["code"],"PLAN_INSPECTION")

    def test_corrupted_package_refused(self):
        (self.package/"assembly.step").write_bytes(b"tampered")
        with self.assertRaises(ValueError):review(self.package)

    def test_output_inside_package_refused(self):
        with self.assertRaisesRegex(ValueError,"outside"):
            safe_output_path(self.package/"review.csv",self.package)

    def test_excel_formula_from_untrusted_action_is_inert(self):
        self.files["procurement_exceptions.csv"]+=b"1,P01,A,Purchased,1,SUPPLIER_MISSING,=HYPERLINK(123)\n"
        self.save()
        content=render_csv(review(self.package))
        self.assertIn("'=HYPERLINK",content)
        self.assertNotIn(",=HYPERLINK",content)

    def test_reject_invalid_inspection_reference(self):
        self.files["feature_inspection_requirements.csv"]+=b"unknown,Bracket,F1,GEOMETRY,REVIEW_REQUIRED,Inspect\n"
        self.save()
        with self.assertRaises(ValueError):review(self.package)

    def test_read_only_review_does_not_alter_package(self):
        before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in self.package.iterdir()}
        review(self.package)
        after={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in self.package.iterdir()}
        self.assertEqual(before,after)
        self.assertEqual(len(before),10)

if __name__=="__main__":unittest.main()

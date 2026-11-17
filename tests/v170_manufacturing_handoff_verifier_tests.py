"""End-to-end tamper, structure and CSV-crosscheck tests of the handoff verifier.

These fixtures validate the Python verifier, not the Windows Qt+OCCT exporter.
"""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_manufacturing_handoff import verify


class HandoffVerifierTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name)
        files = {
            "assembly.step": b"ISO-10303-21;\nEND-ISO-10303-21;\n",
            "bill_of_materials.csv": b"Item,Part number,Quantity\n1,P1,1\n",
            "occurrence_parts.csv": b"Item,Body ID,Quantity\n1,O1,1\n",
            "body_schedule.csv": b"Body ID,Name\nO1,Bracket\n",
            "document_audit.csv": b"Severity,Area,Code,Body ID,Message,Suggested action\n",
            "procurement_exceptions.csv": b"item_number,part_number,revision,procurement_type,quantity,exception_code,next_action\n",
            "feature_inspection_requirements.csv": b"body_id,body_name,feature_id,category,review_status,requirement\n",
            "READ_ME_BEFORE_FABRICATION.txt": b"UNISSUED, REVIEW REQUIRED\n",
            "source_design.cadworkbench": b"{\"test\":1}",
        }
        self.files = files
        self.write_package(files)

    def write_package(self, files, **fields):
        for path in self.path.iterdir():
            if path.is_file() or path.is_symlink():
                path.unlink()
            else:
                shutil.rmtree(path)
        inventory=[]
        for name, contents in files.items():
            (self.path / name).write_bytes(contents)
            inventory.append({"path":name,"bytes":len(contents),
                              "sha256":hashlib.sha256(contents).hexdigest()})
        manifest={"schema_version":1,"application":"CADWorkbench",
                  "status":"UNISSUED_REVIEW_REQUIRED","generated_utc":"2026-11-15T00:00:00.000Z",
                  "body_count":1,"bom_lines":1,"audit_errors":0,"audit_warnings":0,
                  "procurement_exceptions":0,"inspection_requirements":0,"files":inventory}
        manifest.update(fields)
        (self.path / "handoff_manifest.json").write_text(json.dumps(manifest),encoding="utf-8")

    def test_valid(self):
        self.assertEqual(verify(self.path)["bom_lines"], 1)

    def test_tampered_step(self):
        (self.path / "assembly.step").write_bytes(b"CHANGED")
        with self.assertRaisesRegex(ValueError,"mismatch"):
            verify(self.path)

    def test_missing_source(self):
        (self.path / "source_design.cadworkbench").unlink()
        with self.assertRaises(ValueError): verify(self.path)

    def test_extra_file(self):
        (self.path / "unknown.exe").write_bytes(b"bogus")
        with self.assertRaises(ValueError): verify(self.path)

    def test_manifest_issued_is_forbidden(self):
        self.write_package(self.files,status="APPROVED")
        with self.assertRaises(ValueError): verify(self.path)

    def test_manifest_errors_forbidden(self):
        self.write_package(self.files,audit_errors=1)
        with self.assertRaises(ValueError): verify(self.path)

    def test_manifest_bom_count_mismatch(self):
        self.write_package(self.files,bom_lines=5)
        with self.assertRaisesRegex(ValueError,"bom_lines"):
            verify(self.path)

    def test_manifest_procurement_count_mismatch(self):
        self.write_package(self.files,procurement_exceptions=3)
        with self.assertRaisesRegex(ValueError,"procurement_exceptions"):
            verify(self.path)

    def test_audit_warnings_mismatch(self):
        files=self.files.copy()
        files["document_audit.csv"]+=b'WARNING,Model,TEST,1,Warning,Review\n'
        self.write_package(files)
        with self.assertRaisesRegex(ValueError,"Audit findings"):
            verify(self.path)

    def test_header_only_inspection_register_is_valid(self):
        self.assertEqual(verify(self.path)["inspection_requirements"],0)

    def test_inspection_count_mismatch(self):
        self.write_package(self.files,inspection_requirements=1)
        with self.assertRaisesRegex(ValueError,"inspection_requirements"):
            verify(self.path)

    def test_duplicate_manifest_file(self):
        self.write_package(self.files)
        mf=self.path / "handoff_manifest.json"
        data=json.loads(mf.read_text())
        data["files"].append(data["files"][0])
        mf.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,"Duplicate"):
            verify(self.path)

    def test_nested_file_path(self):
        self.write_package(self.files)
        mf=self.path / "handoff_manifest.json"
        data=json.loads(mf.read_text())
        data["files"][0]["path"]="../escaped.step"
        mf.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,"Unsafe"):
            verify(self.path)

    def test_reject_symlink(self):
        self.write_package(self.files)
        path=self.path / "external"
        try:
            path.symlink_to(self.path / "assembly.step")
        except (OSError,NotImplementedError):
            self.skipTest("Symlinks not permitted by filesystem")
        with self.assertRaises(ValueError):
            verify(self.path)

    def test_reject_nested_directory(self):
        (self.path / "extra").mkdir()
        with self.assertRaises(ValueError): verify(self.path)

    def test_bom_cell_tamper_detected(self):
        (self.path / "bill_of_materials.csv").write_text("item,part,qty\n1,P1,2\n")
        with self.assertRaises(ValueError): verify(self.path)

    def test_body_schedule_not_linked_to_occurrences(self):
        files=self.files.copy()
        files["body_schedule.csv"]=b"Body ID,Name\nUNKNOWN,Bracket\n"
        self.write_package(files)
        with self.assertRaisesRegex(ValueError,"body IDs"):
            verify(self.path)

    def test_bom_quantity_aggregates_to_occurrences(self):
        files=self.files.copy()
        files["bill_of_materials.csv"]=b"Item,Part number,Quantity\n1,P1,2\n"
        self.write_package(files)
        with self.assertRaisesRegex(ValueError,"BOM quantities"):
            verify(self.path)

    def test_procurement_exception_must_match_bom(self):
        files=self.files.copy()
        files["procurement_exceptions.csv"]=(
            b"item_number,part_number,revision,procurement_type,quantity,exception_code,next_action\n"
            b"1,WRONG,A,Purchased,1,SUPPLIER_MISSING,Find supplier\n")
        self.write_package(files,procurement_exceptions=1)
        with self.assertRaisesRegex(ValueError,"Procurement exception"):
            verify(self.path)

    def test_inspection_requirement_body_must_exist(self):
        files=self.files.copy()
        files["feature_inspection_requirements.csv"]=(
            b"body_id,body_name,feature_id,category,review_status,requirement\n"
            b"UNKNOWN,Bracket,F1,GD_T,REVIEW_REQUIRED,Check datum\n")
        self.write_package(files,inspection_requirements=1)
        with self.assertRaisesRegex(ValueError,"Inspection requirement"):
            verify(self.path)

    def test_valid_cross_linked_review_registers(self):
        files=self.files.copy()
        files["procurement_exceptions.csv"]=(
            b"item_number,part_number,revision,procurement_type,quantity,exception_code,next_action\n"
            b"1,P1,A,Purchased,1,SUPPLIER_MISSING,Find supplier\n")
        files["feature_inspection_requirements.csv"]=(
            b"body_id,body_name,feature_id,category,review_status,requirement\n"
            b"O1,Bracket,F1,GD_T,REVIEW_REQUIRED,Check datum\n")
        self.write_package(files,procurement_exceptions=1,inspection_requirements=1)
        self.assertEqual(verify(self.path)["procurement_exceptions"],1)

    def test_invalid_occurrence_quantity(self):
        files=self.files.copy()
        files["occurrence_parts.csv"]=b"Item,Body ID,Quantity\n1,O1,2\n"
        self.write_package(files)
        with self.assertRaisesRegex(ValueError,"exactly one"):
            verify(self.path)

    def test_invalid_inspection_review_status(self):
        files=self.files.copy()
        files["feature_inspection_requirements.csv"]=(
            b"body_id,body_name,feature_id,category,review_status,requirement\n"
            b"O1,Bracket,F1,GD_T,APPROVED,Check datum\n")
        self.write_package(files,inspection_requirements=1)
        with self.assertRaisesRegex(ValueError,"review status"):
            verify(self.path)

if __name__=='__main__':unittest.main()

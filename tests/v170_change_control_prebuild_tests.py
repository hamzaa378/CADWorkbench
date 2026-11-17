"""Source-contract and genuine handoff fixtures for the prebuild/change-control increment."""
from pathlib import Path
import hashlib
import json
import shutil
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from verify_manufacturing_handoff import verify
from assess_change_control import assess
from preflight_major_build import check


class ChangeControlTests(unittest.TestCase):
    def make_handoff(self, folder: Path, *, occurrence_item='1', bom_quantity='1', audit_severity='WARNING',
                     step=b'STEP1', source=b'CAD1', status='REVIEW_REQUIRED'):
        folder.mkdir()
        files={
            'assembly.step':step,
            'bill_of_materials.csv':f'Item,Part number,Quantity\n1,P01,{bom_quantity}\n'.encode(),
            'occurrence_parts.csv':f'Item,Body ID,Quantity\n{occurrence_item},O1,1\n'.encode(),
            'body_schedule.csv':b'Body ID,Name\nO1,Bracket\n',
            'document_audit.csv':f'Severity,Body ID,Code,Suggested action\n{audit_severity},O1,SOURCE_REVIEW,Check\n'.encode(),
            'procurement_exceptions.csv':b'item_number,part_number,revision,procurement_type,quantity,exception_code,next_action\n',
            'feature_inspection_requirements.csv':f'body_id,body_name,feature_id,category,review_status,requirement\nO1,Bracket,F1,DIMENSION,{status},Check diameter\n'.encode(),
            'READ_ME_BEFORE_FABRICATION.txt':b'UNISSUED REVIEW ONLY',
            'source_design.cadworkbench':source,
        }
        for name,data in files.items():(folder/name).write_bytes(data)
        manifest={'schema_version':1,'application':'CADWorkbench','status':'UNISSUED_REVIEW_REQUIRED',
                  'generated_utc':'2026-11-16T00:00:00Z','body_count':1,'bom_lines':1,'audit_errors':0,
                  'audit_warnings':int(audit_severity=='WARNING'), 'procurement_exceptions':0,
                  'inspection_requirements':1,
                  'files':[{'path':name,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}
                           for name,data in sorted(files.items())]}
        (folder/'handoff_manifest.json').write_text(json.dumps(manifest))
        return folder

    def test_actual_corrected_native_source(self):
        contents=(ROOT/'tests/associative_features_v159_tests.cpp').read_text()
        self.assertIn('int witnessedFaceIndex=0,topIndex=0;',contents)
        self.assertEqual(contents.count('int faceIndex=0'),1)

    def test_bom_group_mismatch_even_with_matching_totals(self):
        with tempfile.TemporaryDirectory() as d:
            p=self.make_handoff(Path(d)/'bad',occurrence_item='2')
            with self.assertRaisesRegex(ValueError,'Occurrence item groups'):
                verify(p)

    def test_unknown_audit_severity_is_not_ignored(self):
        with tempfile.TemporaryDirectory() as d:
            p=self.make_handoff(Path(d)/'bad',audit_severity='UNREVIEWED')
            with self.assertRaisesRegex(ValueError,'audit severity'):
                verify(p)

    def test_review_matrix_marks_evidence_unreviewed(self):
        with tempfile.TemporaryDirectory() as d:
            old=self.make_handoff(Path(d)/'old',step=b'STEP1',source=b'CAD1')
            new=self.make_handoff(Path(d)/'new',step=b'STEP2',source=b'CAD2')
            report=assess(old,new)
            self.assertFalse(report['design_approval'])
            self.assertIn('DESIGN_GEOMETRY_REVIEW',report['review_triggers'])
            self.assertTrue(all(x['state']=='NOT_REVIEWED' for x in report['required_evidence']))
            self.assertEqual(len(report['inspection_items_requiring_review']),1)

    def test_cad_step_divergence_is_flagged(self):
        with tempfile.TemporaryDirectory() as d:
            old=self.make_handoff(Path(d)/'old',step=b'STEP1',source=b'CAD1')
            new=self.make_handoff(Path(d)/'new',step=b'STEP1',source=b'CAD2')
            result=assess(old,new)
            self.assertIn('CAD_STEP_DIVERGENCE_REVIEW',result['review_triggers'])
            self.assertIn('cad_step_equivalence',[x['topic'] for x in result['required_evidence']])

    def test_preflight_rejects_known_old_redeclaration(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'tests').mkdir()
            (root/'tests/associative_features_v159_tests.cpp').write_text('int faceIndex=0;\nint faceIndex=0,topIndex=0;')
            self.assertTrue(any('C2374' in x for x in check(root)))

if __name__=='__main__':unittest.main()

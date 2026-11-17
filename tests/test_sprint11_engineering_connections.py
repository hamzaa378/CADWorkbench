"""Real signed-package, independent metrology and drawing reconciliation tests."""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
from pathlib import Path
from unittest.mock import patch
import random
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import v170_machining_handoff_v2_tests as fixtures
from review_measured_holes import review as review_holes, render_csv as measured_csv, main as review_cli
from reconcile_drawing_handoff import reconcile
from engineering_readiness_workflow import evaluate
from verify_unissued_drawings import FIELDS


class ConnectedEngineeringTests(unittest.TestCase):
    def setUp(self):
        self.fixture=fixtures.MachiningHandoffV2Tests('test_v2_verified')
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root=self.fixture.base
        self.handoff=self.fixture.new
        self.handoff_template=self.fixture.template()
        self.fixture.write(self.handoff,extra={'hole_position_inspection_TEMPLATE.csv':self.handoff_template})
        self.measured=self.root/'independent_measurements.csv'
        self.fill()
        self.drawings=self.root/'drawing_previews'
        self.drawings.mkdir()
        self.make_drawings()

    def fill(self,**changes):
        rows=list(csv.DictReader(io.StringIO(self.handoff_template.decode())))
        for row in rows:
            row.update(measured_datum_x_mm='5.02',measured_datum_y_mm='6',
                       measured_projected_x_mm='5.02',measured_projected_y_mm='6',
                       projected_height_mm='0',actual_diameter_mm='4.05',
                       minimum_diameter_mm='4',maximum_diameter_mm='4.2',
                       diametral_position_tolerance_mm='0.1',material_condition='RFS',
                       maximum_pin_diameter_mm='3.8',inspection_status='MEASURED')
            row.update(changes)
        with self.measured.open('w',newline='',encoding='utf-8') as output:
            writer=csv.DictWriter(output,fieldnames=rows[0].keys())
            writer.writeheader();writer.writerows(rows)
        return rows

    def make_drawings(self,*,owner='O1',token='T1',drawing='SHEET-01'):
        name='drawing_001_UNISSUED.svg'
        payload=b'<svg xmlns="http://www.w3.org/2000/svg"/>'
        (self.drawings/name).write_bytes(payload)
        with (self.drawings/'drawing_index.csv').open('w',newline='',encoding='utf-8') as output:
            writer=csv.writer(output)
            writer.writerow(FIELDS)
            writer.writerow([name,owner,drawing,'UNISSUED REVIEW REQUIRED','',token,
                            hashlib.sha256(payload).hexdigest(),''])

    def test_independent_measurement_rejoins_exact_signed_cad(self):
        r=review_holes(self.handoff,self.measured)
        self.assertEqual((r['checked_holes'],r['within_entered_limits']),(1,1))
        self.assertAlmostEqual(r['holes'][0]['measured_diametral_position_mm'],.04)
        self.assertFalse(r['release_approved'])
        self.assertTrue(r['all_within_entered_limits'])
        self.assertIn('geometry_token',r['holes'][0])

    def test_unissued_template_can_never_act_as_measured_evidence(self):
        self.measured.write_bytes(self.handoff_template)
        with self.assertRaisesRegex(ValueError,'status must be MEASURED'):
            review_holes(self.handoff,self.measured)

    def test_renumbered_or_missing_hole_rejected(self):
        self.fill(hole_number='7')
        with self.assertRaisesRegex(ValueError,'missing|stale|changed'):
            review_holes(self.handoff,self.measured)

    def test_stale_geometry_revision_rejected(self):
        self.fill(geometry_token='T2')
        with self.assertRaisesRegex(ValueError,'stale'):
            review_holes(self.handoff,self.measured)

    def test_nominal_coordinate_or_diameter_edited_rejected(self):
        for name in ('nominal_x_mm','nominal_y_mm','nominal_diameter_mm'):
            with self.subTest(name=name):
                self.fill(**{name:'999'})
                with self.assertRaisesRegex(ValueError,'changed'):
                    review_holes(self.handoff,self.measured)

    def test_inspection_status_cannot_claim_signed_approval(self):
        self.fill(inspection_status='ACCEPTED')
        with self.assertRaisesRegex(ValueError,'MEASURED'):
            review_holes(self.handoff,self.measured)

    def test_csv_column_injection_rejected(self):
        content=self.measured.read_text()
        lines=content.splitlines()
        self.measured.write_text(lines[0]+',release_approved\n'+lines[1]+',true\n')
        with self.assertRaisesRegex(ValueError,'precisely'):
            review_holes(self.handoff,self.measured)

    def test_nan_inf_empty_bounds_and_height_rejected(self):
        for field,value in [('actual_diameter_mm','nan'),('maximum_pin_diameter_mm','inf'),
                            ('measured_datum_x_mm',''),('maximum_diameter_mm','-1'),
                            ('projected_height_mm','-0.01'),('measured_projected_y_mm','100')]:
            with self.subTest(field=field):
                self.fill(**{field:value})
                with self.assertRaises(ValueError):review_holes(self.handoff,self.measured)

    def test_rfs_and_mmc_bonus_differ(self):
        self.fill(measured_datum_x_mm='5.06',measured_projected_x_mm='5.06',
                  material_condition='RFS')
        rfs=review_holes(self.handoff,self.measured)
        self.assertFalse(rfs['all_within_entered_limits'])
        self.fill(measured_datum_x_mm='5.06',measured_projected_x_mm='5.06',
                  material_condition='MMC')
        mmc=review_holes(self.handoff,self.measured)
        self.assertTrue(mmc['all_within_entered_limits'])
        self.assertAlmostEqual(mmc['holes'][0]['material_bonus_mm'],.05)

    def test_pin_fit_can_fail_while_size_and_position_pass(self):
        self.fill(maximum_pin_diameter_mm='4.04')
        r=review_holes(self.handoff,self.measured)
        self.assertEqual(r['rejected_pin_fit'],1)
        self.assertTrue(r['holes'][0]['size_within_entered_limits'])

    def test_measured_data_must_be_external_to_signed_handoff(self):
        with self.assertRaises(ValueError):review_holes(self.handoff,self.handoff/'hole_position_inspection_TEMPLATE.csv')

    def test_signature_tampering_invalidates_independent_results(self):
        (self.handoff/'axial_hole_locations.csv').write_text('tampered')
        with self.assertRaisesRegex(ValueError,'hash|size'):
            review_holes(self.handoff,self.measured)

    def test_legacy_handoff_missing_template_rejected(self):
        self.fixture.write(self.handoff)
        with self.assertRaisesRegex(ValueError,'signed nominal inspection template'):
            review_holes(self.handoff,self.measured)

    def test_drawing_and_handoff_revisions_reconcile(self):
        result=reconcile(self.handoff,self.drawings)
        self.assertEqual(result['sheet_count'],1)
        self.assertEqual(result['review_issue_count'],0)
        self.assertFalse(result['release_approved'])

    def test_drawing_reference_token_mismatch_requires_review(self):
        self.make_drawings(token='geometry-batch-digest')
        result=reconcile(self.handoff,self.drawings)
        self.assertIn('TOKEN_DIFFERENCE_OR_COMPOSITE_SHEET_REVIEW',
                      [issue['code'] for issue in result['issues']])

    def test_drawing_unknown_body_requires_review(self):
        self.make_drawings(owner='unrelated')
        result=reconcile(self.handoff,self.drawings)
        self.assertIn('DRAWING_OWNER_NOT_IN_CAD_HANDOFF',
                      [issue['code'] for issue in result['issues']])

    def test_drawing_hash_tamper_rejected(self):
        (self.drawings/'drawing_001_UNISSUED.svg').write_text('modified')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):
            reconcile(self.handoff,self.drawings)

    def test_drawing_index_missing_token_requires_review(self):
        self.make_drawings(token='')
        result=reconcile(self.handoff,self.drawings)
        self.assertIn('DRAWING_SOURCE_TOKEN_MISSING',
                      [issue['code'] for issue in result['issues']])

    def test_inspection_result_csv_never_encodes_approval(self):
        result=review_holes(self.handoff,self.measured)
        body=measured_csv(result)
        self.assertNotIn('release_approved',body)
        self.assertIn('within_all_entered_limits',body)

    def test_report_digest_changes_with_measurement(self):
        a=review_holes(self.handoff,self.measured)
        self.fill(measured_datum_x_mm='5.04',measured_projected_x_mm='5.04')
        b=review_holes(self.handoff,self.measured)
        self.assertNotEqual(a['independent_measurement_sha256'],b['independent_measurement_sha256'])

    def test_end_to_end_command_writes_two_external_reports(self):
        output_json=self.root/'review.json';output_csv=self.root/'review.csv'
        with patch.object(sys,'argv',['review_measured_holes.py',str(self.handoff),
                                     str(self.measured),'--json',str(output_json),
                                     '--csv',str(output_csv)]):
            self.assertEqual(review_cli(),0)
        report=json.loads(output_json.read_text())
        self.assertFalse(report['release_approved'])
        self.assertEqual(report['checked_holes'],1)
        self.assertIn('within_all_entered_limits',output_csv.read_text())
        self.assertFalse((self.handoff/'review.json').exists())

    def test_invalid_measurements_do_not_leave_partial_reports(self):
        self.fill(inspection_status='APPROVED')
        output_json=self.root/'invalid.json';output_csv=self.root/'invalid.csv'
        with patch.object(sys,'argv',['review_measured_holes.py',str(self.handoff),
                                     str(self.measured),'--json',str(output_json),
                                     '--csv',str(output_csv)]):
            self.assertEqual(review_cli(),1)
        self.assertFalse(output_json.exists() or output_csv.exists())

    def test_aggregated_reviewer_without_evidence_keeps_open_actions(self):
        result=evaluate(self.handoff)
        codes={item['code'] for item in result['actions']}
        self.assertTrue({'INDEPENDENT_MEASUREMENTS_NOT_SUPPLIED',
                         'UNISSUED_DRAWINGS_NOT_SUPPLIED',
                         'MANUFACTURING_RELEASE_REVIEW_REQUIRED'} <= codes)
        self.assertFalse(result['release_approved'])

    def test_aggregated_reviewer_with_all_evidence_still_requires_signoff(self):
        result=evaluate(self.handoff,measured_csv=self.measured,drawing_folder=self.drawings)
        self.assertEqual(result['inspection_summary']['checked_holes'],1)
        self.assertEqual(result['drawing_summary']['sheet_count'],1)
        codes={item['code'] for item in result['actions']}
        self.assertIn('MEASUREMENT_EVIDENCE_SIGNOFF_REQUIRED',codes)
        self.assertIn('DRAWING_DIMENSION_GDT_REVIEW_REQUIRED',codes)
        self.assertFalse(result['release_approved'])

    def test_aggregated_reviewer_flags_out_of_tolerance_hole(self):
        self.fill(measured_datum_x_mm='5.4',measured_projected_x_mm='5.4')
        result=evaluate(self.handoff,measured_csv=self.measured,drawing_folder=self.drawings)
        self.assertIn('MEASURED_LIMIT_NOT_MET',{item['code'] for item in result['actions']})

    def test_aggregated_reviewer_rejects_bad_geometry_revision(self):
        self.fill(geometry_token='obsolete')
        with self.assertRaisesRegex(ValueError,'stale'):
            evaluate(self.handoff,measured_csv=self.measured,drawing_folder=self.drawings)

    def test_geometry_csv_with_no_holes_cannot_claim_review(self):
        # Signed source may be a valid body but has no inspected axial holes.
        self.fixture.write(self.handoff,holes=fixtures.HOLE_HEADER,
                           covered=fixtures.COVER_HEADER+'O1,T1,INSPECTED_NO_AXIAL_HOLES,0,0,No axial holes\n',
                           extra={'hole_position_inspection_TEMPLATE.csv':self.handoff_template.splitlines()[0]+b'\n'})
        with self.assertRaisesRegex(ValueError,'1-1000'):
            review_holes(self.handoff,self.measured)

if __name__=='__main__':unittest.main()

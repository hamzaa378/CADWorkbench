"""Sprint 12 genuine signed-handoff, drawing, metrology and output-boundary tests."""
from __future__ import annotations
import csv
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import v170_machining_handoff_v2_tests as fixtures
from verify_manufacturing_handoff import verify
from verify_unissued_drawings import verify as verify_drawings,FIELDS
from engineering_revision_impact import impact
from drawing_revision_diff import compare_drawings
from engineering_readiness_workflow import evaluate
from engineering_evidence_bundle import build_bundle,verify_bundle

class RevisionWorkflows(unittest.TestCase):
    def setUp(self):
        self.fixture=fixtures.MachiningHandoffV2Tests('test_v2_verified')
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root=self.fixture.base
        self.old,self.new=self.fixture.old,self.fixture.new
        self.write_new()
        self.fixture.write(self.old,extra={'hole_position_inspection_TEMPLATE.csv':self.fixture.template()})
        self.old_draw=self.root/'old_draw';self.old_draw.mkdir()
        self.new_draw=self.root/'new_draw';self.new_draw.mkdir()
        self.draw(self.old_draw)
        self.draw(self.new_draw)
        self.measured=self.root/'measurements.csv'
        self.measure()

    def write_new(self,*,token='T1',x='5.0',diam='4.0'):
        hole=fixtures.HOLE_HEADER+f'O1,{token},1,{x},6.0,{diam},0.0,20.0,YES\n'
        coverage=fixtures.COVER_HEADER+f'O1,{token},INSPECTED_AXIAL_ONLY,1,0,Axial only\n'
        self.fixture.write(self.new,covered=coverage,holes=hole,
            extra={'hole_position_inspection_TEMPLATE.csv':self.fixture.template(
                geometry_token=token,nominal_x_mm=x,nominal_diameter_mm=diam)})
        self.assertEqual(verify(self.new)['schema_version'],2)

    def draw(self,folder:Path,*,number='S01',token='T1',content=b'<svg/>'):
        for entry in folder.iterdir():entry.unlink()
        name='drawing_001_UNISSUED.svg'
        (folder/name).write_bytes(content)
        with (folder/'drawing_index.csv').open('w',newline='',encoding='utf-8') as f:
            w=csv.writer(f);w.writerow(FIELDS)
            w.writerow([name,'O1',number,'UNISSUED REVIEW REQUIRED','',token,
                        hashlib.sha256(content).hexdigest(),''])

    def measure(self,*,token='T1',x='5.0',diam='4.0'):
        header=self.fixture.template(geometry_token=token,nominal_x_mm=x,
                                     nominal_diameter_mm=diam).decode()
        row=list(csv.DictReader(io.StringIO(header)))[0]
        row.update(measured_datum_x_mm=x,measured_datum_y_mm='6',
            measured_projected_x_mm=x,measured_projected_y_mm='6',
            projected_height_mm='0',actual_diameter_mm=diam,
            minimum_diameter_mm=diam,maximum_diameter_mm=str(float(diam)+0.2),
            diametral_position_tolerance_mm='0.1',material_condition='RFS',
            maximum_pin_diameter_mm='0',inspection_status='MEASURED')
        with self.measured.open('w',encoding='utf-8',newline='') as f:
            writer=csv.DictWriter(f,fieldnames=list(row));writer.writeheader();writer.writerow(row)

    def test_reference_match_not_approval(self):
        r=impact(self.old,self.new)
        self.assertEqual(len(r['hole_changes']),1)
        self.assertEqual(r['hole_changes'][0]['status'],'REFERENCE_MATCH')
        self.assertEqual(r['fresh_inspection_count'],0)
        self.assertFalse(r['release_approved'])

    def test_change_nominal_requires_reinspection(self):
        self.write_new(x='5.5')
        r=impact(self.old,self.new)
        self.assertEqual(r['hole_changes'][0]['changed_fields'],['x_mm'])
        self.assertEqual(r['fresh_inspection_count'],1)
        self.assertIn('HOLE_CHANGED',{a['code'] for a in r['actions']})
        self.assertIn('MACHINING_AXIAL_REVIEW',r['review_triggers'])

    def test_geometry_token_only_invalidates(self):
        self.write_new(token='T2')
        r=impact(self.old,self.new)
        self.assertIn('geometry_token',r['hole_changes'][0]['changed_fields'])
        self.assertTrue(r['hole_changes'][0]['fresh_inspection_required'])

    def test_changed_hole_diameter_requires_reinspection(self):
        self.write_new(diam='4.5')
        self.assertEqual(impact(self.old,self.new)['fresh_inspection_count'],1)

    def test_previous_measured_evidence_remains_historical(self):
        r=impact(self.old,self.new,previous_measurements=self.measured)
        prior=r['prior_measurement_evidence']
        self.assertEqual(prior['prior_holes_checked'],1)
        self.assertEqual(prior['automatically_carried_forward'],0)

    def test_spoofed_previous_measurement_rejected(self):
        self.measure(token='T3')
        with self.assertRaisesRegex(ValueError,'stale'):
            impact(self.old,self.new,previous_measurements=self.measured)

    def test_v1_to_v2_migration_is_conservative(self):
        self.fixture.write(self.old,schema=1)
        r=impact(self.old,self.new)
        self.assertEqual(r['hole_changes'][0]['status'],'ADDED')
        self.assertEqual(r['fresh_inspection_count'],1)

    def test_drawing_reference_match_unapproved(self):
        r=compare_drawings(self.old_draw,self.new_draw)
        self.assertEqual(r['changed_count'],0)
        self.assertFalse(r['release_approved'])

    def test_drawing_token_change_flags_review(self):
        self.draw(self.new_draw,token='T2')
        r=compare_drawings(self.old_draw,self.new_draw)
        self.assertEqual(r['changed_count'],1)
        self.assertIn('current_geometry_token',r['sheets'][0]['changed_fields'])

    def test_drawing_pixels_change_flags_review_even_if_token_same(self):
        self.draw(self.new_draw,content=b'<svg><path/></svg>')
        r=compare_drawings(self.old_draw,self.new_draw)
        self.assertIn('svg_sha256',r['sheets'][0]['changed_fields'])

    def test_drawing_number_change_flags_review(self):
        self.draw(self.new_draw,number='S02')
        self.assertEqual(compare_drawings(self.old_draw,self.new_draw)['changed_count'],1)

    def test_drawing_pair_required(self):
        with self.assertRaisesRegex(ValueError,'both previous and current'):
            impact(self.old,self.new,previous_drawings=self.old_draw)

    def test_drawing_tamper_blocks_impact(self):
        (self.new_draw/'drawing_001_UNISSUED.svg').write_bytes(b'changed after hashing')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):
            impact(self.old,self.new,previous_drawings=self.old_draw,current_drawings=self.new_draw)

    def test_revision_impacts_readiness_actions(self):
        self.write_new(token='T2')
        r=evaluate(self.new,previous_handoff=self.old,
                   previous_drawings=self.old_draw,drawing_folder=self.new_draw)
        self.assertGreater(r['revision_impact_summary']['open_action_count'],0)
        self.assertIn('HOLE_CHANGED',{a['code'] for a in r['actions']})
        self.assertFalse(r['release_approved'])

    def test_invalid_prior_evidence_without_prior_handoff(self):
        with self.assertRaisesRegex(ValueError,'requires a previous'):
            evaluate(self.new,previous_measurements=self.measured)

    def test_missing_drawing_evidence_generates_action(self):
        self.draw(self.new_draw,token='T2')
        r=evaluate(self.new,drawing_folder=self.new_draw)
        self.assertIn('TOKEN_DIFFERENCE_OR_COMPOSITE_SHEET_REVIEW',
                      {a['code'] for a in r['actions']})

    def test_end_to_end_evidence_packet(self):
        destination=self.root/'issue-001'
        m=build_bundle(self.new,destination,previous=self.old,drawings=self.new_draw,
                       previous_drawings=self.old_draw,measured=self.measured,
                       previous_measurements=self.measured)
        self.assertEqual(m['status'],'UNISSUED_REVIEW_EVIDENCE_ONLY')
        self.assertFalse(m['release_approved'])
        self.assertEqual(len(m['files']),6)
        self.assertEqual(verify_bundle(destination)['open_action_count'],m['open_action_count'])
        self.assertFalse(json.loads((destination/'readiness.json').read_text())['release_approved'])

    def test_publish_is_atomic_on_bad_measurement(self):
        self.measure(token='WRONG')
        target=self.root/'bad-release'
        with self.assertRaises(ValueError):build_bundle(self.new,target,measured=self.measured)
        self.assertFalse(target.exists())

    def test_existing_evidence_is_never_overwritten(self):
        target=self.root/'existing'
        build_bundle(self.new,target)
        orig=(target/'evidence_manifest.json').read_bytes()
        with self.assertRaisesRegex(ValueError,'NEW directory'):
            build_bundle(self.new,target)
        self.assertEqual(orig,(target/'evidence_manifest.json').read_bytes())

    def test_evidence_tamper_rejected(self):
        target=self.root/'tamper'
        build_bundle(self.new,target)
        (target/'actions.csv').write_text('spoofed')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):
            verify_bundle(target)

    def test_evidence_unknown_attachment_rejected(self):
        target=self.root/'tamper'
        build_bundle(self.new,target)
        (target/'APPROVED.txt').write_text('false claim')
        with self.assertRaisesRegex(ValueError,'Extra or missing'):
            verify_bundle(target)

    def test_source_handoff_untouched(self):
        before=(self.new/'handoff_manifest.json').read_bytes()
        build_bundle(self.new,self.root/'report')
        self.assertEqual(before,(self.new/'handoff_manifest.json').read_bytes())

    def test_never_write_inside_source_directory(self):
        with self.assertRaisesRegex(ValueError,'external'):
            build_bundle(self.new,self.new/'reports')

    def test_evidence_missing_input_rejected(self):
        with self.assertRaises(ValueError):build_bundle(self.root/'does_not_exist',self.root/'report')
        self.assertFalse((self.root/'report').exists())

    def test_hardened_svg_checksum_verification(self):
        self.assertEqual(verify_drawings(self.new_draw),1)
        (self.new_draw/'drawing_001_UNISSUED.svg').write_bytes(b'wrong')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):verify_drawings(self.new_draw)

    def test_hardened_drawing_index_rejects_extra_file(self):
        (self.new_draw/'rogue.txt').write_text('not expected')
        with self.assertRaisesRegex(ValueError,'Unexpected/missing'):verify_drawings(self.new_draw)

    def test_bundle_can_be_verified_by_public_cli(self):
        import subprocess
        dest=self.root/'cli-evidence'
        build_bundle(self.new,dest)
        p=subprocess.run([sys.executable,str(ROOT/'tools/verify_engineering_evidence.py'),
                          str(dest)],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertIn('files_verified',p.stdout)

    def test_bundled_report_is_cross_revision(self):
        self.write_new(token='T2')
        dest=self.root/'cross-revision'
        build_bundle(self.new,dest,previous=self.old)
        impact_report=json.loads((dest/'revision_impact.json').read_text())
        self.assertEqual(impact_report['fresh_inspection_count'],1)
        self.assertIn('HOLE_CHANGED',(dest/'actions.csv').read_text())

    def test_measured_results_are_bound_to_current_revision(self):
        self.write_new(token='T2')
        with self.assertRaisesRegex(ValueError,'stale'):
            build_bundle(self.new,self.root/'no-invalid-current',measured=self.measured)
        self.measure(token='T2')
        result=build_bundle(self.new,self.root/'valid-current',measured=self.measured)
        self.assertFalse(result['release_approved'])

    def test_manifest_cannot_be_relabelled_approved(self):
        dest=self.root/'spoof'
        build_bundle(self.new,dest)
        mf=dest/'evidence_manifest.json'
        data=json.loads(mf.read_text());data['release_approved']=True
        mf.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'Invalid or approved'):
            verify_bundle(dest)

    def test_historical_handoff_hash_tampering_is_detected(self):
        (self.old/'assembly.step').write_bytes(b'modified')
        with self.assertRaisesRegex(ValueError,'hash|size'):
            impact(self.old,self.new)

    def test_drawing_folder_symlink_rejected(self):
        link=self.root/'draw-link'
        try:link.symlink_to(self.new_draw,target_is_directory=True)
        except OSError:self.skipTest('Symlinks unavailable on this platform')
        with self.assertRaisesRegex(ValueError,'Missing|symlink'):
            verify_drawings(link)

    def test_native_hole_revision_api_registered(self):
        self.assertIn('src/core/HoleRevisionImpact.h',(ROOT/'CMakeLists.txt').read_text())
        self.assertIn('holeRevisionWorkflows()',
                      (ROOT/'tests/workshop_engineering_v164_tests.cpp').read_text())
        self.assertIn('hole_revision_impact_portable_tests.cpp',
                      (ROOT/'tools/run_portable_regressions.py').read_text())
        self.assertIn('src/core/DrawingRevisionImpact.h',
                      (ROOT/'CMakeLists.txt').read_text())
        self.assertIn('compareDrawingRevisions',
                      (ROOT/'tests/document_audit_tests.cpp').read_text())
        self.assertIn('drawing_revision_impact_portable_tests.cpp',
                      (ROOT/'tools/run_portable_regressions.py').read_text())

if __name__=='__main__':unittest.main()

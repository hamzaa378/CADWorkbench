"""Synthetic v2 integration tests, backwards compatibility and tamper detection.

The native CAD exporter must still pass Windows Qt/OpenCascade compilation.
"""
from pathlib import Path
import hashlib
import json
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from verify_manufacturing_handoff import verify
from compare_manufacturing_handoffs import compare
from assess_change_control import assess

COVER_HEADER = 'body_id,geometry_token,status,hole_count,excluded_cylinders,reason\n'
HOLE_HEADER = 'body_id,geometry_token,hole_number,x_mm,y_mm,diameter_mm,z_min_mm,z_max_mm,through_envelope\n'


class MachiningHandoffV2Tests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base=Path(self.tmp.name)
        self.old=self.base/'old';self.old.mkdir()
        self.new=self.base/'new';self.new.mkdir()
        self.write(self.old)
        self.write(self.new)

    def write(self, folder, *, covered=None, holes=None, extra=None, schema=2):
        covered = covered if covered is not None else COVER_HEADER + 'O1,T1,INSPECTED_AXIAL_ONLY,1,0,Axial only\n'
        holes = holes if holes is not None else HOLE_HEADER + 'O1,T1,1,5.0,6.0,4.0,0.0,20.0,YES\n'
        files={
            'assembly.step':b'STEP',
            'bill_of_materials.csv':b'Item,Part number,Quantity\n1,P01,1\n',
            'occurrence_parts.csv':b'Item,Body ID,Quantity\n1,O1,1\n',
            'body_schedule.csv':b'Body ID,Name\nO1,Bracket\n',
            'document_audit.csv':b'Severity,Area,Code,Body ID,Message,Suggested action\n',
            'procurement_exceptions.csv':b'item_number,part_number,revision,procurement_type,quantity,exception_code,next_action\n',
            'feature_inspection_requirements.csv':b'body_id,body_name,feature_id,category,review_status,requirement\n',
            'READ_ME_BEFORE_FABRICATION.txt':b'UNISSUED REVIEW REQUIRED',
            'source_design.cadworkbench':b'CAD',
        }
        if schema == 2:
            files['machining_coverage.csv']=covered.encode()
            files['axial_hole_locations.csv']=holes.encode()
        if extra:
            files.update(extra)
        for entry in folder.iterdir():
            entry.unlink()
        for name, data in files.items():
            (folder/name).write_bytes(data)
        records=[dict(path=name,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data))
                 for name,data in sorted(files.items())]
        manifest=dict(schema_version=schema,application='CADWorkbench',status='UNISSUED_REVIEW_REQUIRED',
                      generated_utc='2026-11-16T00:00:00.000Z',body_count=1,bom_lines=1,
                      audit_errors=0,audit_warnings=0,procurement_exceptions=0,
                      inspection_requirements=0,files=records)
        if schema==2:
            manifest['machining_holes']=len(holes.strip().splitlines())-1
            manifest['machining_review_required']=int('NOT_EVALUATED' in covered or 'PARTIAL_CYLINDERS_EXCLUDED' in covered)
        (folder/'handoff_manifest.json').write_text(json.dumps(manifest))
        return folder

    def test_v2_verified(self):
        m=verify(self.new)
        self.assertEqual(m['machining_holes'],1)
        self.assertEqual(m['machining_review_required'],0)

    def test_v1_legacy_handoff_still_verifies(self):
        self.write(self.old,schema=1)
        self.assertEqual(verify(self.old)['schema_version'],1)

    def test_v1_v2_migration_triggers_review(self):
        self.write(self.old,schema=1)
        result=compare(self.old,self.new)
        self.assertTrue(result['machining_review_changed'])
        self.assertIn('MACHINING_AXIAL_REVIEW',result['review_triggers'])
        self.assertFalse(assess(self.old,self.new)['design_approval'])

    def test_changed_diameter_flags_inspection_and_fixture_review(self):
        self.write(self.new,holes=HOLE_HEADER+'O1,T1,1,5.0,6.0,5.0,0.0,20.0,YES\n')
        result=compare(self.old,self.new)
        self.assertEqual(len(result['axial_holes_added_or_changed']),1)
        assessment=assess(self.old,self.new)
        topics={x['topic'] for x in assessment['required_evidence']}
        self.assertTrue({'hole_location_inspection','fixture_exit_review','tool_access_review'}<=topics)

    def test_missing_coverage_body_rejected(self):
        self.write(self.new,covered=COVER_HEADER)
        with self.assertRaisesRegex(ValueError,'Machining coverage'):
            verify(self.new)

    def test_unknown_status_rejected(self):
        self.write(self.new,covered=COVER_HEADER+'O1,T1,APPROVED,1,0,None\n')
        with self.assertRaisesRegex(ValueError,'machining coverage state'):
            verify(self.new)

    def test_uninspected_claimed_hole_rejected(self):
        self.write(self.new,covered=COVER_HEADER+'O1,T1,NOT_EVALUATED,1,0,Unknown\n')
        with self.assertRaisesRegex(ValueError,'Uninspected'):
            verify(self.new)

    def test_source_revision_mismatch_rejected(self):
        self.write(self.new,holes=HOLE_HEADER+'O1,T99,1,5,6,4,0,20,YES\n')
        with self.assertRaisesRegex(ValueError,'geometry revision'):
            verify(self.new)

    def test_missing_body_rejected(self):
        self.write(self.new,holes=HOLE_HEADER+'UNKNOWN,T1,1,5,6,4,0,20,YES\n')
        with self.assertRaisesRegex(ValueError,'missing body'):
            verify(self.new)

    def test_duplicate_hole_rejected(self):
        self.write(self.new,covered=COVER_HEADER+'O1,T1,INSPECTED_AXIAL_ONLY,2,0,Axial only\n',
                   holes=HOLE_HEADER+'O1,T1,1,5,6,4,0,20,YES\nO1,T1,1,6,6,4,0,20,YES\n')
        with self.assertRaisesRegex(ValueError,'Duplicate axial-hole'):
            verify(self.new)

    def test_nonfinite_coordinate_rejected(self):
        self.write(self.new,holes=HOLE_HEADER+'O1,T1,1,nan,6,4,0,20,YES\n')
        with self.assertRaisesRegex(ValueError,'Invalid hole dimension'):
            verify(self.new)

    def test_negative_depth_rejected(self):
        self.write(self.new,holes=HOLE_HEADER+'O1,T1,1,5,6,4,22,20,YES\n')
        with self.assertRaisesRegex(ValueError,'Invalid hole dimension'):
            verify(self.new)

    def test_surface_not_applicable_valid_with_no_holes(self):
        self.write(self.new,covered=COVER_HEADER+'O1,T1,SURFACE_NOT_APPLICABLE,0,0,Surface\n',holes=HOLE_HEADER)
        self.assertEqual(verify(self.new)['machining_holes'],0)

    def test_partial_coverage_flags_outstanding_review(self):
        self.write(self.new,covered=COVER_HEADER+'O1,T1,PARTIAL_CYLINDERS_EXCLUDED,1,2,Review nonaxial\n')
        self.assertEqual(verify(self.new)['machining_review_required'],1)
        self.assertIn('MACHINING_AXIAL_REVIEW',assess(self.old,self.new)['review_triggers'])

    def test_not_evaluated_counted_and_named(self):
        self.write(self.new,covered=COVER_HEADER+'O1,T1,NOT_EVALUATED,0,0,Unsupported geometry\n',holes=HOLE_HEADER)
        self.assertEqual(verify(self.new)['machining_review_required'],1)

    def test_not_evaluated_must_have_reason(self):
        self.write(self.new,covered=COVER_HEADER+'O1,T1,NOT_EVALUATED,0,0,\n',holes=HOLE_HEADER)
        with self.assertRaisesRegex(ValueError,'no explanation'):
            verify(self.new)

    def test_manifests_cannot_claim_manufacturing_approval(self):
        mf=self.new/'handoff_manifest.json'
        d=json.loads(mf.read_text());d['status']='ISSUED_FOR_MANUFACTURE'
        mf.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'approval state'):
            verify(self.new)

    def test_unlisted_extra_machining_file_rejected(self):
        (self.new/'extra_machining.csv').write_text('not expected')
        with self.assertRaises(ValueError):
            verify(self.new)

    def test_invalid_manifest_machine_hole_count_rejected(self):
        mf=self.new/'handoff_manifest.json'
        d=json.loads(mf.read_text());d['machining_holes']=5
        mf.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'Machining hole/review totals'):
            verify(self.new)

    def template(self, **changes):
        row = dict(body_id='O1', geometry_token='T1', hole_number='1',
                   nominal_x_mm='5.0', nominal_y_mm='6.0', nominal_diameter_mm='4.0',
                   measured_datum_x_mm='', measured_datum_y_mm='',
                   measured_projected_x_mm='', measured_projected_y_mm='',
                   projected_height_mm='', actual_diameter_mm='',
                   minimum_diameter_mm='', maximum_diameter_mm='',
                   diametral_position_tolerance_mm='', material_condition='',
                   maximum_pin_diameter_mm='', inspection_status='UNMEASURED')
        row.update(changes)
        header = ','.join(row)
        return (header+'\n'+','.join(row.values())+'\n').encode()

    def test_nominal_inspection_template_cross_links_to_signed_holes(self):
        self.write(self.new, extra={'hole_position_inspection_TEMPLATE.csv': self.template()})
        self.assertEqual(verify(self.new)['machining_holes'],1)

    def test_inspection_template_rejects_fabricated_measurements(self):
        for key, value in (('actual_diameter_mm','4.1'),
                           ('measured_datum_x_mm','5.03'),
                           ('inspection_status','PASS')):
            with self.subTest(key=key):
                self.write(self.new, extra={'hole_position_inspection_TEMPLATE.csv':
                                            self.template(**{key:value})})
                with self.assertRaisesRegex(ValueError,'fabricated|measurement'):
                    verify(self.new)

    def test_inspection_template_rejects_geometry_revision_spoofing(self):
        for key,value in (('geometry_token','newer'),('nominal_x_mm','99'),
                          ('nominal_diameter_mm','4.7')):
            with self.subTest(key=key):
                self.write(self.new, extra={'hole_position_inspection_TEMPLATE.csv':
                                            self.template(**{key:value})})
                with self.assertRaisesRegex(ValueError,'geometry and revision'):
                    verify(self.new)

    def test_inspection_template_cannot_inject_hidden_approval_column(self):
        raw=self.template().decode().splitlines()
        rogue=(raw[0]+",manufacturing_approval\n"+raw[1]+",YES\n").encode()
        self.write(self.new, extra={'hole_position_inspection_TEMPLATE.csv':rogue})
        with self.assertRaisesRegex(ValueError,'unexpected or missing column'):
            verify(self.new)

    def test_inspection_template_cannot_add_unknown_hole(self):
        template=self.template()
        header, data = template.decode().splitlines()
        self.write(self.new, extra={'hole_position_inspection_TEMPLATE.csv':
                                    (header+'\n'+data+'\n'+data+'\n').encode()})
        with self.assertRaisesRegex(ValueError,'cover every CAD design hole'):
            verify(self.new)

    def test_legacy_v2_handoff_without_optional_template_remains_valid(self):
        self.write(self.new)
        self.assertEqual(verify(self.new)['schema_version'],2)

    def test_native_export_adds_blank_template_to_signed_manifest(self):
        cpp=(ROOT/'src/core/ManufacturingHandoff.cpp').read_text(encoding='utf-8')
        self.assertIn('stageFile(staged,"hole_position_inspection_TEMPLATE.csv"',cpp)
        self.assertLess(cpp.index('stageFile(staged,"hole_position_inspection_TEMPLATE.csv"'),
                        cpp.index('manifest.insert("schema_version",2)'))
        self.assertIn('"UNMEASURED"',cpp)
        self.assertIn('number(hole.xMm),number(hole.yMm)',cpp)

    def test_cpp_handoff_publishes_both_tables_before_manifest(self):
        text=(ROOT/'src/core/ManufacturingHandoff.cpp').read_text(encoding='utf-8')
        self.assertIn('inspectAxialHoles(part,&holes,&inspectionIssue)',text)
        self.assertLess(text.index('stageFile(staged,"machining_coverage.csv"'),text.index('manifest.insert("schema_version",2)'))
        self.assertLess(text.index('stageFile(staged,"axial_hole_locations.csv"'),text.index('manifest.insert("schema_version",2)'))
        self.assertIn('"PARTIAL_CYLINDERS_EXCLUDED"',text)

if __name__=='__main__':unittest.main()

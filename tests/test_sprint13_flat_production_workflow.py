"""Sprint 13: end-to-end native export shape, revision, handoff and bundle invariants."""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from verify_linked_flat_package import verify
from engineering_readiness_workflow import evaluate
from engineering_evidence_bundle import build_bundle,verify_bundle
import v170_machining_handoff_v2_tests as fixtures

class LinkedFlatPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.folder=self.root/'flat';self.folder.mkdir()
        self.create()

    def create(self,*,bends=((90.,2.,.33),(-90.,3.,.40)),relief=(2.,1.),revision='T1'):
        folder=self.folder
        for path in folder.iterdir():path.unlink()
        flanges=[100.,70.,60.]
        thickness,width=2.,45.
        length=flanges[0];stations=[];allowances=[]
        rows=[]
        for i,(angle,radius,k) in enumerate(bends):
            allowance=math.radians(abs(angle))*(radius+k*thickness)
            station=length+allowance/2
            allowances.append(allowance);stations.append(station)
            length+=allowance+flanges[i+1]
            rows.append([str(i+1),'UP' if angle>0 else 'DOWN',angle,radius,k,allowance,
                         2*(radius+thickness)*math.tan(math.radians(abs(angle))/2)-allowance,station,length,width])
        for row in rows:row[8]=length
        rw,rd=relief
        contour=[(0.,0.)]
        if rw:
            for x in stations:contour.extend([(x-rw/2,0.),(x-rw/2,rd),(x+rw/2,rd),(x+rw/2,0.)])
        contour += [(length,0.),(length,width)]
        if rw:
            for x in reversed(stations):contour.extend([(x+rw/2,width),(x+rw/2,width-rd),(x-rw/2,width-rd),(x-rw/2,width)])
        contour += [(0.,width),(0.,0.)]
        dxf=[]
        def dline(layer,a,b):
            dxf.extend(['0','LINE','8',layer,'10',f'{a[0]:.6f}','20',f'{a[1]:.6f}',
                        '30','0.0','11',f'{b[0]:.6f}','21',f'{b[1]:.6f}','31','0.0'])
        dxf += ['0','SECTION','2','ENTITIES']
        for a,b in zip(contour,contour[1:]):dline('OUTLINE',a,b)
        for x in stations:dline('BEND',(x,rd),(x,width-rd))
        dxf+=['0','ENDSEC','0','EOF']
        (folder/'flat-pattern.dxf').write_text('\n'.join(dxf)+'\n',encoding='ascii')
        values=['M0,0']+[f'L{x:.6f},{y:.6f}' for x,y in contour[1:-1]]+['Z']
        svg=(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {length:.6f} {width:.6f}" '
             f'data-document-type="folded-sheet-flat-preview" data-bend-count="{len(bends)}">'
             '<path id="blank-outline" d="'+' '.join(values)+'"/>')
        for i,(x,(angle,_,_)) in enumerate(zip(stations,bends)):
            svg+=f'<path id="bend-{i+1}" data-angle-degrees="{angle}" d="M{x:.6f},{rd:.6f} V{width-rd:.6f}"/>'
        svg+='</svg>'
        (folder/'flat-pattern.svg').write_text(svg,encoding='utf-8')
        with (folder/'bend-schedule.csv').open('w',encoding='utf-8',newline='') as stream:
            writer=csv.writer(stream)
            writer.writerow(['bend_index','direction','angle_degrees','inside_radius_mm','k_factor',
                            'bend_allowance_mm','bend_deduction_mm','bend_station_mm','flat_length_mm','sheet_width_mm'])
            writer.writerows(rows)
        mf={'format':'CADWorkbench associative multi-flange package v1',
            'folded_source_id':'O1','flat_body_id':'FLAT-1',
            'source_geometry_token':revision,'linked_source_token':revision,'source_name':'Bracket',
            'width_mm':width,'thickness_mm':thickness,'relief_width_mm':rw,'relief_depth_mm':rd,
            'straight_flange_lengths_mm':flanges,'per_bend_k_factors':[b[2] for b in bends],
            'bends':[{'signed_angle_degrees':b[0],'inside_radius_mm':b[1]} for b in bends]}
        self.rehash(mf)

    def rehash(self,mf=None):
        if mf is None:mf=json.loads((self.folder/'manifest.json').read_text())
        mf['sha256']={name:hashlib.sha256((self.folder/name).read_bytes()).hexdigest()
                      for name in ('flat-pattern.svg','flat-pattern.dxf','bend-schedule.csv')}
        (self.folder/'manifest.json').write_text(json.dumps(mf),encoding='utf-8')

    def test_good_relief_and_bend_stations(self):
        data=verify(self.folder)
        self.assertEqual(data['bend_count'],2)
        self.assertEqual(data['reverse_bend_count'],1)
        self.assertFalse(data['release_approved'])
        self.assertFalse(data['shop_spacing_review_required'])

    def test_shop_limit_requires_review(self):
        gap=verify(self.folder)['minimum_bend_center_spacing_mm']
        self.assertEqual(verify(self.folder,minimum_center_spacing_mm=gap+1)['too_close_bend_pairs'],[1])

    def test_stale_flat_token_rejected(self):
        mf=json.loads((self.folder/'manifest.json').read_text());mf['linked_source_token']='other'
        self.rehash(mf)
        with self.assertRaisesRegex(ValueError,'stale'):verify(self.folder)

    def test_dxf_wrong_bend_x_even_with_valid_hash(self):
        p=self.folder/'flat-pattern.dxf';raw=p.read_text();raw=raw.replace('\n8\nBEND\n10\n', '\n8\nBEND\n10\n999.000000',1)
        p.write_text(raw);self.rehash()
        with self.assertRaises(ValueError):verify(self.folder)

    def test_csv_wrong_station_even_with_valid_hash(self):
        p=self.folder/'bend-schedule.csv'
        with p.open(newline='') as stream:records=list(csv.reader(stream))
        records[1][7]=str(float(records[1][7])+2.0)
        with p.open('w',newline='') as stream:csv.writer(stream).writerows(records)
        self.rehash()
        with self.assertRaises(ValueError):verify(self.folder)

    def test_svg_wrong_outline_even_with_valid_hash(self):
        p=self.folder/'flat-pattern.svg';raw=p.read_text().replace('M0,0','M7,0',1)
        p.write_text(raw);self.rehash()
        with self.assertRaisesRegex(ValueError,'SVG'):verify(self.folder)

    def test_csv_deduction_tamper_rehashed(self):
        p=self.folder/'bend-schedule.csv'
        with p.open(newline='') as stream:records=list(csv.reader(stream))
        records[1][6]=str(float(records[1][6])+0.5)
        with p.open('w',newline='') as stream:csv.writer(stream).writerows(records)
        self.rehash()
        with self.assertRaisesRegex(ValueError,'bend deduction'):verify(self.folder)

    def test_no_relief(self):
        self.create(relief=(0.,0.))
        self.assertEqual(verify(self.folder)['bend_count'],2)

    def test_duplicate_or_linked_file(self):
        (self.folder/'unexpected.txt').write_text('x')
        with self.assertRaises(ValueError):verify(self.folder)

    def test_nonfinite_manifest(self):
        mf=json.loads((self.folder/'manifest.json').read_text());mf['width_mm']=float('nan')
        self.rehash(mf)
        with self.assertRaises(ValueError):verify(self.folder)

    def test_cross_handoff_stale_geometry_rejected(self):
        fixture=fixtures.MachiningHandoffV2Tests('test_v2_verified')
        fixture.setUp();self.addCleanup(fixture.doCleanups)
        fixture.write(fixture.new)
        self.create(revision='wrong')
        with self.assertRaisesRegex(ValueError,'source revision'):
            evaluate(fixture.new,sheet_metal_packages=(self.folder,))

    def test_readiness_and_evidence_integration(self):
        fixture=fixtures.MachiningHandoffV2Tests('test_v2_verified')
        fixture.setUp();self.addCleanup(fixture.doCleanups)
        fixture.write(fixture.new)
        r=evaluate(fixture.new,sheet_metal_packages=(self.folder,))
        self.assertEqual(len(r['sheet_metal_summary']),1)
        self.assertTrue(any(x['code']=='FLAT_DEVELOPMENT_PHYSICAL_REVIEW_REQUIRED' for x in r['actions']))
        self.assertFalse(r['release_approved'])
        folder=self.root/'evidence'
        manifest=build_bundle(fixture.new,folder,sheet_metal_packages=(self.folder,))
        self.assertIn('sheet_metal_review.json',{x['path'] for x in manifest['files']})
        self.assertFalse(verify_bundle(folder)['release_approved'])
        # Change action count and regenerate the manifest hash: semantic verifier must reject.
        readiness=folder/'readiness.json'
        report=json.loads(readiness.read_text());report['open_action_count']+=1
        readiness.write_text(json.dumps(report))
        mf=json.loads((folder/'evidence_manifest.json').read_text())
        for f in mf['files']:
            if f['path']=='readiness.json':
                raw=readiness.read_bytes();f['bytes']=len(raw);f['sha256']=hashlib.sha256(raw).hexdigest()
        (folder/'evidence_manifest.json').write_text(json.dumps(mf))
        with self.assertRaisesRegex(ValueError,'action register'):verify_bundle(folder)

if __name__=='__main__':unittest.main()

class MultiPartNativeBatchTests(LinkedFlatPackageTests):
    def _build_batch(self,with_stock=False):
        import shutil
        batch=self.root/'batch';batch.mkdir()
        parts=[];entries=[]
        for i in range(2):
            name=f'part_{i+1:03d}_FLAT{i+1}'
            folder=batch/name
            shutil.copytree(self.folder,folder)
            mf=json.loads((folder/'manifest.json').read_text());mf['flat_body_id']=f'FLAT{i+1}'
            (folder/'manifest.json').write_text(json.dumps(mf))
            item={'package_directory':name,'flat_body_id':mf['flat_body_id'],
                  'folded_source_id':mf['folded_source_id'],
                  'svg_sha256':hashlib.sha256((folder/'flat-pattern.svg').read_bytes()).hexdigest(),
                  'dxf_sha256':hashlib.sha256((folder/'flat-pattern.dxf').read_bytes()).hexdigest(),
                  'bend_csv_sha256':hashlib.sha256((folder/'bend-schedule.csv').read_bytes()).hexdigest()}
            parts.append(item)
            entries.append([name,item['flat_body_id'],item['folded_source_id'],item['svg_sha256'],
                            item['dxf_sha256'],item['bend_csv_sha256']])
        with (batch/'batch-index.csv').open('w',newline='') as stream:
            w=csv.writer(stream,lineterminator='\n');w.writerow(('part_folder','flat_body_id','folded_source_id',
                      'svg_sha256','dxf_sha256','bend_csv_sha256'));w.writerows(entries)
        manifest={'format':'CADWorkbench multi-part associative sheet-metal job v1','part_count':2,
                 'parts':parts,'index_csv_sha256':hashlib.sha256((batch/'batch-index.csv').read_bytes()).hexdigest()}
        if with_stock:
            from verify_linked_flat_package import verify
            data=verify(self.folder)
            width,length=data['width_mm'],data['developed_length_mm']
            with (batch/'stock-layout.csv').open('w',newline='') as f:
                w=csv.writer(f,lineterminator='\n');w.writerow(('sheet','flat_body_id','x_mm','y_mm',
                         'length_mm','width_mm','rotated_90_deg'))
                w.writerow([1,'FLAT1',0,0,length,width,'NO'])
                w.writerow([1,'FLAT2',length+5,0,length,width,'NO'])
            (batch/'stock-layout.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg"/>')
            expected_area=width*length-2*2*2*1
            with (batch/'cut-length-summary.csv').open('w',newline='') as f:
                w=csv.writer(f,lineterminator='\n');w.writerow(('flat_body_id','material_area_mm2',
                      'relief_area_mm2','exposed_cut_length_mm'))
                for i in range(2):w.writerow([f'FLAT{i+1}',expected_area,8,1000])
            hashes={k:hashlib.sha256((batch/f).read_bytes()).hexdigest() for k,f in (
                ('stock_layout_csv_sha256','stock-layout.csv'),('stock_layout_svg_sha256','stock-layout.svg'),
                ('cut_length_csv_sha256','cut-length-summary.csv'))}
            manifest['stock_layout']={'stock_width_mm':1000.,'stock_height_mm':60.,
                     'sheet_count':1,'gap_mm':4.,'relief_aware_contour_placement':True,
                     'blank_envelope_area_mm2':length*width*2,
                     'net_contour_material_area_mm2':expected_area*2,
                     'total_exposed_neutral_cut_length_mm':2000.,**hashes}
        (batch/'batch-manifest.json').write_text(json.dumps(manifest))
        return batch

    def test_native_batch_index_and_stock_evidence(self):
        from verify_linked_flat_batch import verify
        batch=self._build_batch(with_stock=True)
        result=verify(batch)
        self.assertEqual(result['part_count'],2)
        self.assertEqual(result['stock_summary']['sheet_count'],1)
        self.assertFalse(result['release_approved'])

    def test_native_batch_without_stock(self):
        from verify_linked_flat_batch import verify
        result=verify(self._build_batch())
        self.assertEqual(len(result['parts']),2)
        self.assertIsNone(result['stock_summary'])

    def test_native_batch_index_substitution(self):
        from verify_linked_flat_batch import verify
        batch=self._build_batch()
        idx=batch/'batch-index.csv';idx.write_text(idx.read_text().replace('FLAT2','FLAT3'))
        with self.assertRaisesRegex(ValueError,'digest'):verify(batch)

    def test_native_batch_stock_overflow(self):
        from verify_linked_flat_batch import verify
        batch=self._build_batch(with_stock=True)
        csv_path=batch/'stock-layout.csv'
        raw=csv_path.read_text().replace('FLAT2,', 'FLAT2,')
        rows=list(csv.reader(io.StringIO(raw)))
        rows[2][2]='999.0'
        with csv_path.open('w',newline='') as f:csv.writer(f).writerows(rows)
        mf=json.loads((batch/'batch-manifest.json').read_text())
        mf['stock_layout']['stock_layout_csv_sha256']=hashlib.sha256(csv_path.read_bytes()).hexdigest()
        (batch/'batch-manifest.json').write_text(json.dumps(mf))
        with self.assertRaisesRegex(ValueError,'exceeds sheet'):verify(batch)

    def test_batch_readiness_integration(self):
        fixture=fixtures.MachiningHandoffV2Tests('test_v2_verified')
        fixture.setUp();self.addCleanup(fixture.doCleanups)
        fixture.write(fixture.new)
        batch=self._build_batch()
        r=evaluate(fixture.new,sheet_metal_batch=batch)
        self.assertEqual(r['sheet_metal_batch_summary']['part_count'],2)
        self.assertEqual(len(r['sheet_metal_summary']),2)
        self.assertFalse(r['release_approved'])
        evidence=self.root/'bundle'
        build_bundle(fixture.new,evidence,sheet_metal_batch=batch)
        self.assertEqual(len(json.loads((evidence/'sheet_metal_review.json').read_text())['packages']),2)
        self.assertFalse(verify_bundle(evidence)['release_approved'])

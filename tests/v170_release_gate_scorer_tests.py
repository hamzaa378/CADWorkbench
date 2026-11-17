"""Contract tests for the conservative 60-case score and 130 native JUnit gate."""
import csv,tempfile,unittest,xml.etree.ElementTree as ET
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from score_v170_gate import evaluate
ROOT=Path(__file__).resolve().parents[1]

class GateScorerTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);(self.root/'docs').mkdir()
        (self.root/'evidence').mkdir();(self.root/'evidence'/'case.txt').write_text('simulated fixture')
        with (ROOT/'docs/V170_ACCEPTANCE_RESULTS.csv').open(newline='') as f:
            self.rows=list(csv.DictReader(f))
        self.csvfile=self.root/'docs/cases.csv'
        self.junit=self.root/'junit.xml'
    def write_cases(self):
        with self.csvfile.open('w',newline='') as f:
            w=csv.DictWriter(f,fieldnames=self.rows[0].keys());w.writeheader();w.writerows(self.rows)
    def write_junit(self,n=130,failed=False,skipped=False):
        suite=ET.Element('testsuite')
        for i in range(n):
            case=ET.SubElement(suite,'testcase',name='case'+str(i))
            if i==3 and failed:ET.SubElement(case,'failure')
            if i==2 and skipped:ET.SubElement(case,'skipped')
        ET.ElementTree(suite).write(self.junit)
    def test_baseline_scores_unverified(self):
        self.write_cases();x=evaluate(self.csvfile,None)
        self.assertFalse(x['release_approved'])
        self.assertEqual(sum(v['passed'] for v in x['scenario_results'].values()),0)
    def test_all_case_ids_present(self):
        self.assertEqual(len(self.rows),60)
        self.assertEqual(len({r['id'] for r in self.rows}),60)
    def test_no_evidence_no_credit(self):
        self.rows[0]['status']='PASS';self.write_cases();self.write_junit()
        x=evaluate(self.csvfile,self.junit)
        self.assertEqual(x['scenario_results']['P']['passed'],0)
        self.assertTrue(any('requires an evidence' in p for p in x['problems']))
    def test_native_skips_and_failures_block(self):
        for missed in ('failed','skipped','short'):
            self.write_cases();self.write_junit(n=129 if missed=='short' else 130,
                failed=missed=='failed',skipped=missed=='skipped')
            self.assertFalse(evaluate(self.csvfile,self.junit)['native']['passed'])
    def test_critical_flag_tampering_does_not_bypass_gate(self):
        for row in self.rows:
            row['status']='PASS';row['evidence_path']='evidence/case.txt'
            if row['id']=='P01':row['critical']='false';row['status']='NOT_RUN'
        self.write_cases();self.write_junit()
        x=evaluate(self.csvfile,self.junit)
        self.assertFalse(x['release_approved'])
        self.assertIn('P01',x['scenario_results']['P']['critical_unaccepted'])
        self.assertTrue(any('inconsistent workstream' in p for p in x['problems']))

    def test_positive_scoring_fixture(self):
        for row in self.rows:
            row['status']='PASS';row['evidence_path']='evidence/case.txt'
        self.write_cases();self.write_junit()
        x=evaluate(self.csvfile,self.junit)
        self.assertTrue(x['release_approved'])
        self.assertEqual(x['scenario_results']['P']['passed'],20)
    def test_exact_85_85_65_boundary_with_critical_evidence(self):
        for letter,desired in [('P',17),('A',17),('D',13)]:
            all_rows=[r for r in self.rows if r['workstream']==letter]
            chosen=[r for r in all_rows if r['critical']=='true']
            chosen+= [r for r in all_rows if r['critical']=='false'][:desired-len(chosen)]
            ids={r['id'] for r in chosen}
            for row in all_rows:
                row['status']='PASS' if row['id'] in ids else 'NOT_RUN'
                row['evidence_path']='evidence/case.txt' if row['id'] in ids else ''
        self.write_cases();self.write_junit()
        x=evaluate(self.csvfile,self.junit)
        self.assertTrue(x['release_approved'])
        self.assertEqual([x['scenario_results'][k]['passed'] for k in 'PAD'],[17,17,13])

    def test_critical_required_despite_numeric_threshold(self):
        for row in self.rows:
            row['status']='PASS';row['evidence_path']='evidence/case.txt'
        for row in self.rows:
            if row['id']=='P01':row['status']='NOT_RUN'
        self.write_cases();self.write_junit()
        x=evaluate(self.csvfile,self.junit)
        self.assertFalse(x['release_approved'])
        self.assertIn('P01',x['scenario_results']['P']['critical_unaccepted'])
    def test_two_workstreams_must_reach_85(self):
        for row in self.rows:
            row['status']='PASS';row['evidence_path']='evidence/case.txt'
        # P and D only 16 each: critical all passed, but no two 17/20.
        for letter in ('P','D'):
            extras=[r for r in self.rows if r['workstream']==letter and r['critical']=='false']
            for row in extras[:4]:row['status']='NOT_RUN'
        self.write_cases();self.write_junit()
        x=evaluate(self.csvfile,self.junit)
        self.assertFalse(x['release_approved'])
if __name__=='__main__':unittest.main()

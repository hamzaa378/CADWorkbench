"""Tests for the full Windows native + 60-scenario audit evidence model."""
import csv
import importlib.util
import json
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

PROJECT=Path(__file__).resolve().parent.parent
MODULE=PROJECT/'tools/v170_full_test_audit.py'
SPEC=importlib.util.spec_from_file_location('v170_audit',MODULE)
audit=importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)

class FullAuditTest(unittest.TestCase):
    def make_report(self,path,failed=(),duplicate=False):
        root=ET.Element('testsuites');suite=ET.SubElement(root,'testsuite')
        names=audit.expected_tests(PROJECT/'CMakeLists.txt')
        for name in names:
            case=ET.SubElement(suite,'testcase',name=name,time='0.01')
            if name in failed:ET.SubElement(case,'failure',message='regression',type='failure').text='trace'
        if duplicate:ET.SubElement(suite,'testcase',name=names[0])
        ET.ElementTree(root).write(path,encoding='utf-8',xml_declaration=True)

    def check_outputs(self,directory):
        self.assertTrue((directory/'v170_full_audit.json').is_file())
        self.assertTrue((directory/'v170_native_130_matrix.csv').is_file())
        self.assertTrue((directory/'v170_acceptance_60_matrix.csv').is_file())
        self.assertTrue((directory/'v170_remaining_work.md').is_file())
        with (directory/'v170_acceptance_60_matrix.csv').open(newline='') as f:
            rows=list(csv.DictReader(f))
        self.assertEqual(len(rows),60)
        self.assertEqual(len(set(r['id'] for r in rows)),60)
        return json.loads((directory/'v170_full_audit.json').read_text(encoding="utf-8"))

    def test_all_native_pass_does_not_auto_accept_scenarios(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);junit=root/'pass.xml';self.make_report(junit)
            result=audit.run(PROJECT,junit,PROJECT/'docs/V170_ACCEPTANCE_RESULTS.csv',root/'out')
            self.assertEqual(result,1)
            report=self.check_outputs(root/'out')
            self.assertEqual(report['native_counts']['PASS'],130)
            self.assertTrue(report['native_complete'])
            self.assertFalse(report['release_gate_evidence_satisfied'])
            self.assertEqual([r['accepted'] for r in report['workstreams'].values()],[0,0,0])

    def test_failing_native_case_is_retained(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);junit=root/'fail.xml'
            self.make_report(junit,failed={'cadcore_folded_sheet_metal'})
            self.assertEqual(audit.run(PROJECT,junit,PROJECT/'docs/V170_ACCEPTANCE_RESULTS.csv',root/'out'),1)
            report=self.check_outputs(root/'out')
            self.assertEqual(report['native_counts']['FAIL'],1)
            rows=report['native_cases']
            self.assertEqual([r['name'] for r in rows if r['status']=='FAIL'],['cadcore_folded_sheet_metal'])

    def test_duplicate_native_result_blocks_completion(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);junit=root/'duplicate.xml';self.make_report(junit,duplicate=True)
            audit.run(PROJECT,junit,PROJECT/'docs/V170_ACCEPTANCE_RESULTS.csv',root/'out')
            report=self.check_outputs(root/'out')
            self.assertFalse(report['native_complete'])
            self.assertTrue(any('Duplicate native' in p for p in report['problems']))

    def test_missing_native_result_is_not_a_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            audit.run(PROJECT,root/'absent.xml',PROJECT/'docs/V170_ACCEPTANCE_RESULTS.csv',root/'out')
            report=self.check_outputs(root/'out')
            self.assertEqual(report['native_counts']['NOT_RUN'],130)
            self.assertFalse(report['native_complete'])

    def test_false_acceptance_evidence_is_not_counted(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);junit=root/'pass.xml';self.make_report(junit)
            with (PROJECT/'docs/V170_ACCEPTANCE_RESULTS.csv').open(newline='') as fd:
                lines=list(csv.DictReader(fd))
            lines[0]['status']='PASS';lines[0]['evidence_path']='this/does/not/exist.png'
            file=root/'fake.csv'
            with file.open('w',newline='') as f:
                writer=csv.DictWriter(f,fieldnames=lines[0].keys());writer.writeheader();writer.writerows(lines)
            audit.run(PROJECT,junit,file,root/'out')
            report=self.check_outputs(root/'out')
            self.assertEqual(report['workstreams']['P']['accepted'],0)
            self.assertTrue(any('P01' in p for p in report['problems']))

    def test_fine_grained_folded_commit_trace_and_runner_exist(self):
        text=(PROJECT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
        for phrase in ('[folded commit]', 'solid builder: before', 'linked propagation: before',
                       'undo snapshot: before', '[folded checkpoint]', 'B-rep deep copy: before'):
            self.assertIn(phrase,text)
        runner=(PROJECT/'RUN_V170_COMPREHENSIVE_AUDIT.ps1').read_text(encoding='utf-8')
        self.assertIn('--output-junit',runner)
        self.assertIn('unittest discover',runner)
        self.assertIn('CAD_V170_FOLDED_TRACE',runner)
        self.assertIn('tools/v170_full_test_audit.py',runner)
        self.assertIn('MANUAL ACCEPTANCE PENDING',runner)
        self.assertIn('exit 3',runner)
        self.assertIn('Automated verification FAILED',runner)

if __name__=='__main__':unittest.main()

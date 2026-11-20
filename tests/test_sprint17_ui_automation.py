"""Sprint 17 isolated GUI automation engine/guard regressions.
No Windows desktop, OCCT or pywinauto required for these tests.
"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'automation'))
from ui_workflow_engine import (AutomationBlocked, NotAvailable, WorkflowFailure,
                                exact_palette_match, normalized_label,
                                perform_step, run_workflows, validate_scenarios)
import runner


class FakeBackend:
    def __init__(self, name, workspace, *, missing=(), blocked=(), bad_count=False):
        self.name, self.workspace = name, workspace
        self.missing, self.blocked = set(missing), set(blocked)
        self.bad_count = bad_count
        self.count = 0
        self.events = []
        self.closed = False
    def start(self):
        self.events.append('start')
    def stop(self):
        self.events.append('stop')
        self.closed = True
    def screenshot(self, path):
        path.write_bytes(b'png-placeholder')
        return True
    def command(self, menu, label, query, *, execute=True):
        self.events.append(('command', label, execute))
        if label in self.missing:
            raise NotAvailable('Not in menu or Ctrl+K: ' + label)
        if label in self.blocked:
            raise AutomationBlocked('Accessibility unavailable: ' + label)
        if execute and label == 'Box...':
            self.count += 1
        if execute and label == 'New':
            self.count = 0
        return 'Ctrl+K search' if label.startswith('SearchOnly') else 'menu'
    def wait_body_count(self, expected):
        return self.count + (1 if self.bad_count else 0)
    def accept_dialog(self, title, fields=None, button='OK'):
        self.events.append(('accept', title, button))
    def save_file(self, path, title):
        kind = 'faceted-folded-sheet-v1' if self.name == 'folded_geometry' else 'rectangle-extrusion-v1'
        data = {'units':'mm','parts':[{'type':'solid',
          'parametric_definition': {'type':'rectangle-extrusion-v1',
              'parameters': {'width':40,'height':30,'depth':20}},
          'folded_sheet_definition':{'type':kind,'sheet_width_mm':25,'thickness_mm':1.5,
              'straight_flanges_mm':[40,30,35],
              'bends':[{'angle_degrees':90}, {'angle_degrees':-90}]}}]}
        Path(path).write_text(json.dumps(data), encoding='utf-8')
    def open_file(self, path):
        self.count = 1
    def dialog_text(self, title):
        return title + ' geometry verified'
    def select_body(self, index, *, extend=False):
        self.events.append(('select', index))
    def window_title(self):
        return 'CADWorkbench v1.70 - Untitled'


def simple(ident, label='Box...', *, probe=False):
    return {'id': ident, 'steps': [
        {'op':'command_probe' if probe else 'command', 'menu':['Create',label], 'label':label},
        *([] if probe else [{'op':'assert_bodies', 'count':1}])
    ]}


class Sprint17UIEngineTests(unittest.TestCase):
    def test_normalization_handles_qt_mnemonics(self):
        self.assertEqual(normalized_label('&Save &As...'), 'save as...')
    def test_normalization_ignores_shortcut(self):
        self.assertEqual(normalized_label('File  [Ctrl+S]'), 'file')
    def test_palette_exact_match_avoids_fuzzy(self):
        self.assertTrue(exact_palette_match('★  Box...    ·    Create    [B]', 'Box...'))
        self.assertFalse(exact_palette_match('Box array    ·    Create', 'Box...'))
    def test_validation_rejects_traversal_id(self):
        with self.assertRaises(ValueError):
            validate_scenarios([simple('../escape')])
    def test_validation_rejects_duplicates(self):
        with self.assertRaises(ValueError):
            validate_scenarios([simple('good'), simple('good')])
    def test_validation_rejects_missing_steps(self):
        with self.assertRaises(ValueError):
            validate_scenarios([{'id':'bad','steps':[]}])
    def test_validation_rejects_inconsistent_menu_label(self):
        bad = simple('badmenu'); bad['steps'][0]['label'] = 'Not box'
        with self.assertRaises(ValueError):
            validate_scenarios([bad])
    def test_validation_rejects_unknown_op(self):
        bad = simple('badop'); bad['steps'][0]['op'] = 'unsafe_action'
        with self.assertRaises(ValueError):
            validate_scenarios([bad])
    def test_unsafe_export_destination_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            for path in ['../outside.txt', '/tmp/unsafe.txt', 'C:\\escape.txt']:
                with self.subTest(path=path):
                    with self.assertRaises(WorkflowFailure):
                        perform_step(FakeBackend('one',temp), {'op':'save_file','name':path}, Path(temp))
    def test_successful_assertion_is_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            results = run_workflows(FakeBackend, [simple('solid')], Path(temp)/'report')
            self.assertEqual(results[0].status, 'PASS')
    def test_probe_is_discovered_not_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            results = run_workflows(FakeBackend, [simple('probe', probe=True)], Path(temp)/'report')
            self.assertEqual(results[0].status, 'DISCOVERED')
    def test_missing_command_is_not_ui_inconsistency_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            cases = [simple('unavailable', 'SearchOnlyCmd', probe=True), simple('subsequent')]
            made = []
            def create(name,path):
                obj = FakeBackend(name,path, missing={'SearchOnlyCmd'}); made.append(obj); return obj
            results = run_workflows(create, cases, Path(temp)/'report')
            self.assertEqual([e.status for e in results], ['NOT_AVAILABLE','PASS'])
            self.assertTrue(all(b.closed for b in made))
            report = json.loads((Path(temp)/'report'/'report.json').read_text(encoding='utf-8'))
            self.assertEqual(report['summary']['NOT_AVAILABLE'], 1)
            self.assertFalse(report['manual_acceptance'])
    def test_accessibility_failure_not_classified_as_geometry_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            def factory(name,path): return FakeBackend(name,path,blocked={'Box...'})
            result = run_workflows(factory,[simple('blocked')],Path(temp)/'report')
            self.assertEqual(result[0].status,'AUTOMATION_BLOCKED')
    def test_actual_wrong_body_count_is_fail(self):
        with tempfile.TemporaryDirectory() as temp:
            def factory(name,path): return FakeBackend(name,path,bad_count=True)
            result = run_workflows(factory,[simple('wrong')],Path(temp)/'report')
            self.assertEqual(result[0].status,'FAIL')
    def test_failure_does_not_stop_subsequent_independent_scenarios(self):
        with tempfile.TemporaryDirectory() as temp:
            def factory(name,path): return FakeBackend(name,path,bad_count=(name=='first'))
            results = run_workflows(factory,[simple('first'),simple('second')],Path(temp)/'report')
            self.assertEqual([x.status for x in results],['FAIL','PASS'])
    def test_html_escapes_untrusted_scenario_text(self):
        with tempfile.TemporaryDirectory() as temp:
            label = 'SearchOnly<script>alert(1)</script>'
            result = run_workflows(lambda n,p:FakeBackend(n,p,missing={label}),
                [simple('escaped',label,probe=True)],Path(temp)/'report')
            html = (Path(temp)/'report'/'report.html').read_text(encoding='utf-8')
            self.assertNotIn('<script>',html)
            self.assertIn('&lt;script&gt;',html)
    def test_report_is_atomic_json_and_html(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)/'report'
            run_workflows(FakeBackend,[simple('one')],folder)
            self.assertTrue((folder/'one'/'final_state.png').exists())
            self.assertFalse(list(folder.glob('*.tmp')))
            with self.assertRaises(ValueError):
                run_workflows(FakeBackend,[simple('one')],folder)
    def test_only_filter_works(self):
        with tempfile.TemporaryDirectory() as temp:
            results=run_workflows(FakeBackend,[simple('first'),simple('second')],
                                  Path(temp)/'report',selected={'second'})
            self.assertEqual([x.name for x in results],['second'])
    def test_full_scenario_suite_parses_and_has_provenance(self):
        suite = json.loads((ROOT/'automation/sprint17_ui_scenarios.json').read_text(encoding='utf-8'))
        self.assertEqual(suite['schema'],'cad-ui-scenarios-v1')
        validate_scenarios(suite['scenarios'])
        self.assertGreaterEqual(len(suite['scenarios']),10)
        actions = (ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
        for scenario in suite['scenarios']:
            for step in scenario['steps']:
                if step['op'] in ('command','command_probe'):
                    # Labels are sourced from real Qt QAction declarations (mnemonic may vary).
                    self.assertIn(step['label'],actions.replace('&',''), (scenario['id'],step['label']))
    def test_no_implicit_host_desktop_control(self):
        with self.assertRaises(ValueError):
            runner.require_ui_permission(in_isolated_vm=False,allow_active_desktop=False)
        with self.assertRaises(ValueError):
            runner.require_ui_permission(in_isolated_vm=True,allow_active_desktop=True)
        runner.require_ui_permission(in_isolated_vm=True,allow_active_desktop=False)
        runner.require_ui_permission(in_isolated_vm=False,allow_active_desktop=True)

    def test_saved_scene_assertion_checks_numeric_geometry(self):
        with tempfile.TemporaryDirectory() as temp:
            result=run_workflows(FakeBackend, [
                {'id':'scene', 'steps':[
                    {'op':'command','menu':['Create','Box...'],'label':'Box...'},
                    {'op':'save_file','name':'example.cadscene'},
                    {'op':'assert_scene','name':'example.cadscene','checks':[
                        {'path':'units','equals':'mm'},
                        {'path':'parts.0.parametric_definition.parameters.width','equals':40}]}]
                }], Path(temp)/'report')
            self.assertEqual(result[0].status,'PASS')

    def test_malformed_native_scene_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            file=Path(temp)/'invalid.cadscene'; file.write_text('not-json',encoding='utf-8')
            with self.assertRaises(WorkflowFailure):
                perform_step(FakeBackend('scene',temp),{'op':'assert_scene','name':file.name,
                    'checks':[{'path':'units','equals':'mm'}]},Path(temp))

    def test_cli_validate_without_gui(self):
        self.assertEqual(runner.cli(['--validate']),0)
    def test_cli_list_without_gui(self):
        self.assertEqual(runner.cli(['--list']),0)
    def test_report_uses_no_manual_acceptance_claim(self):
        with tempfile.TemporaryDirectory() as temp:
            directory=Path(temp)/'report'
            run_workflows(FakeBackend,[simple('one')],directory,execution_mode='vm')
            report=json.loads((directory/'report.json').read_text(encoding='utf-8'))
            self.assertEqual(report['mode'],'vm')
            self.assertFalse(report['manual_acceptance'])

if __name__ == '__main__':
    unittest.main()

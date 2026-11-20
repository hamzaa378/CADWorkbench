"""Windows GUI regression contracts for Qt dialogs invisible to top-level UIA.

Pure selection/dispatch tests run on Linux, and Windows gets actual HWND probing
when the user starts the desktop workflow runner. These tests cannot substitute
for the live Windows UI result.
"""
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'automation'))
from qt_dialog_discovery import WindowRecord, choose_verified_dialog, norm_title
from windows_ui_backend import WindowsCadUI
from ui_workflow_engine import AutomationBlocked


def rec(handle, label, pid=9, visible=True, cls='Qt6QWindowIcon'):
    return WindowRecord(handle, label, pid, visible, cls)


class QtDialogDiscoveryTests(unittest.TestCase):
    def test_normalization(self):
        self.assertEqual(norm_title('  Folded   sheet-metal BODY  '), 'folded sheet-metal body')

    def test_exact_dialog_can_be_selected_even_if_uia_top_level_omits_it(self):
        records = [rec(1, 'CADWorkbench v1.70.0 - Untitled'),
                   rec(2, 'Create Box'), rec(3,'Unrelated CAD menu')]
        chosen = choose_verified_dialog(records,pid=9,title='Create Box',main_hwnd=1)
        self.assertEqual(chosen.hwnd, 2)

    def test_selected_dialog_must_belong_to_cad_process(self):
        records = [rec(2, 'Create Box',pid=12), rec(3, 'Create Box',pid=9,visible=False)]
        self.assertIsNone(choose_verified_dialog(records,pid=9,title='Create Box'))

    def test_main_window_cannot_be_typed_into_as_a_dialog(self):
        records = [rec(1,'Create Box',pid=9),rec(2,'CADWorkbench v1.70.0 - Untitled',pid=9)]
        self.assertIsNone(choose_verified_dialog(records,pid=9,title='Create Box',main_hwnd=1))

    def test_exact_title_wins_over_a_substring_title(self):
        records = [rec(3,'Create Box - previous',pid=9),rec(4,'Create Box',pid=9)]
        self.assertEqual(choose_verified_dialog(records,pid=9,title='Create Box').hwnd,4)

    def test_partial_title_allowed_only_when_dialog_is_known_process(self):
        records = [rec(9,'Find CAD tools and components (Ctrl+K)',pid=9)]
        self.assertEqual(choose_verified_dialog(records,pid=9,title='Find CAD tools and components').hwnd,9)

    def test_unnamed_dialog_is_not_assumed_by_foreground(self):
        records = [rec(20,'',pid=9)]
        self.assertIsNone(choose_verified_dialog(records,pid=9,title='Create Box'))

    def test_missing_title_never_matches_all_windows(self):
        self.assertIsNone(choose_verified_dialog([rec(3,'Create Box')],pid=9,title=''))

    def test_modal_fields_use_verified_keyboard_route_if_uia_has_no_edits(self):
        cad = WindowsCadUI(Path('CADWorkbench.exe'),'box',Path('workspace'))
        cad._wait_dialog = Mock(return_value=object())
        cad._descendants = Mock(return_value=[])
        cad._keyboard_modal_fields = Mock()
        cad.accept_dialog('Create Box',[40,30,20])
        cad._keyboard_modal_fields.assert_called_once_with('Create Box',[40,30,20],'OK')

    def test_full_accessibility_uses_semantic_edits_and_ok_control(self):
        cad = WindowsCadUI(Path('CADWorkbench.exe'),'box',Path('workspace'))
        edits = [Mock(),Mock(),Mock()]
        button = Mock()
        cad._wait_dialog = Mock(return_value=object())
        cad._descendants = Mock(return_value=edits)
        cad._button = Mock(return_value=button)
        cad._keyboard_modal_fields = Mock()
        cad.accept_dialog('Create Box',[40,30,20])
        for widget,value in zip(edits,(40,30,20)):
            widget.set_edit_text.assert_called_once_with(str(value))
        button.click_input.assert_called_once()
        cad._keyboard_modal_fields.assert_not_called()

    def test_refold_fields_with_comma_lists_are_preserved(self):
        cad = WindowsCadUI(Path('CADWorkbench.exe'),'fold',Path('workspace'))
        cad._wait_dialog = Mock(return_value=object())
        cad._descendants = Mock(return_value=[])
        cad._keyboard_modal_fields = Mock()
        fields = ['40,30,35','90,-90','1.5,1.5','25','1.5']
        cad.accept_dialog('Folded sheet-metal body',fields)
        cad._keyboard_modal_fields.assert_called_once_with('Folded sheet-metal body',fields,'OK')

    def test_inaccessible_engineering_report_is_not_falsely_passed(self):
        cad = WindowsCadUI(Path('CADWorkbench.exe'),'audit',Path('workspace'))
        cad._wait_dialog = Mock(return_value=object())
        cad._descendants = Mock(return_value=[])
        with self.assertRaises(AutomationBlocked):
            cad.dialog_text('Integrated engineering preflight')

    def test_missing_button_does_not_trigger_blind_ui_action(self):
        cad = WindowsCadUI(Path('CADWorkbench.exe'),'box',Path('workspace'))
        cad._wait_dialog = Mock(return_value=object())
        cad._descendants = Mock(return_value=[Mock(),Mock(),Mock()])
        cad._button = Mock(return_value=None)
        cad._keyboard_modal_fields = Mock()
        cad.accept_dialog('Create Box',[40,30,20])
        cad._keyboard_modal_fields.assert_called_once()

if __name__ == '__main__':
    unittest.main()

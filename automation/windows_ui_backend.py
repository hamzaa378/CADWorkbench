"""Real Windows GUI driver for CADWorkbench (Qt/Win32 accessibility/UIA).

Menu location is not an assertion: try the expected menu and then Ctrl+K.
No coordinates, OCR, silent menu-removal failures, or undocumented CAD API calls.
"""
from __future__ import annotations

import re
import time
from pathlib import Path

from ui_workflow_engine import (NotAvailable, AutomationBlocked, PreconditionBlocked,
                                WorkflowFailure, exact_palette_match, normalized_label)
from qt_dialog_discovery import choose_verified_dialog, windows_for_process, norm_title


class WindowsCadUI:
    def __init__(self, executable: Path, name: str, workspace: Path, timeout: float = 15.0):
        self.executable = Path(executable)
        self.name = name
        self.workspace = Path(workspace)
        self.timeout = timeout
        self.app = None
        self.win = None
        self.desktop = None
        self.keys = None
        self.pid = 0
        self._routes: list[dict] = []
        self._dialog_hwnd = 0

    def start(self):
        if not self.executable.is_file():
            raise AutomationBlocked(f"CADWorkbench binary not found: {self.executable}")
        try:
            from pywinauto import Application, Desktop
            from pywinauto.keyboard import send_keys
        except ImportError as exc:
            raise AutomationBlocked("Install pywinauto on Windows: py -m pip install pywinauto") from exc
        self.keys = send_keys
        self.desktop = Desktop(backend="uia")
        try:
            self.app = Application(backend="uia").start(str(self.executable), work_dir=str(self.executable.parent))
            self.pid = self.app.process
            self.win = self.app.window(title_re=r".*CADWorkbench v.*")
            self.win.wait("visible", timeout=self.timeout)
            self.win.set_focus()
        except Exception as exc:
            raise AutomationBlocked(f"Application did not expose its main window: {exc}") from exc

    def stop(self):
        if self.app is None:
            return
        try:
            self.app.kill(soft=False)
        except Exception:
            pass

    @staticmethod
    def _text(element) -> str:
        try:
            return (element.window_text() or "").strip()
        except Exception:
            return ""

    def _visible_windows(self):
        try:
            return [w for w in self.desktop.windows(process=self.pid) if w.is_visible()]
        except Exception as exc:
            raise AutomationBlocked(f"Unable to enumerate CAD windows: {exc}") from exc

    def _poll(self, operation, seconds=None):
        until = time.monotonic() + (seconds or self.timeout)
        last = None
        while time.monotonic() < until:
            try:
                item = operation()
                if item is not None:
                    return item
            except Exception as exc:
                last = exc
            time.sleep(.15)
        return None

    def _lookup_menu_item(self, label: str):
        for window in self._visible_windows():
            try:
                for item in window.descendants(control_type="MenuItem"):
                    if normalized_label(self._text(item)) == normalized_label(label):
                        return item
            except Exception:
                continue
        return None

    def _try_menu(self, path: list[str], execute: bool) -> bool:
        # Opening an app menu is never proof of an engineering feature.
        try:
            self.win.set_focus()
            root = path[0]
            top = self._lookup_menu_item(root)
            if top is None:
                return False
            top.click_input()
            for ix, label in enumerate(path[1:]):
                target = self._poll(lambda: self._lookup_menu_item(label), 2.5)
                if target is None:
                    self.keys("{ESC}")
                    return False
                if ix == len(path)-2 and not execute:
                    self.keys("{ESC}")
                    return True
                target.click_input()
            return True
        except Exception:
            try:
                self.keys("{ESC}")
            except Exception:
                pass
            return False

    def _palette(self, query: str, exact: str, execute: bool) -> str:
        try:
            self.win.set_focus()
            self.keys("{ESC 2}")
            self.win.set_focus()
            self.keys("^k")
        except Exception as exc:
            raise AutomationBlocked(f"Could not open Ctrl+K command search: {exc}") from exc
        palette = self._poll(lambda: self._dialog("Find CAD tools and components"), 6)
        if palette is None:
            raise AutomationBlocked("Ctrl+K command palette not exposed to Windows UI Automation")
        try:
            edits = self._descendants(palette, "Edit")
            if edits:
                edits[0].set_edit_text(query)
            else:
                self._focus_verified_modal("Find CAD tools and components")
                self.keys("^a")
                self.keys(query, with_spaces=True)
        except AutomationBlocked:
            raise
        except Exception as exc:
            raise AutomationBlocked(f"Could not type in command search: {exc}") from exc

        def matching():
            try:
                for item in self._descendants(palette, "ListItem"):
                    if exact_palette_match(self._text(item), exact):
                        return item
            except Exception:
                pass
            return None
        item = self._poll(matching, 4)
        if item is None:
            try:
                content = ' '.join(self._text(x) for x in self._descendants(palette, "Text"))
            except Exception:
                content = ""
            try:
                self.keys("{ESC}")
            except Exception:
                pass
            if re.search(r"\b0 matching command", content):
                raise NotAvailable(f"Command '{exact}' not in menu or Ctrl+K results (query: {query})")
            # An exact result missing in an otherwise live list means the command
            # is unavailable to the current context; don't click a fuzzy match.
            if re.search(r"\b\d+ matching command", content):
                raise NotAvailable(f"No enabled exact command '{exact}' in Ctrl+K search")
            raise AutomationBlocked("Could not read Ctrl+K result items or matching count")
        if execute:
            try:
                item.double_click_input()
            except Exception as exc:
                raise AutomationBlocked(f"Found command but could not run it: {exc}") from exc
        else:
            try:
                self.keys("{ESC}")
            except Exception:
                pass
        return "Ctrl+K search"

    def command(self, menu: list[str], label: str, query: str, *, execute=True) -> str:
        if not isinstance(menu, list) or len(menu) < 2 or menu[-1] != label:
            raise WorkflowFailure(f"Malformed menu route for {label}")
        if self._try_menu(menu, execute):
            return "menu: " + " > ".join(menu)
        # Mandatory fallback: UI inconsistency is NOT the outcome.
        return self._palette(query, label, execute)

    def _native_dialog_record(self, title: str):
        """Qt-owned dialogs can exist as HWNDs without UIA top-level exposure."""
        try:
            return choose_verified_dialog(
                windows_for_process(self.pid, include_children_of=getattr(self.win, "handle", 0)),
                pid=self.pid, title=title, main_hwnd=getattr(self.win, "handle", 0))
        except ImportError as exc:
            raise AutomationBlocked("Windows dialog discovery requires pywin32 (win32gui)") from exc
        except Exception:
            return None

    def _dialog(self, title: str):
        wanted = normalized_label(title)
        # First prefer UI Automation's verified CAD process windows.
        try:
            for w in self._visible_windows():
                if wanted in normalized_label(self._text(w)) and not normalized_label(self._text(w)).startswith("cadworkbench v"):
                    self._dialog_hwnd = getattr(w, "handle", 0)
                    return w
        except AutomationBlocked:
            pass
        # Qt6: process-filtered UIA top-level enumeration may omit owned QDialogs.
        candidate = self._native_dialog_record(title)
        if candidate is None:
            return None
        self._dialog_hwnd = candidate.hwnd
        # UIA via verified HWND can expose widgets even when UIA's window list omits it.
        try:
            wrapped = self.desktop.window(handle=candidate.hwnd).wrapper_object()
            if wrapped is not None:
                return wrapped
        except Exception:
            pass
        # Qt dialog may only expose a Win32 wrapper. Keyboard navigation is
        # allowed ONLY after the correct CAD-process HWND and title are verified.
        try:
            from pywinauto import Desktop
            return Desktop(backend="win32").window(handle=candidate.hwnd).wrapper_object()
        except Exception:
            return candidate

    def _visible_native_titles(self):
        try:
            return [f"{r.hwnd:#x} {r.class_name}: {r.title!r}"
                    for r in windows_for_process(self.pid, include_children_of=getattr(self.win,"handle",0))
                    if r.visible][:24]
        except Exception:
            return []

    def _wait_dialog(self, title: str):
        self._dialog_hwnd = 0
        dialog = self._poll(lambda: self._dialog(title), self.timeout)
        if dialog is None:
            visible = [self._text(w) for w in self._visible_windows()]
            native = self._visible_native_titles()
            raise AutomationBlocked(
                f"Expected Qt dialog '{title}' inaccessible. UIA windows: {visible}; "
                f"Win32 CAD windows: {native}")
        return dialog

    @staticmethod
    def _descendants(dialog, control_type):
        try:
            return dialog.descendants(control_type=control_type)
        except Exception:
            return []

    @classmethod
    def _button(cls, dialog, title):
        return next((x for x in cls._descendants(dialog, "Button")
                     if normalized_label(x.window_text()) == normalized_label(title)), None)

    def _focus_verified_modal(self, title: str):
        """Never send fallback keystrokes unless the expected dialog is foreground."""
        import win32gui
        candidate = self._native_dialog_record(title)
        if candidate is None or not win32gui.IsWindow(candidate.hwnd):
            raise AutomationBlocked(f"Keyboard fallback refused: '{title}' modal HWND not verified")
        try:
            win32gui.SetForegroundWindow(candidate.hwnd)
        except Exception:
            pass
        for _ in range(12):
            active = win32gui.GetForegroundWindow()
            if active == candidate.hwnd or win32gui.GetAncestor(active, 2) == candidate.hwnd:
                return
            time.sleep(.08)
        raise AutomationBlocked(f"Keyboard fallback refused: '{title}' is not foreground")

    def _keyboard_modal_fields(self, title: str, fields: list, button: str):
        """Qt QFormLayout uses predictable TAB order, but only on a verified modal."""
        self._focus_verified_modal(title)
        if fields:
            # The CAD dialogs under test focus the first QDoubleSpinBox/QLineEdit.
            # Never use screen coordinates or type if another process has focus.
            for index, value in enumerate(fields):
                self.keys("^a")
                self.keys(str(value), with_spaces=True, pause=0.025)
                if index != len(fields)-1:
                    self.keys("{TAB}")
        if normalized_label(button) in ("cancel", "no", "close"):
            self.keys("{ESC}")
        elif normalized_label(button) in ("ok", "yes", "open", "save"):
            self.keys("{ENTER}")
        else:
            raise AutomationBlocked(f"Keyboard fallback has no safe key for '{button}'")

    def accept_dialog(self, title: str, fields: list | None = None, button="OK"):
        dialog = self._wait_dialog(title)
        values = list(fields or [])
        edits = self._descendants(dialog, "Edit")
        control = self._button(dialog, button)
        if values and len(edits) >= len(values) and control is not None:
            # Prefer semantic UIA when all fields and the confirmation control
            # are available; don't leave partially edited dialogs to keyboard mode.
            try:
                for index, value in enumerate(values):
                    edits[index].set_edit_text(str(value))
                control.click_input()
                return
            except Exception as exc:
                raise AutomationBlocked(f"Accessible Qt dialog '{title}' could not be completed: {exc}") from exc
        if not values and control is not None:
            try:
                control.click_input()
                return
            except Exception:
                pass
        self._keyboard_modal_fields(title, values, button)

    def dialog_text(self, title):
        dialog = self._wait_dialog(title)
        pieces = [self._text(e) for e in self._descendants(dialog, "Text")]
        if not pieces:
            pieces = [self._text(e) for e in self._descendants(dialog, "Static")]
        if pieces:
            return ' '.join(pieces)
        # Native HWND proves the dialog exists but does not prove the contents.
        raise AutomationBlocked(f"Dialog '{title}' is present but text is inaccessible; "
                                "cannot assert its engineering contents")

    def window_title(self):
        if self.win is None:
            raise AutomationBlocked("Application window not available")
        return self._text(self.win)

    def wait_body_count(self, wanted: int):
        """Retry because CAD/OCCT scene refresh may be asynchronous."""
        until = time.monotonic() + self.timeout
        observed = None
        while time.monotonic() < until:
            try:
                observed = self.body_count()
                if observed == wanted:
                    return observed
            except AutomationBlocked:
                pass
            time.sleep(.2)
        if observed is None:
            raise AutomationBlocked("Could not read model body count from UIA or status bar")
        return observed

    def body_count(self):
        texts = [self._text(e) for e in self.win.descendants()]
        texts.extend(self._text(w) for w in self._visible_windows())
        for v in texts:
            match = re.search(r"Showing\s+(\d+)\s+of\s+(\d+)\s+bodies", v, re.I)
            if match:
                return int(match.group(2))
        for v in texts:
            match = re.search(r"\b(\d+)\s+bodies\s*\|\s*millimeters", v, re.I)
            if match:
                return int(match.group(1))
        raise AutomationBlocked("Body counter not exposed by UIA; cannot assert geometry creation")

    def select_body(self, index: int, *, extend=False):
        if index < 0:
            raise WorkflowFailure("Negative body index")
        try:
            # QTreeWidget is usually Table or Tree in Qt6 accessibility.
            candidates = []
            for kind in ("TreeItem", "DataItem", "ListItem"):
                candidates.extend(self.win.descendants(control_type=kind))
            matches = []
            for item in candidates:
                title = self._text(item)
                if 'occurrence' in title.casefold() or title.startswith(('Box', 'Constrained', 'Folded', 'Cylinder')):
                    if title not in [self._text(x) for x in matches]:
                        matches.append(item)
            if index >= len(matches):
                raise AutomationBlocked(f"Cannot access body row {index}; {len(matches)} candidate UIA rows")
            matches[index].click_input(pressed="ctrl" if extend else "")
        except AutomationBlocked:
            raise
        except Exception as exc:
            raise AutomationBlocked(f"Design Browser selection unavailable: {exc}") from exc

    def _file_dialog(self, title: str, path: str, button: str):
        dialog = self._wait_dialog(title)
        edits = self._descendants(dialog, "Edit")
        control = self._button(dialog, button)
        if edits and control is not None:
            try:
                target = next((x for x in edits if 'file name' in self._text(x).casefold()), None)
                if target is None:
                    target = edits[-1]
                target.set_edit_text(path)
                control.click_input()
                return
            except Exception:
                pass
        # Native QFileDialog / Common Item Dialog: Alt+N selects filename.
        # We must first verify the CAD-owned dialog HWND and foreground.
        self._focus_verified_modal(title)
        self.keys("%n")
        self.keys("^a")
        self.keys(path, with_spaces=True)
        self.keys("{ENTER}")

    def save_file(self, path: str, title: str):
        self._file_dialog(title, path, "Save")

    def open_file(self, path: str):
        self.command(["File", "Open..."], "Open...", "Open")
        self._file_dialog("Open CAD scene", path, "Open")

    def screenshot(self, path: Path):
        if self.win is None:
            return False
        try:
            self.win.capture_as_image().save(str(path))
            return path.is_file()
        except Exception:
            pass
        try:
            # Keep evidence scoped to CADWorkbench rather than capturing private
            # unrelated application windows or the entire user's desktop.
            from PIL import ImageGrab
            import win32gui
            hwnd = getattr(self.win, "handle", 0)
            if not hwnd or not win32gui.IsWindow(hwnd):
                return False
            left, top, right, bottom = win32gui.GetWindowRect(hwnd)
            if right <= left or bottom <= top:
                return False
            ImageGrab.grab(bbox=(left, top, right, bottom), all_screens=True).save(str(path))
            return path.is_file()
        except Exception:
            return False

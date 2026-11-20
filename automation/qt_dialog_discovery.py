"""Windows/Qt modal discovery with conservative, title-verified targeting.

Qt6 often hosts dialogs in separately owned HWNDs not returned by
pywinauto's process-filtered UIA Desktop.windows() query. Windows-owned
handles are a second *observation* mechanism, never permission to type
into an unrelated window.  This module has no CAD geometry dependencies.
"""
from __future__ import annotations
from dataclasses import dataclass


def norm_title(value: str) -> str:
    return " ".join((value or "").casefold().split())


@dataclass(frozen=True)
class WindowRecord:
    hwnd: int
    title: str
    pid: int
    visible: bool
    class_name: str = ""


def choose_verified_dialog(records: list[WindowRecord], *, pid: int, title: str,
                           main_hwnd: int = 0) -> WindowRecord | None:
    """Require a matching visible window and correct process; no fuzzy foreground guessing.

    Exact titles win; an explicitly titled dialog with suffix information is
    allowed as a fallback. Main CADWorkbench window can never be returned.
    """
    wanted = norm_title(title)
    if not wanted:
        return None
    valid = [x for x in records if x.pid == pid and x.visible and x.hwnd != main_hwnd
             and not norm_title(x.title).startswith("cadworkbench v")]
    for r in valid:
        if norm_title(r.title) == wanted:
            return r
    for r in valid:
        if wanted in norm_title(r.title):
            return r
    return None


def windows_for_process(pid: int, *, include_children_of: int = 0) -> list[WindowRecord]:
    """Return visible, owned Qt and native Win32 windows for the CAD process.

    The same HWND may appear in both top-level and child enumeration; it is
    de-duplicated.  A missing pywin32 installation is a declared dependency
    error rather than a silent desktop driver failure.
    """
    import win32gui
    import win32process
    seen: dict[int, WindowRecord] = {}

    def add(hwnd, _):
        try:
            _, found_pid = win32process.GetWindowThreadProcessId(hwnd)
            if found_pid != pid:
                return
            seen[hwnd] = WindowRecord(
                hwnd=hwnd, title=win32gui.GetWindowText(hwnd), pid=found_pid,
                visible=bool(win32gui.IsWindowVisible(hwnd)),
                class_name=win32gui.GetClassName(hwnd))
        except Exception:
            pass  # A window can close between enumeration and inspection.

    win32gui.EnumWindows(add, None)
    if include_children_of and win32gui.IsWindow(include_children_of):
        win32gui.EnumChildWindows(include_children_of, add, None)
    return list(seen.values())

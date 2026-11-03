"""Source-level contract for the named-parameter-link editor UI.

This checks signal wiring only; native Windows core regressions remain the
runtime authority for link validation, rebuild, persistence, and undo behavior.
"""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
UI = (ROOT / "src/ui/MainWindow.cpp").read_text(encoding="utf-8-sig")


def method_body(name):
    match = re.search(r"void MainWindow::" + re.escape(name) + r"\(\)\s*\{", UI)
    if match is None:
        raise AssertionError(f"UI method {name} is missing")
    depth = 1
    for index in range(match.end(), len(UI)):
        if UI[index] == "{":
            depth += 1
        elif UI[index] == "}":
            depth -= 1
            if depth == 0:
                return UI[match.end():index]
    raise AssertionError(f"UI method {name} has no closing brace")


class ParameterLinkEditorContracts(unittest.TestCase):
    def test_edit_menu_exposes_parameter_link_dialog(self):
        self.assertIn('"Edit named parameter links..."', UI)
        self.assertIn("&MainWindow::editParameterLinks", UI)

    def test_dialog_reads_links_and_uses_atomic_core_operations(self):
        body = method_body("editParameterLinks")
        self.assertIn("document_.parameterLinks(id)", body)
        self.assertIn("document_.setParameterLink(", body)
        self.assertIn("document_.removeParameterLink(", body)
        self.assertIn("Set / replace link", body)
        self.assertIn("Remove selected link", body)
        self.assertIn("target->currentText()", body)
        self.assertIn("source->currentText()", body)
        self.assertIn("scale->value()", body)

    def test_ui_limits_links_to_supported_named_dimensions(self):
        body = method_body("editParameterLinks")
        for name in ("width", "height", "depth", "hole_diameter", "hole_pattern_spacing"):
            with self.subTest(name=name):
                self.assertIn(f'it.key()=="{name}"', body)
        self.assertIn("dimensions.size()<2", body)


if __name__ == "__main__":
    unittest.main()

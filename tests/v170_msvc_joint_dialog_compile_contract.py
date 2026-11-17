"""Reject the Windows C3535 regression in articulated joint editor sources.

These source guards supplement; they do not replace native MSVC compilation.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

class MsvcJointEditorCompileContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')

    def test_callable_factory_is_lambda_object_not_pointer(self):
        method=self.ui.split('void MainWindow::editSavedArticulatedJointDefinition(){',1)[1].split(
            'void MainWindow::rebindSavedArticulatedJoint(){',1)[0]
        self.assertRegex(method, r'\bauto\s+make\s*=\s*\[&\]\s*\(')
        self.assertNotRegex(method, r'\bauto\s*\*\s*make\s*=')
        self.assertIn('auto* ax=make(',method)
        self.assertIn('auto* yawBox=make(',method)
        self.assertIn('auto* coordinate=make(',method)

    def test_no_lambdas_misdeclared_as_auto_pointer_in_cpp(self):
        bad=[]
        pattern=re.compile(r'\bauto\s*\*\s*[A-Za-z_]\w*\s*=\s*\[[^\]\n]*\]\s*\(')
        for path in (ROOT/'src').rglob('*.cpp'):
            for match in pattern.finditer(path.read_text(encoding='utf-8')):
                bad.append(f'{path.relative_to(ROOT)}:{match.group(0)}')
        self.assertEqual(bad,[],f'MSVC C3535 lambda declarations: {bad}')

if __name__=='__main__':
    unittest.main()

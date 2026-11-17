"""Source integration assertions for v1.70 assembly hierarchy increment.

These checks do not replace native Windows CTest or Qt GUI verification.
"""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
class AssemblyHierarchyContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header=(ROOT/'src/core/SceneDocument.h').read_text(encoding="utf-8")
        cls.parts=(ROOT/'src/core/PartsData.cpp').read_text(encoding="utf-8")
        cls.audit_h=(ROOT/'src/core/AssemblyFitAudit.h').read_text(encoding="utf-8")
        cls.audit=(ROOT/'src/core/AssemblyFitAudit.cpp').read_text(encoding="utf-8")
        cls.ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        cls.native=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding="utf-8")
        cls.fit_native=(ROOT/'tests/assembly_fit_audit_v170_tests.cpp').read_text(encoding="utf-8")
    def test_nested_mutation_and_snapshot(self):
        self.assertIn('moveAssemblySubtree(const QString& oldPath,const QString& newPath',self.header)
        for snippet in ('if(!canonicalGroupPath(oldPath,&from,error)',
                        'if(movedIndices.isEmpty())',
                        'if(movedIndices.contains(i))continue;',
                        'checkpoint();',
                        'parts_[movedIndices[i]].assemblyPath=replacementPaths[i]'):
            self.assertIn(snippet,self.parts)
        self.assertNotIn('parts_[movedIndices[i]].geometryToken=',self.parts)
    def test_invalid_destinations_fail_closed(self):
        for snippet in ('input.size()>200','input.contains("//")',
                        'segment==".."','parts.size()>32',
                        'to.startsWith(oldPrefix,Qt::CaseInsensitive)',
                        'current.startsWith(newPrefix,Qt::CaseInsensitive)'):
            self.assertIn(snippet,self.parts)
    def test_fit_reuses_exact_kernel_and_group_membership(self):
        self.assertIn('inspectAssemblyGroupFit(const SceneDocument& document',self.audit_h)
        self.assertIn('inspectAssemblyFit(document,ids,requiredClearanceMm,result,error)',self.audit)
        self.assertIn('part.assemblyPath.startsWith(prefix)',self.audit)
        self.assertIn('ids.size()<2 || ids.size()>32',self.audit)
    def test_gui_and_native_acceptance_coverage(self):
        for snippet in ('Rename / move selected assembly group',
                        'Audit selected group clearance',
                        'document_.moveAssemblySubtree(oldPath,newPath,&issue)',
                        'cad::inspectAssemblyGroupFit(document_,groupPath,clearance,&pairs,&issue)'):
            self.assertIn(snippet,self.ui)
        self.assertIn('testAssemblyHierarchySubtreeMoves()',self.native)
        self.assertIn('rejected group mutation was not atomic',self.native)
        self.assertIn('reopened hierarchy missing nested group',self.native)
        self.assertIn('Nested group must exclude other siblings',self.fit_native)
        self.assertIn('Rejected group scan must preserve previous report',self.fit_native)

if __name__=='__main__':unittest.main()

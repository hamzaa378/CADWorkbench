"""Sprint 16 integration contracts and executable rate audit coverage."""
from pathlib import Path
import unittest
import re
import subprocess
import shutil
import os
from unittest import mock
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def cpp20_compiler():
    """Return a compatible portable C++ compiler, not merely the first g++ on PATH.

    Some Windows installations have a legacy MinGW g++ that predates C++20.
    Portable test compilation is optional during a native MSVC validation;
    the mandatory CMake/MSVC and CTest gates must never be skipped instead.
    """
    seen=set()
    for name in ('g++', 'clang++'):
        compiler=shutil.which(name)
        if not compiler or compiler in seen:
            continue
        seen.add(compiler)
        try:
            probe=subprocess.run(
                [compiler, '-std=c++20', '-x', 'c++', '-fsyntax-only', '-'],
                input='#if __cplusplus < 202002L\n#error C++20 required\n#endif\nint main(){return 0;}\n',
                capture_output=True, text=True, encoding='utf-8', errors='replace',
                timeout=20,
            )
            if probe.returncode==0:
                return compiler
        except (OSError, subprocess.TimeoutExpired):
            continue
    return None

class Sprint16Readiness(unittest.TestCase):
    def test_integrated_native_api_and_menu(self):
        h=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')
        c=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
        for api in ('buildIntegratedEngineeringReadinessCsv','exportIntegratedEngineeringReadinessCsv'):
            self.assertIn(' '+api+'(',h)
            self.assertIn('SceneDocument::'+api+'(',c)
        self.assertIn('Integrated project readiness: geometry, drawings, refold and joint rates',ui)
        self.assertIn('QSaveFile saved(path)',c)
    def test_real_existing_kernels_called_in_readonly_review(self):
        c=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
        method=c.split('bool SceneDocument::buildIntegratedEngineeringReadinessCsv(',1)[1].split(
            'bool SceneDocument::exportIntegratedEngineeringReadinessCsv(',1)[0]
        for required in ('validShape(', 'drawingSheetSourceStatus(',
                         'buildSavedDrawingSvgBatch(', 'auditFoldedSheetRefoldGeometry(',
                         'cad::motion::auditKeyframeRates(', 'runDocumentAudit(*this)', 'NOT_ISSUED',
                         'MOTION_NOT_CERTIFIED', 'BLOCK', 'REVIEW'):
            self.assertIn(required,method)
        self.assertNotIn('checkpoint()',method)
    def test_atomic_multitrack_native_integration(self):
        h=(ROOT/'src/core/SceneDocument.h').read_text(encoding='utf-8')
        c=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding='utf-8')
        native=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding='utf-8')
        for name in ('previewJointMotionKeyframesBatch', 'setJointMotionKeyframesBatch',
                     'validateJointMotionKeyframesBatch'):
            self.assertIn(' '+name+'(',h)
            self.assertIn('SceneDocument::'+name+'(',c)
        method=c.split('bool SceneDocument::setJointMotionKeyframesBatch(',1)[1].split(
            'bool SceneDocument::evaluateAssemblyMotionStudy(',1)[0]
        self.assertIn('checkpoint();',method)
        self.assertIn('parts_=std::move(candidate)',method)
        self.assertIn('Retime all saved joint motion tracks (one undo)',ui)
        self.assertIn('Multi-joint motion timeline was not one undo checkpoint',native)
        self.assertIn('Coordinated motion timeline did not survive save/reopen',native)

    def test_sprint15_msvc_refold_missing_include_is_prevented(self):
        import sys
        sys.path.insert(0, str(ROOT/'tools'))
        try:
            from preflight_major_build import check_native_direct_includes
        finally:
            sys.path.pop(0)
        self.assertEqual(check_native_direct_includes(ROOT), [])
        original=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding='utf-8')
        for required in ('FoldedContourTopology.h', 'RefoldGeometryAudit.h'):
            self.assertIn(f'#include "{required}"', original)
            with tempfile.TemporaryDirectory() as directory:
                source=Path(directory)/'src/core/SceneDocument.cpp'
                source.parent.mkdir(parents=True)
                source.write_text(original.replace(f'#include "{required}"',
                                                   f'// removed "{required}" for MSVC regression fixture', 1), encoding='utf-8')
                problems=check_native_direct_includes(Path(directory))
                self.assertTrue(any(required in problem for problem in problems),
                                (required, problems))

    def test_sprint15_refold_headers_compile_without_qt(self):
        compiler=cpp20_compiler()
        if compiler is None:
            self.skipTest('No C++20-capable g++/clang++ available; Windows native MSVC gate runs separately')
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'native_refold_header_contract.cpp'
            source.write_text('#include "FoldedContourTopology.h"\n#include "RefoldGeometryAudit.h"\n#include <vector>\nint main() {\n  using cad::detail::ContourPoint;\n  std::vector<ContourPoint> contour{{0,0},{25,0},{50,0},{50,1},{25,1},{0,1}};\n  std::vector<double> straight{25,25};\n  std::vector<cad::refold::Bend> bends{{90.0,1.0}};\n  cad::refold::Audit audit{};\n  std::string issue;\n  (void)cad::refold::inspect(straight,1,5,bends,contour,&audit,&issue);\n  return 0;\n}\n', encoding='utf-8')
            build=subprocess.run([compiler,'-std=c++20','-fsyntax-only',
                                  '-I',str(ROOT/'src/core'),str(source)],
                                 capture_output=True,text=True,encoding='utf-8',errors='replace')
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)

    def test_motion_rate_compiled_portable(self):
        compiler=cpp20_compiler()
        if compiler is None:
            self.skipTest('No C++20-capable g++/clang++ available; Windows native MSVC gate runs separately')
        with tempfile.TemporaryDirectory() as directory:
            exe=Path(directory)/('rate_audit.exe' if os.name=='nt' else 'rate_audit')
            source=ROOT/'tests/joint_keyframe_rate_audit_portable_tests.cpp'
            build=subprocess.run([compiler,'-std=c++20','-O0',str(source),'-o',str(exe)],
                                 text=True,capture_output=True,encoding='utf-8',errors='replace')
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],text=True,capture_output=True,encoding='utf-8',errors='replace')
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('18015 keyframe-rate audit assertions',run.stdout)
    def test_windows_legacy_cpp_compiler_fallback(self):
        """A gcc too old for -std=c++20 cannot block modern clang++ discovery."""
        with mock.patch('shutil.which', side_effect=lambda name: {'g++': '/old/g++', 'clang++': '/new/clang++'}[name]), \
             mock.patch('subprocess.run') as run:
            run.side_effect = [mock.Mock(returncode=1), mock.Mock(returncode=0)]
            self.assertEqual(cpp20_compiler(), '/new/clang++')
            self.assertEqual(run.call_count, 2)
            self.assertTrue(all('-std=c++20' in call.args[0] for call in run.call_args_list))

    def test_windows_legacy_compiler_returns_none(self):
        """Older MinGW alone should not incorrectly be treated as a C++20 compiler."""
        with mock.patch('shutil.which', side_effect=lambda name: '/old/g++' if name=='g++' else None), \
             mock.patch('subprocess.run', return_value=mock.Mock(returncode=1)):
            self.assertIsNone(cpp20_compiler())

    def test_utf8_source_contract_explicit(self):
        """Source contract reads must not depend on Windows cp1252 locale."""
        source=(ROOT/'tests/test_sprint16_integrated_readiness.py').read_text(encoding='utf-8')
        self.assertFalse(re.search(r'\.read_text\(\s*\)', source))
        self.assertIn(".read_text(encoding='utf-8')", source)

    def test_native_regression_and_inventory(self):
        text=(ROOT/'tests/document_audit_tests.cpp').read_text(encoding='utf-8')
        self.assertIn('Integrated readiness failed cross-domain native preflight',text)
        self.assertIn('Readiness audit failed to block a stale drawing source',text)
        self.assertIn('Native integrated readiness missed a persisted prismatic joint-rate bound',
                      (ROOT/'tests/cadcore_tests.cpp').read_text(encoding='utf-8'))
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertEqual(len(re.findall(r'add_test\s*\(\s*NAME\s+(\w+)',cmake)),130)
    def test_sprint16_targeted_native_build_covers_all_selected_ctests(self):
        """A CTest filter must never reference an unbuilt native executable.

        The user's November 18 Windows run built CADWorkbench.exe but the
        focused cadcore_v170_articulated_closed_loop test was NOT_RUN because
        articulated_assembly_v170_tests.exe was absent. Catch this at preflight.
        """
        launcher=(ROOT/'RUN_SPRINT16_WINDOWS_TARGETED_BUILD.ps1').read_text(encoding='utf-8')
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        target_loop=re.search(r'foreach\(\$target\s+in\s+@\((.*?)\)\s*\)',launcher,re.S|re.I)
        self.assertIsNotNone(target_loop,'No targeted native executable build list')
        build_targets=set(re.findall(r"'([^']+)'",target_loop.group(1)))
        focused_match=re.search(r"-R\s+'\^\(([^']+)\)\$'",launcher)
        self.assertIsNotNone(focused_match,'No focused CTest selection found')
        cmake_ctests=dict(re.findall(
            r'add_test\s*\(\s*NAME\s+(\w+)\s+COMMAND\s+(\w+)',
            cmake,re.I))
        selected=focused_match.group(1).split('|')
        self.assertEqual(len(selected),4)
        for ctest_name in selected:
            self.assertIn(ctest_name,cmake_ctests,
                          'Focused CTest is not registered in CMakeLists.txt')
            self.assertIn(cmake_ctests[ctest_name],build_targets,
                          f'{ctest_name} requires missing build target {cmake_ctests[ctest_name]}')
        self.assertIn('articulated_assembly_v170_tests',build_targets)

    def test_portable_registration(self):
        self.assertIn('joint_keyframe_rate_audit_portable_tests.cpp',
                      (ROOT/'tools/run_portable_regressions.py').read_text(encoding='utf-8'))
        self.assertIn('joint_keyframe_rate_audit_portable_tests.cpp',
                      (ROOT/'scripts/verify_portable_consolidation.sh').read_text(encoding='utf-8'))

if __name__=='__main__':unittest.main()

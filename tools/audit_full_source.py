#!/usr/bin/env python3
"""Full-tree static source-integrity inventory (NOT a native compiler or proof of correct geometry).

Checks every shipped code file for corruption/conflict markers; checks portable
and CMake registrations, path uniqueness on Windows and unchanged native CTest
count. This audit is intentionally independent of the generated hash manifest
so it can run during a coding session before final packaging.
"""
from __future__ import annotations
import argparse
import ast
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
EXTENSIONS={'.cpp','.h','.hpp','.py','.ps1','.cmake'}
AREAS=('src','tests','tools','scripts')
PORTABLE_SCRIPT='tools/run_portable_regressions.py'
REQUIRED_CORE={
    'src/core/FeatureRebuildDiagnostics.cpp',
    'src/core/ShopToleranceAllocation.h',
    'src/core/MotionSweptClearance.h',
    'src/core/BranchReferenceAgreement.h',
    'src/core/DrawingReferenceAudit.h',
    'src/core/ReferenceRouteConsensus.h',
    'src/core/AtomicRebuildGuard.h',
}

def inspect(root:Path)->dict:
    problems=[]
    metrics={'files_scanned':0,'lines_scanned':0,'cpp_files':0,'python_files':0,'powershell_files':0,
             'portable_regressions':0,'native_ctest_registrations':0,'largest_cpp_file':None,
             'largest_cpp_lines':0}
    entries=[]
    for area in AREAS:
        folder=root/area
        if not folder.is_dir():
            problems.append(f'Missing source directory: {area}')
            continue
        entries.extend(p for p in folder.rglob('*') if p.is_file() and p.suffix.lower() in EXTENSIONS)
    if (root/'CMakeLists.txt').is_file():entries.append(root/'CMakeLists.txt')
    else:problems.append('CMakeLists.txt missing')
    seen={}
    for path in sorted(entries):
        rel=path.relative_to(root).as_posix()
        folded=rel.casefold()
        if folded in seen and seen[folded]!=rel:problems.append(f'Windows path case collision: {seen[folded]} / {rel}')
        seen[folded]=rel
        try: raw=path.read_bytes();data=raw.decode('utf-8')
        except UnicodeError:
            problems.append(f'Non-UTF8 source: {rel}')
            continue
        if b'\x00' in raw:problems.append(f'NUL byte in source: {rel}')
        if re.search(r'^(?:<<<<<<< |>>>>>>> |=======\s*$)',data,re.MULTILINE):
            problems.append(f'Unresolved merge marker: {rel}')
        line_count=data.count('\n')+(0 if data.endswith('\n') else 1)
        metrics['files_scanned']+=1
        metrics['lines_scanned']+=line_count
        if path.suffix=='.cpp':
            metrics['cpp_files']+=1
            if line_count>metrics['largest_cpp_lines']:
                metrics['largest_cpp_lines']=line_count;metrics['largest_cpp_file']=rel
        if path.suffix=='.py':
            metrics['python_files']+=1
            # Pre-build gate: check *all* Python files, not just the runner.
            # The Qt/MSVC build is too expensive to start with a syntax error
            # already present in an unrelated acceptance or packaging script.
            try:ast.parse(data,filename=rel)
            except SyntaxError as exc:problems.append(f'Python syntax error in {rel}:{exc.lineno}: {exc.msg}')
        if path.suffix=='.ps1':metrics['powershell_files']+=1
    cmake=root/'CMakeLists.txt'
    cmake_text=cmake.read_text(encoding='utf-8') if cmake.is_file() else ''
    for rel in sorted(REQUIRED_CORE):
        if not (root/rel).is_file():problems.append(f'Missing engineering source: {rel}')
        elif rel not in cmake_text:problems.append(f'Missing CMake source registration: {rel}')
    motion=(root/'src/core/SceneDocument.cpp')
    if motion.is_file():
        scene=motion.read_text(encoding='utf-8')
        for token in ('betweenPoses.observe(fraction,simultaneous',
                      'tracked.append(liveArticulatedFixed_)',
                      'sweptMinimum<=requiredClearanceMm',
                      'UNSUPPORTED_ROTATION'):
            if token not in scene:problems.append(f'Missing strict workcell motion safety wiring: {token}')
    # Cross-check explicit native sources, but ignore variable-based CMake paths.
    for rel in set(re.findall(r'(?<![A-Za-z0-9_])(?:src|tests)/[A-Za-z0-9_./-]+\.(?:cpp|h|hpp)\b',cmake_text)):
        if not (root/rel).is_file():problems.append(f'CMake references missing source: {rel}')
    ctest_names=re.findall(r'add_test\s*\(\s*NAME\s+([A-Za-z0-9_]+)',cmake_text)
    metrics['native_ctest_registrations']=len(ctest_names)
    legacy_file=root/'tests/BASELINE_130_NATIVE_CTEST_NAMES.txt'
    legacy=legacy_file.read_text(encoding='utf-8').splitlines() if legacy_file.is_file() else []
    if len(legacy)!=130 or len(set(legacy))!=130:
        problems.append('Missing or corrupted historical 130-native-test baseline')
    if len(ctest_names)!=130 or len(set(ctest_names))!=len(ctest_names) or not set(legacy).issubset(ctest_names):
        problems.append('Expected all 130 historical unique native CTest registrations')
    registry=root/PORTABLE_SCRIPT
    try:
        tree=ast.parse(registry.read_text(encoding='utf-8'),filename=str(registry))
        assignments=[node.value for node in tree.body if isinstance(node,ast.Assign)
                     and any(isinstance(t,ast.Name) and t.id=='SOURCES' for t in node.targets)]
        portable=list(ast.literal_eval(assignments[0])) if len(assignments)==1 else []
        metrics['portable_regressions']=len(portable)
        if len(portable)!=len(set(portable)) or len(portable)<35:
            problems.append('Portable registry duplicates or missing new regression executables')
        for name in portable:
            if not re.fullmatch(r'[A-Za-z0-9_]+\.cpp',name) or not (root/'tests'/name).is_file():
                problems.append(f'Unsafe/missing registered portable source: {name}')
        for name in ('geometry_numeric_stability_portable_tests.cpp',
                     'shop_tolerance_allocation_portable_tests.cpp',
                     'motion_swept_clearance_portable_tests.cpp'):
            if name not in portable:problems.append(f'Sprint 4 regression missing from portable registry: {name}')
        for name in ('reference_route_consensus_portable_tests.cpp',
                     'atomic_rebuild_guard_portable_tests.cpp'):
            if name not in portable:problems.append(f'Sprint 6 regression missing from portable registry: {name}')
    except (OSError,SyntaxError,IndexError,ValueError,TypeError) as exc:
        problems.append(f'Cannot parse portable test registry: {exc}')
    metrics['warnings']=[
        'This audit is STATIC only: Windows MSVC/Qt/OCCT compile, 130 native CTests and manual acceptance remain independent gates.',
        'A large GUI translation unit remains intentionally unchanged under the UI feature freeze.',
    ]
    return {'status':'PASS_STATIC_SOURCE_INTEGRITY' if not problems else 'FAIL_STATIC_SOURCE_INTEGRITY',
            **metrics,'problems':problems}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--report',type=Path)
    args=parser.parse_args()
    result=inspect(args.root.resolve())
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))
    return 0 if not result['problems'] else 1
if __name__=='__main__':raise SystemExit(main())

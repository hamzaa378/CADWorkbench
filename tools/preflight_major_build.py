#!/usr/bin/env python3
"""Fast source/registration preflight before Windows CMake + 130 native CTests.

Not a replacement for compilation, CTest or engineering GUI acceptance.
"""
from pathlib import Path
import argparse
import hashlib
import re
import sys


def check_native_direct_includes(root: Path) -> list[str]:
    """Prevent missing direct headers for native code not compiled by portable suites.

    Sprint 15 passed the portable suites but failed MSVC at SceneDocument.cpp
    because SheetMetal.cpp included the refold headers while SceneDocument.cpp
    did not. Transitive includes are intentionally NOT accepted by this guard.
    """
    scene = root / 'src/core/SceneDocument.cpp'
    if not scene.is_file():
        return ['Missing SceneDocument.cpp for native direct-include check']
    code = scene.read_text(encoding='utf-8')
    includes = set(re.findall(r'(?m)^\s*#\s*include\s*"([^"]+)"', code))
    requirements = (
        ('detail::ContourPoint', 'FoldedContourTopology.h'),
        ('refold::Bend', 'RefoldGeometryAudit.h'),
        ('refold::Audit', 'RefoldGeometryAudit.h'),
        ('refold::inspect(', 'RefoldGeometryAudit.h'),
    )
    missing = []
    for symbol, header in requirements:
        if symbol in code and header not in includes:
            missing.append(f'MSVC C2065/C2653 risk: {symbol} requires a direct #include "{header}" in SceneDocument.cpp')
    return missing


def check(root: Path) -> list[str]:
    errors=check_native_direct_includes(root)
    native = root/'tests/associative_features_v159_tests.cpp'
    if not native.is_file():
        return [f'Missing native regression source: {native}']
    data=native.read_text(encoding='utf-8')
    if re.search(r'\bint\s+faceIndex\s*=\s*0\s*,\s*topIndex', data) and re.search(r'\bint\s+faceIndex\s*=\s*0\s*;', data):
        errors.append('Known MSVC C2374/C2086 regression: duplicate faceIndex declaration')
    # The constructor is intentionally large; reject the real Windows MSVC
    # C2374/C2086 redeclaration that escaped portable builds in Nov 2026.
    main_window=root/'src/ui/MainWindow.cpp'
    if not main_window.is_file():
        errors.append('Missing main window source')
    else:
        ui=main_window.read_text(encoding='utf-8')
        declarations=re.findall(r'\bauto\s*\*\s*contextGrid\s*=',ui)
        if len(declarations)!=1:
            errors.append('MSVC C2374/C2086 risk: contextGrid must be declared exactly once in MainWindow constructor')
    manifests=[root/'SHA256SUMS.txt', root/'SOURCE_MANIFEST_SHA256.txt']
    if any(not mf.is_file() for mf in manifests):
        errors.append('Missing SHA-256 manifest')
    else:
        if manifests[0].read_bytes()!=manifests[1].read_bytes():
            errors.append('Conflicting source manifests')
        else:
            for line in manifests[0].read_text(encoding='utf-8').splitlines():
                if not line.strip():continue
                match=re.fullmatch(r'([a-f0-9]{64})  ([^\\\r\n]+)', line)
                if not match or '..' in Path(match.group(2)).parts or Path(match.group(2)).is_absolute():
                    errors.append('Invalid source manifest line'); continue
                source=root/match.group(2)
                if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=match.group(1):
                    errors.append('Hash mismatch: '+match.group(2))
    ctest = (root/'CMakeLists.txt').read_text(encoding='utf-8') if (root/'CMakeLists.txt').is_file() else ''
    if not re.search(r'target_compile_options\(CADWorkbench\s+PRIVATE[^)]*/bigobj',ctest):
        errors.append('MSVC C1128 risk: CADWorkbench target needs /bigobj for MainWindow.cpp')
    tree = (root/'src/core/ShapeFeatureTree.cpp').read_text(encoding='utf-8') if (root/'src/core/ShapeFeatureTree.cpp').is_file() else ''
    # Selected-edge groups have a separate per-member machining size. A size
    # is never a geometric anchor key; this caught CTest #45 in Sprint 2.
    if tree.count('anchor.remove("size");') != 2 or tree.count('fillet->Add(memberSize,') != 1 or tree.count('chamfer->Add(memberSize,') != 1:
        errors.append('Grouped blend regression: per-member size leaked into strict edge anchor or was not applied')
    if 'FeatureRebuildDiagnostics.cpp' not in ctest:
        errors.append('Missing rebuild diagnostics library registration')
    if 'associative_features_v159_tests' not in ctest:
        errors.append('Missing associative-feature native test target')
    return errors


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    args=parser.parse_args()
    errors=check(args.root)
    for msg in errors: print('FAIL:',msg,file=sys.stderr)
    if errors:return 1
    print('PASS: manifest + known MSVC redeclaration preflight (native build still required)')
    return 0

if __name__=='__main__':raise SystemExit(main())

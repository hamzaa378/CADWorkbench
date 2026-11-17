#!/usr/bin/env python3
"""Offline, fail-closed Windows build/CTest/Python diagnostic classifier.

Read the complete transcript of RUN_V170_COMPREHENSIVE_AUDIT.ps1, including
stderr. It is NOT a substitute for the audit's signed/hashed evidence or native
build. Never infer passing native tests from synthetic fixtures embedded in the
Python test log. JSON issues are machine-readable and line-addressable.
"""
import argparse
import json
from pathlib import Path
import re
import sys
from datetime import datetime, timezone

PROBLEMS = (
    ('MSVC_C1128', re.compile(r'error C1128\b'), 'Add target-scoped /bigobj for large MSVC translation units; consider splitting the unit.'),
    ('MSVC_REDECLARATION', re.compile(r'error C(?:2374|2086|2371)\b'), 'Fix duplicate declarations within the same C++ scope.'),
    ('MSVC_COMPILER_INTERNAL', re.compile(r'error C1001\b'), 'Isolate the translation unit; reduce optimization or compiler complexity.'),
    ('MSVC_HEAP_EXHAUSTED', re.compile(r'error C1060\b'), 'Reduce parallel builds or split the translation unit.'),
    ('MSVC_OTHER', re.compile(r'\berror C\d{4}\b'), 'Inspect the compiler location and fix C++ source; do not skip build.'),
    ('LINK_FAILURE', re.compile(r'(?:fatal error LNK\d+|\berror LNK\d+)'), 'Inspect unresolved symbols, library linkage, and ABI compatibility.'),
    ('CMAKE_ERROR', re.compile(r'CMake Error(?: at|:)'), 'Resolve CMake configuration/dependency errors.'),
    ('GROUPED_EDGE_BUILD_FAILURE', re.compile(r'Witnessed unequal-size (?:two-edge blend did not rebuild|group failed)'), 'Inspect size/edge witness field separation and reproduce cadcore_universal_feature_tree on Windows; keep failing operation and full kernel diagnostic.'),
    ('CTEST_FAILED', re.compile(r'\*\*\*Failed|\*\*\*Timeout|\btests FAILED:'), 'Reproduce the named native CTest and retain its failure output.'),
    ('PYTHON_FAILED', re.compile(r'^(?:FAILED \(|ERROR:|FAIL:)'), 'Inspect the exact Python failure and isolate its test.'),
)

def diagnose(text: str, expected_native: int=130, expected_python: int=781) -> dict:
    issues=[]
    lines=text.splitlines()
    seen=set()
    for number,line in enumerate(lines,1):
        for code,pattern,hint in PROBLEMS:
            if pattern.search(line):
                key=(code,number)
                if key not in seen:
                    issues.append({'code':code,'line':number,'message':line.strip()[:400],'recommendation':hint})
                    seen.add(key)
                # A compiler diagnostic should not also be called generic.
                if code!='MSVC_OTHER':
                    break
    # Native 130/130 lines in Python audit-fixture output are not CTest evidence.
    # A CTest pass must be matched by its full summary, and cannot override a
    # native build-failure marker in the same final audit.
    summaries=re.findall(r'(?mi)^\s*100%\s+tests passed(?:,\s*0 tests failed)?\s+out of\s+(\d+)\s*$',text)
    native_complete=any(int(n)==expected_native for n in summaries)
    native_blocked=bool(re.search(r'Native tests NOT RUN|Native build error:|Windows Release build failed',text,re.I))
    native_failed=bool(re.search(r'\b\d+% tests passed,\s*[1-9]\d* tests failed out of',text))
    python_success=re.findall(r'Ran\s+(\d+)\s+tests?\s+in\s+[\d.]+s\s*\n\s*OK',text)
    python_complete=any(int(n)>=expected_python for n in python_success)
    audit_success=bool(re.search(r'Automated verification PASSED|Automated verification succeeded|Automated and recorded acceptance gates passed',text,re.I))
    audit_failed=bool(re.search(r'Automated verification FAILED',text,re.I))
    if native_blocked:
        issues.append({'code':'NATIVE_NOT_RUN','line':None,'message':'Build failure or explicit native NOT RUN marker','recommendation':'Fix compile/link failure and rerun all native CTests.'})
    if native_failed:
        issues.append({'code':'NATIVE_TEST_FAILURE','line':None,'message':'At least one real native CTest failed','recommendation':'Reproduce failing native tests; do not claim a complete gate.'})
    if not native_complete:
        issues.append({'code':'NATIVE_EVIDENCE_MISSING','line':None,'message':f'No complete {expected_native}/{expected_native} CTest summary','recommendation':'Capture the full CTest output after a successful build.'})
    if not python_complete:
        issues.append({'code':'PYTHON_EVIDENCE_MISSING','line':None,'message':f'No complete passing Python inventory of at least {expected_python} tests','recommendation':'Run the full unittest inventory and retain its exit status.'})
    if audit_failed:
        issues.append({'code':'AUDIT_FAILED','line':None,'message':'Full audit explicitly reported FAILED','recommendation':'Address build, CTest and Python failures.'})
    compiler_errors=any(re.search(r'(?:error C\d{4}|(?:fatal )?error LNK\d+|CMake Error)',ln) for ln in lines)
    pass_gate=(native_complete and python_complete and audit_success and not
               (native_blocked or native_failed or audit_failed or compiler_errors or
                any(i['code'] in ('CTEST_FAILED','PYTHON_FAILED') for i in issues)))
    return {'schema':1,'status':'AUTOMATED_PASS' if pass_gate else 'INCOMPLETE_OR_FAILED',
            'native_ctest_complete':native_complete and not native_blocked and not native_failed,
            'python_inventory_complete':python_complete,
            'full_audit_success_explicit':audit_success and not audit_failed,
            'manual_acceptance':'NOT_ASSESSED', 'issues':issues}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log',required=True,type=Path)
    ap.add_argument('--out',type=Path)
    ap.add_argument('--expected-native',type=int,default=130)
    ap.add_argument('--expected-python',type=int,default=781)
    args=ap.parse_args()
    if args.expected_native<=0 or args.expected_python<=0:
        ap.error('Expected test counts must be positive')
    if not args.log.is_file():
        ap.error('Log file not found: '+str(args.log))
    result=diagnose(args.log.read_text(encoding='utf8',errors='replace'),args.expected_native,args.expected_python)
    result['log_file']=args.log.name
    result['created_utc']=datetime.now(timezone.utc).isoformat()
    if args.out:
        args.out.parent.mkdir(parents=True,exist_ok=True)
        args.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(result['status'])
    for i in result['issues']:
        print(f"{i['code']} line {i['line']}: {i['message']}\n  {i['recommendation']}")
    return 0 if result['status']=='AUTOMATED_PASS' else 1

if __name__=='__main__':
    raise SystemExit(main())

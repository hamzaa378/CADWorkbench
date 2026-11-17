#!/usr/bin/env python3
"""Safe two-hour focused coding sessions for CADWorkbench.

start: baseline source integrity, record a clean source snapshot.
status: display changes without touching sources.
stage: freeze a candidate by regenerating BOTH source manifests before the full audit.
verify: fail if the staged candidate changed during or after the full audit.
finish: require 130/130 Windows CTests and full Python/source regression evidence;
        records completion but NEVER claims manual engineering acceptance.

Only run `stage` after coding. The normal comprehensive audit remains strict:
there is no flag to skip tests, ignore errors, or bypass the source manifest.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time

IGNORED_DIRS={'build','out','vcpkg_installed','.git','__pycache__','.pytest_cache','.mypy_cache','.venv','.dev_session'}
EXCLUDED_FILES={'SHA256SUMS.txt','SOURCE_MANIFEST_SHA256.txt'}
SOURCE_EXTENSIONS={'.cpp','.cc','.c','.h','.hpp','.py','.ps1','.md','.txt','.json','.csv','.yml','.yaml','.sh','.cmake','.ui','.qrc','.xml','.ini','.svg','.gitignore','.gitattributes'}


def digest(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def timestamp()->str:
    return datetime.now(timezone.utc).isoformat(timespec='seconds')


def scanner(root:Path)->dict[str,str]:
    # os.walk permits pruning .venv/build before enumerating their huge trees.
    records={}
    for folder, dirs, names in os.walk(root,followlinks=False):
        path=Path(folder)
        dirs[:]=[name for name in dirs if name not in IGNORED_DIRS
                and not name.startswith('.venv')
                and not name.startswith('cmake-build')]
        for name in names:
            if name in EXCLUDED_FILES:
                continue
            if name not in ('.gitattributes','.gitignore') and Path(name).suffix not in SOURCE_EXTENSIONS:
                continue
            f=path/name
            rel=f.relative_to(root)
            if f.is_symlink():
                raise ValueError(f'Refusing source symlink: {rel}')
            if not f.is_file():
                continue
            key=rel.as_posix()
            if '\n' in key or '\r' in key or '\\' in key:
                raise ValueError('Unsafe manifest pathname: '+key)
            records[key]=digest(f)
    return dict(sorted(records.items()))


def manifest_text(mapping:dict[str,str])->str:
    return ''.join(f'{value}  {name}\n' for name,value in sorted(mapping.items()))


def load_manifest(root:Path)->dict[str,str]:
    f=root/'SHA256SUMS.txt'
    other=root/'SOURCE_MANIFEST_SHA256.txt'
    if not f.is_file() or not other.is_file() or f.read_bytes()!=other.read_bytes():
        raise ValueError('Missing or inconsistent source manifests')
    out={}
    for line in f.read_text(encoding='utf8').splitlines():
        if not line.strip():continue
        m=re.fullmatch(r'([a-f0-9]{64})  ([^\r\n]+)',line)
        if not m:
            raise ValueError('Malformed manifest record')
        k=m.group(2)
        p=Path(k)
        if p.is_absolute() or '..' in p.parts or '\\' in k or k in out:
            raise ValueError('Unsafe or duplicate manifest path: '+k)
        out[k]=m.group(1)
    return out


def check_manifest(root:Path)->dict[str,str]:
    recorded=load_manifest(root)
    current=scanner(root)
    if current!=dict(sorted(recorded.items())):
        missing=sorted(set(recorded)-set(current))
        new=sorted(set(current)-set(recorded))
        altered=sorted(k for k in set(recorded)&set(current) if recorded[k]!=current[k])
        raise ValueError(f'Source manifest mismatch: missing={missing[:5]} new={new[:5]} changed={altered[:5]}')
    return current


def state_path(root:Path)->Path:
    return root/'build/dev_sessions/active_session.json'


def write_json(path:Path,content:dict)->None:
    path.parent.mkdir(parents=True,exist_ok=True)
    tmp=path.with_suffix('.staging')
    if tmp.exists():raise ValueError('Previous staging file exists: '+str(tmp))
    tmp.write_text(json.dumps(content,indent=2,sort_keys=True)+'\n',encoding='utf8')
    tmp.replace(path)


def session(root:Path)->dict:
    path=state_path(root)
    if not path.is_file():
        raise ValueError('No active session. Run start first.')
    data=json.loads(path.read_text(encoding='utf8'))
    if data.get('project_root')!=str(root.resolve()):
        raise ValueError('Session belongs to another project root')
    if data.get('status')!='active':
        raise ValueError('Session is no longer active')
    return data


def changes(baseline:dict[str,str],current:dict[str,str])->dict[str,list[str]]:
    return {'added':sorted(set(current)-set(baseline)),
            'modified':sorted(k for k in current.keys()&baseline.keys() if current[k]!=baseline[k]),
            'deleted':sorted(set(baseline)-set(current))}


def ensure_frontend_frozen(delta:dict[str,list[str]])->None:
    ui=[name for files in delta.values() for name in files if name.startswith('src/ui/')]
    if ui:
        raise ValueError('Frontend freeze violated during backend development session: '+', '.join(ui[:8]))


def start(root:Path,label:str)->None:
    if state_path(root).exists():
        raise ValueError('A session state already exists; finish it before starting another')
    initial=check_manifest(root)
    state={'schema':1,'status':'active','project_root':str(root.resolve()),
           'label':label,'started_utc':timestamp(),'baseline':initial,
           'baseline_manifest_sha256':digest(root/'SHA256SUMS.txt'),
           'staged':None}
    write_json(state_path(root),state)
    print(f'PASS: clean baseline recorded ({len(initial)} source files).')
    print('CODE PHASE: make backend changes without running the full regression suite yet.')
    print('When ready, run stage followed by the comprehensive Windows audit.')


def stage(root:Path)->None:
    data=session(root)
    current_manifest=digest(root/'SHA256SUMS.txt')
    allowed={data['baseline_manifest_sha256']}
    if data['staged']:
        allowed.add(data['staged']['manifest_sha256'])
    if current_manifest not in allowed:
        raise ValueError('Manifest was externally modified during session')
    current=scanner(root)
    delta=changes(data['baseline'],current)
    ensure_frontend_frozen(delta)
    # Deliberately rewrite both manifests in lock-step for legitimate edits.
    # This is not a release signoff; the mandatory full audit follows.
    manifest=manifest_text(current)
    for filename in sorted(EXCLUDED_FILES):
        (root/filename).write_text(manifest,encoding='utf8',newline='\n')
    check_manifest(root)
    data['staged']={'at_utc':timestamp(),'at_unix':time.time(),
                    'files':current,'delta':delta,
                    'manifest_sha256':digest(root/'SHA256SUMS.txt')}
    write_json(state_path(root),data)
    print(f'PASS: staged {len(current)} files: {len(delta["added"])} added, {len(delta["modified"])} modified, {len(delta["deleted"])} deleted.')
    print('NEXT: run all 130 native CTests and the complete Python/source suite using RUN_V170_COMPREHENSIVE_AUDIT.ps1')


def verify(root:Path)->dict:
    data=session(root)
    if not data.get('staged'):
        raise ValueError('No staged snapshot: run stage after coding')
    if digest(root/'SHA256SUMS.txt')!=data['staged']['manifest_sha256']:
        raise ValueError('Manifests changed since staging')
    current=check_manifest(root)
    if current!=data['staged']['files']:
        raise ValueError('Source changed since staging. Restage, then rerun all regressions.')
    ensure_frontend_frozen(changes(data['baseline'],current))
    print(f'PASS: {len(current)} staged source files unchanged')
    return data


def finish(root:Path,audit_exit_code:int)->None:
    data=verify(root)
    if audit_exit_code not in (0,3):
        raise ValueError('Automated audit did not succeed: exit code must be 0 or 3')
    qa=root/'build/v170_comprehensive_audit'
    audit=qa/'v170_full_audit.json'
    ctest=qa/'ctest_output.log'
    python=qa/'python_source_tests.log'
    for path in (audit,ctest,python):
        if not path.is_file() or path.stat().st_mtime+2 < data['staged']['at_unix']:
            raise ValueError('Missing or stale post-stage regression evidence: '+str(path))
    results=json.loads(audit.read_text(encoding='utf-8-sig'))
    if not results.get('native_complete') or results.get('problems'):
        raise ValueError('130 native test evidence incomplete or invalid')
    ctest_text=ctest.read_text(encoding='utf8',errors='replace')
    if not re.search(r'100% tests passed,\s*0 tests failed out of 130',ctest_text):
        raise ValueError('130/130 CTest result not present in transcript')
    pytext=python.read_text(encoding='utf8',errors='replace')
    m=re.search(r'Ran\s+(\d+)\s+tests?\s+in',pytext)
    if not m or int(m.group(1))<771 or not re.search(r'(?m)^OK\s*$',pytext) or re.search(r'(?m)^FAILED\s*\(',pytext):
        raise ValueError('Complete passing Python test evidence not present (minimum 771 tests; full discovery must be run)')
    data['status']='completed'
    data['finished_utc']=timestamp()
    data['automation_verified']=True
    data['manual_acceptance_satisfied']=bool(results.get('release_gate_evidence_satisfied'))
    data['audit_exit_code']=audit_exit_code
    completed=state_path(root).parent/'completed_session.json'
    write_json(completed,data)
    state_path(root).unlink()
    summary=state_path(root).parent/'SESSION_REPORT.md'
    delta=data['staged']['delta']
    text=f"""# CADWorkbench development session: {data['label']}

- Started (UTC): {data['started_utc']}
- Finished (UTC): {data['finished_utc']}
- Source changes: {len(delta['added'])} added, {len(delta['modified'])} modified, {len(delta['deleted'])} deleted
- Native CTests: 130/130 verified from this Windows audit
- Python/source inventory: full passing suite verified
- Engineering/manual acceptance: {'recorded gate satisfied' if data['manual_acceptance_satisfied'] else 'PENDING / NOT CLAIMED'}
- Audit exit: {audit_exit_code}

## Changed files
"""+''.join(f'- {kind}: {name}\n' for kind, names in delta.items() for name in names)
    summary.write_text(text,encoding='utf8')
    print('PASS: automated 130-native + full-Python post-coding audit verified.')
    if not data['manual_acceptance_satisfied']:
        print('NOT A FINAL RELEASE: manual engineering acceptance remains pending.')
    print('Session evidence: '+str(summary))


def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('command',choices=['start','status','stage','verify','finish'])
    ap.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    ap.add_argument('--label',default='Backend engineering focused coding session')
    ap.add_argument('--audit-exit-code',type=int)
    args=ap.parse_args()
    root=args.root.resolve()
    try:
        if args.command=='start':start(root,args.label)
        elif args.command=='stage':stage(root)
        elif args.command=='verify':verify(root)
        elif args.command=='finish':
            if args.audit_exit_code is None:raise ValueError('--audit-exit-code is required for finish')
            finish(root,args.audit_exit_code)
        else:
            data=session(root)
            delta=changes(data['baseline'],scanner(root))
            print(f'Active coding session: {data["label"]}; started {data["started_utc"]}')
            for kind, names in delta.items():
                print(f'{kind.upper()} ({len(names)})')
                for name in names[:50]:print('  '+name)
        return 0
    except (OSError,ValueError,KeyError,json.JSONDecodeError) as exc:
        print('BLOCKED: '+str(exc),file=sys.stderr)
        return 2

if __name__=='__main__':raise SystemExit(main())

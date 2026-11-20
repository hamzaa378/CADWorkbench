#!/usr/bin/env python3
"""Verify checksums of an external CADWorkbench UNISSUED evidence packet.

Checksums detect change; they DO NOT authenticate the inspector, approve a CAD
revision, verify datum alignment, or authorize manufacture.
"""
from pathlib import Path
import argparse
import json
import sys
from engineering_evidence_bundle import verify_bundle

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('folder',type=Path)
    a=p.parse_args()
    try:
        m=verify_bundle(a.folder)
        print(json.dumps({'status':m['status'],'release_approved':False,
                          'files_verified':len(m['files']),
                          'open_action_count':m['open_action_count']},indent=2))
        return 0
    except (OSError,ValueError,TypeError,KeyError,UnicodeError) as exc:
        print(f'FAIL: {exc}',file=sys.stderr)
        return 1
if __name__=='__main__':raise SystemExit(main())

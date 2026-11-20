#!/usr/bin/env python3
"""Compare two checksum-verified UNISSUED drawing exports by owner identity.

Detects added, removed, renamed, changed-revision and changed-preview sheets.
A matching hash never establishes geometric, dimensional or GD&T correctness.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys

from verify_unissued_drawings import verify
from review_manufacturing_handoff import safe_output_path


def _inventory(folder: Path) -> dict:
    verify(folder)
    with (folder/'drawing_index.csv').open(encoding='utf-8-sig',newline='') as stream:
        rows=list(csv.DictReader(stream))
    result={}
    numbers=set()
    for row in rows:
        owner=row['sheet_owner_id']
        number=row['drawing_number']
        if not owner or len(owner)>256 or not number.strip() or number in numbers or owner in result:
            raise ValueError('Drawing revision comparison requires unique nonblank owners and drawing numbers')
        result[owner]=row
        numbers.add(number)
    return result


def compare_drawings(previous: Path,current: Path) -> dict:
    previous,current=Path(previous),Path(current)
    old,new=_inventory(previous),_inventory(current)
    result=[]
    for owner in sorted(set(old)|set(new)):
        if owner not in old:
            code='DRAWING_ADDED'
            fields=[]
        elif owner not in new:
            code='DRAWING_REMOVED'
            fields=[]
        else:
            fields=[field for field in ('drawing_number','current_geometry_token','svg_sha256','pdf_sha256','pdf_file')
                    if old[owner][field]!=new[owner][field]]
            code='DRAWING_CHANGED' if fields else 'DRAWING_REFERENCE_MATCH'
        result.append({'owner':owner,'code':code,'changed_fields':fields,
                       'previous_number':old.get(owner,{}).get('drawing_number'),
                       'current_number':new.get(owner,{}).get('drawing_number'),
                       'review_required':code!='DRAWING_REFERENCE_MATCH'})
    return {'status':'UNISSUED_DRAWING_REVISION_REVIEW_REQUIRED',
            'release_approved':False,
            'previous_index_sha256':hashlib.sha256((previous/'drawing_index.csv').read_bytes()).hexdigest(),
            'current_index_sha256':hashlib.sha256((current/'drawing_index.csv').read_bytes()).hexdigest(),
            'changed_count':sum(x['review_required'] for x in result),
            'previous_count':len(old),'current_count':len(new),
            'sheets':result,
            'caution':'Equal preview hashes do not establish dimension correctness or release approval.'}


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('previous',type=Path)
    parser.add_argument('current',type=Path)
    parser.add_argument('--json',type=Path)
    a=parser.parse_args()
    try:
        if a.json:
            out=safe_output_path(safe_output_path(a.json,a.previous),a.current)
            if out.exists() or not out.parent.is_dir():raise ValueError('Output must be a new external file')
        result=compare_drawings(a.previous,a.current)
        data=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
        if a.json:out.open('x',encoding='utf-8').write(data)
        print(data,end='')
        return 0
    except (OSError,ValueError,TypeError,KeyError,UnicodeError) as exc:
        print(f'FAIL: {exc}',file=sys.stderr)
        return 1
if __name__=='__main__':raise SystemExit(main())

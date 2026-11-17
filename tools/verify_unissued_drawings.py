#!/usr/bin/env python3
"""Read-only SHA-256 verification of CADWorkbench's unissued SVG/PDF batch.

This checks export integrity, NOT engineering content, approval or GD&T.
Usage: python tools/verify_unissued_drawings.py <exported_preview_folder>
"""
import argparse
import csv
import hashlib
from pathlib import Path
import re
import sys

FIELDS = ('preview_file','sheet_owner_id','drawing_number','issue_status',
          'pdf_file','current_geometry_token','svg_sha256','pdf_sha256')
DIGEST = re.compile(r'^[0-9a-f]{64}$')

def verify(folder: Path):
    folder = Path(folder)
    manifest=folder/'drawing_index.csv'
    if not folder.is_dir() or not manifest.is_file():
        raise ValueError('Missing unissued drawing folder or drawing_index.csv')
    with manifest.open(newline='', encoding='utf-8-sig') as stream:
        reader=csv.DictReader(stream)
        if tuple(reader.fieldnames or ()) != FIELDS:
            raise ValueError('Unrecognized drawing index columns or missing integrity hashes')
        rows=list(reader)
    if not rows:
        raise ValueError('Drawing index contains no saved sheets')
    if any(p.is_symlink() for p in folder.iterdir()):
        raise ValueError('Symlinks are not accepted in unissued review packages')
    expected={'drawing_index.csv'}
    for number,row in enumerate(rows,1):
        if not all(row.get(field) is not None for field in FIELDS):
            raise ValueError(f'Row {number}: malformed CSV row')
        if row['issue_status']!='UNISSUED REVIEW REQUIRED':
            raise ValueError(f'Row {number}: export is not marked UNISSUED')
        svg=f'drawing_{number:03d}_UNISSUED.svg'
        pdf=f'drawing_{number:03d}_UNISSUED.pdf'
        if row['preview_file']!=svg:
            raise ValueError(f'Row {number}: unexpected SVG name or sequence')
        if row['pdf_file'] not in ('',pdf):
            raise ValueError(f'Row {number}: unexpected PDF name or sequence')
        for name,hash_field in ((svg,'svg_sha256'),(row['pdf_file'],'pdf_sha256')):
            digest=row[hash_field]
            if not name:
                if digest:raise ValueError(f'Row {number}: digest without a PDF')
                continue
            if not DIGEST.fullmatch(digest):
                raise ValueError(f'Row {number}: missing or malformed digest for {name}')
            expected.add(name)
            actual=hashlib.sha256((folder/name).read_bytes()).hexdigest()
            if actual!=digest:
                raise ValueError(f'Row {number}: checksum mismatch for {name}')
    existing={p.name for p in folder.iterdir() if p.is_file()}
    if existing!=expected:
        raise ValueError(f'Unexpected/missing files: {sorted(existing ^ expected)}')
    return len(rows)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder',type=Path)
    args=parser.parse_args()
    try:
        count=verify(args.folder)
    except (ValueError,FileNotFoundError,PermissionError,IsADirectoryError,OSError) as e:
        print(f'FAIL: {e}',file=sys.stderr)
        return 1
    print(f'PASS: {count} unissued drawing sheet(s), SVG/PDF hashes and manifest verified; NOT approved for manufacture')
    return 0
if __name__=='__main__':sys.exit(main())

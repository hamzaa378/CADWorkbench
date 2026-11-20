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
MAX_INDEX_BYTES=8*1024*1024
MAX_DRAWING_BYTES=256*1024*1024
MAX_SHEETS=10000

def _checked_file_hash(path:Path)->str:
    if not path.exists():
        raise FileNotFoundError(f'Missing drawing file: {path.name}')
    if path.is_symlink() or not path.is_file() or path.stat().st_size>MAX_DRAWING_BYTES:
        raise ValueError(f'Linked or oversized drawing file: {path.name}')
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):
            digest.update(block)
    return digest.hexdigest()

def verify(folder: Path):
    folder = Path(folder)
    manifest=folder/'drawing_index.csv'
    if (not folder.is_dir() or folder.is_symlink() or not manifest.is_file() or
            manifest.is_symlink() or manifest.stat().st_size>MAX_INDEX_BYTES):
        raise ValueError('Missing unissued drawing folder or drawing_index.csv')
    with manifest.open(newline='', encoding='utf-8-sig') as stream:
        reader=csv.DictReader(stream)
        if tuple(reader.fieldnames or ()) != FIELDS:
            raise ValueError('Unrecognized drawing index columns or missing integrity hashes')
        rows=list(reader)
    if not rows or len(rows)>MAX_SHEETS:
        raise ValueError('Drawing index contains no saved sheets or exceeds size limit')
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
            actual=_checked_file_hash(folder/name)
            if actual!=digest:
                raise ValueError(f'Row {number}: checksum mismatch for {name}')
    existing={p.name for p in folder.iterdir()}
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

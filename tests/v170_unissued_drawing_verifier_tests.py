"""Real file-integrity and failure tests for unissued drawing batch verification."""
import csv
import hashlib
import tempfile
import unittest
from pathlib import Path
from sys import path
path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from verify_unissued_drawings import verify, FIELDS
class UnissuedBatchVerification(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.root=Path(self.temp.name)
        self.rows=[]
        for i in (1,2):
            svg=f'drawing_{i:03d}_UNISSUED.svg'
            data=f'<svg>{i}</svg>'.encode()
            (self.root/svg).write_bytes(data)
            pdf=f'drawing_{i:03d}_UNISSUED.pdf' if i==2 else ''
            pdata=b'%PDF-1.7 synthetic test bytes'
            if pdf:(self.root/pdf).write_bytes(pdata)
            self.rows.append([svg,f'body{i}',f'DWG{i}','UNISSUED REVIEW REQUIRED',pdf,'geometry-1',
                              hashlib.sha256(data).hexdigest(),hashlib.sha256(pdata).hexdigest() if pdf else ''])
        self.write_index()
    def tearDown(self):self.temp.cleanup()
    def write_index(self):
        with (self.root/'drawing_index.csv').open('w',newline='') as f:
            writer=csv.writer(f);writer.writerow(FIELDS);writer.writerows(self.rows)
    def test_valid_batch(self):self.assertEqual(verify(self.root),2)
    def test_detects_svg_tampering(self):
        (self.root/'drawing_001_UNISSUED.svg').write_text('<svg>tampered</svg>')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):verify(self.root)
    def test_detects_pdf_tampering(self):
        (self.root/'drawing_002_UNISSUED.pdf').write_bytes(b'altered')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):verify(self.root)
    def test_rejects_unapproved_name(self):
        self.rows[0][0]='../danger.svg';self.write_index()
        with self.assertRaisesRegex(ValueError,'unexpected SVG'):verify(self.root)
    def test_rejects_issued(self):
        self.rows[0][3]='APPROVED';self.write_index()
        with self.assertRaisesRegex(ValueError,'not marked UNISSUED'):verify(self.root)
    def test_rejects_extra_or_missing_files(self):
        (self.root/'stray.pdf').write_bytes(b'extra')
        with self.assertRaisesRegex(ValueError,'Unexpected/missing files'):verify(self.root)
        (self.root/'stray.pdf').unlink()
        (self.root/'drawing_001_UNISSUED.svg').unlink()
        with self.assertRaises(FileNotFoundError):verify(self.root)
    def test_rejects_missing_checksum_column(self):
        (self.root/'drawing_index.csv').write_text('preview_file\nfile.svg\n')
        with self.assertRaisesRegex(ValueError,'Unrecognized drawing index'):verify(self.root)
    def test_rejects_digest_without_pdf(self):
        self.rows[0][7]='f'*64;self.write_index()
        with self.assertRaisesRegex(ValueError,'digest without'):verify(self.root)
    def test_rejects_duplicate_sequence(self):
        self.rows.append(self.rows[0]);self.write_index()
        with self.assertRaisesRegex(ValueError,'unexpected SVG'):verify(self.root)
if __name__=='__main__':unittest.main()

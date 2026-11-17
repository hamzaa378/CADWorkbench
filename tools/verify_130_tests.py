#!/usr/bin/env python3
"""Make sure this preliminary snapshot still registers exactly 130 CTests."""
from pathlib import Path
import re
p=Path(__file__).resolve().parents[1]/"CMakeLists.txt"
code=p.read_text(encoding="utf-8")
registered=re.findall(r"^\s*add_test\(\s*NAME\s+(\S+)",code,re.MULTILINE)
if len(registered)!=130 or len(set(registered))!=130:
    raise SystemExit(f"FAIL: expected 130 unique CTests; got {len(registered)} entries ({len(set(registered))} distinct)")
print("PASS: 130 unique native CTest registrations retained")

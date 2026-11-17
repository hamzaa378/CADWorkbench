#!/usr/bin/env python3
"""Strict 19/20-per-stream evidence audit. NEVER marks scenarios passed automatically.

This is a separate 95%-readiness goal, not a replacement for the existing
17/17/13 engineering release gate. All critical scenarios must pass.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path

EXPECTED={name: [f"{name}{i:02d}" for i in range(1,21)] for name in "PAD"}
CRITICAL={
    "P": {"P01","P02","P09","P10","P12","P13","P14","P18"},
    "A": {"A03","A07","A08","A11","A12","A17"},
    "D": {"D04","D08","D09","D17"},
}
ALLOWED={"PASS","FAIL","NOT_RUN","BLOCKED"}

def verify(path: Path, evidence_root: Path):
    with path.open(newline="",encoding="utf-8-sig") as source:
        rows=list(csv.DictReader(source))
    if len(rows)!=60:
        raise ValueError(f"Expected exactly 60 distinct scenarios, found {len(rows)}")
    evidence_root=evidence_root.resolve()
    entries={}
    for row in rows:
        sid=row.get("id","").strip()
        stream=row.get("workstream","").strip().upper()
        status=row.get("status","").strip().upper()
        if stream not in EXPECTED or sid not in EXPECTED[stream] or sid in entries:
            raise ValueError(f"Unknown or duplicate acceptance scenario: {sid}")
        required=sid in CRITICAL[stream]
        if row.get("critical","").strip().lower() not in {"true","false"} or \
           (row["critical"].strip().lower()=="true") != required:
            raise ValueError(f"Critical flag inconsistent with locked acceptance checklist: {sid}")
        if status not in ALLOWED:
            raise ValueError(f"Invalid acceptance status {status}: {sid}")
        evidence=row.get("evidence_path","").strip()
        witness=None
        if status=="PASS":
            if not evidence:
                raise ValueError(f"PASS without evidence: {sid}")
            q=Path(evidence)
            if q.is_absolute() or ".." in q.parts:
                raise ValueError(f"Unsafe evidence path: {sid}")
            file=(evidence_root/q).resolve()
            if not file.is_relative_to(evidence_root) or not file.is_file():
                raise ValueError(f"Missing or outside-root evidence: {sid}")
            if file.stat().st_size==0:
                raise ValueError(f"Empty acceptance evidence: {sid}")
            witness={"path":evidence,"sha256":hashlib.sha256(file.read_bytes()).hexdigest(),
                     "bytes":file.stat().st_size}
        entries[sid]={"id":sid,"workstream":stream,"critical":required,
                      "status":status,"evidence":witness,
                      "scenario":row.get("scenario","").strip()}
    by_stream={}
    for stream, expected in EXPECTED.items():
        passed=sum(entries[s]["status"]=="PASS" for s in expected)
        critical_open=[s for s in sorted(CRITICAL[stream]) if entries[s]["status"]!="PASS"]
        by_stream[stream]={"passed":passed,"total":20,"percent":passed*5,
             "target_95":19,"remaining_to_target":max(0,19-passed),
             "all_critical_passed":not critical_open,"open_critical":critical_open,
             "target_95_verified":passed>=19 and not critical_open,
             "failed":[s for s in expected if entries[s]["status"]=="FAIL"],
             "blocked":[s for s in expected if entries[s]["status"]=="BLOCKED"],
             "not_run":[s for s in expected if entries[s]["status"]=="NOT_RUN"]}
    return {"schema_version":1,"target":"95% (19/20 per workstream + all critical)",
            "all_three_at_95":all(s["target_95_verified"] for s in by_stream.values()),
            "workstreams":by_stream,"evidence":{s:entries[s]["evidence"]
                                      for s in sorted(entries) if entries[s]["evidence"]}}

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument("--results",type=Path,default=Path("docs/V170_ACCEPTANCE_RESULTS.csv"))
    a.add_argument("--evidence-root",type=Path,default=Path("."))
    a.add_argument("--report",type=Path,help="optional JSON evidence ledger output")
    args=a.parse_args()
    result=verify(args.results,args.evidence_root)
    for stream,r in result["workstreams"].items():
        print(f"{stream}: verified {r['passed']}/20; target 19/20; remaining {r['remaining_to_target']}; open critical {','.join(r['open_critical']) or 'none'}")
    print("ALL STREAMS 95% VERIFIED:", result["all_three_at_95"])
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    return 0 if result["all_three_at_95"] else 3

if __name__=="__main__":
    raise SystemExit(main())

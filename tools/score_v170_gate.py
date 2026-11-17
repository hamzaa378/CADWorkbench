#!/usr/bin/env python3
"""Conservative offline v1.70 acceptance gate scorer. Does not execute UI cases."""
import argparse,csv,json,sys,xml.etree.ElementTree as ET
from pathlib import Path

WORKSTREAMS={"P":"Parametric modeling","A":"Assembly engineering","D":"Drawings and sheet metal"}
CRITICAL={"P01","P02","P09","P10","P12","P13","P14","P18",
          "A03","A07","A08","A11","A12","A17",
          "D04","D08","D09","D17"}

def evaluate(csv_path, junit_path, expected_tests=130):
    csv_path=Path(csv_path);root=csv_path.resolve().parent.parent
    lines=[];problems=[]
    with csv_path.open(newline='',encoding='utf-8-sig') as f:
        lines=list(csv.DictReader(f))
    expected={f"{letter}{i:02d}" for letter in "PAD" for i in range(1,21)}
    observed=[row.get("id","") for row in lines]
    if len(lines)!=60 or len(set(observed))!=60 or set(observed)!=expected:
        problems.append("Acceptance inventory must contain exactly 60 unique P01-P20/A01-A20/D01-D20 cases")
    result={k:{"passed":0,"required":(17 if k in "PA" else 13),"total":20,"critical_unaccepted":[]} for k in "PAD"}
    for row in lines:
        key=row.get("id","");k=key[:1]
        if key not in expected:continue
        status=row.get("status","").strip().upper()
        if status not in {"NOT_RUN","PASS","FAIL"}:
            problems.append(f"{key}: unknown status {status!r}");continue
        if row.get("workstream")!=k or row.get("critical")!=("true" if key in CRITICAL else "false"):
            problems.append(f"{key}: inconsistent workstream or critical metadata")
        if status=="PASS":
            witness=row.get("evidence_path","").strip()
            if not witness:
                problems.append(f"{key}: PASS requires an evidence file path");status="NOT_RUN"
            else:
                path=Path(witness)
                if not path.is_absolute():path=root/witness
                if not path.is_file():
                    problems.append(f"{key}: evidence file not found: {witness}");status="NOT_RUN"
        if status=="PASS":result[k]["passed"]+=1
        if key in CRITICAL and status!="PASS":
            result[k]["critical_unaccepted"].append(key)
    for k in result:
        result[k]["percent_verified"]=(result[k]["passed"]/20)*100
    native={"passed":False,"count":0,"expected":expected_tests,"failed_cases":[]}
    if not junit_path or not Path(junit_path).is_file():
        problems.append("Native Windows CTest JUnit report is missing")
    else:
        try:
            doc=ET.parse(junit_path).getroot()
            testcases=list(doc.iter('testcase'))
            native["count"]=len(testcases)
            native["failed_cases"]=[x.attrib.get("name","unnamed") for x in testcases
                if any(c.tag in {"failure","error","skipped"} for c in x)]
            native["passed"]=len(testcases)==expected_tests and not native["failed_cases"] and \
                len({x.attrib.get("name") for x in testcases})==expected_tests
            if not native["passed"]:problems.append("Native CTest inventory incomplete, duplicated, failed, or skipped")
        except (ET.ParseError,OSError) as error:problems.append(f"Invalid native JUnit: {error}")
    eligible=sum(1 for x in result.values() if x["passed"]>=17)>=2 and \
        all(x["passed"]>=13 and not x["critical_unaccepted"] for x in result.values())
    approved=not problems and eligible and native["passed"]
    return {"release_approved":approved,"scenario_results":result,"native":native,
            "problems":problems,"warnings":["Evidence file existence does not independently certify GUI workflow correctness; review each artifact."]}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--csv',default='docs/V170_ACCEPTANCE_RESULTS.csv')
    ap.add_argument('--junit',default='build/cadworkbench_release_ctest.xml')
    ap.add_argument('--output',default='build/v170_acceptance_gate.json')
    args=ap.parse_args()
    report=evaluate(args.csv,args.junit)
    out=Path(args.output);out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for k,v in report['scenario_results'].items():
        print(f"{k} {WORKSTREAMS[k]}: {v['passed']}/20 ({v['percent_verified']:.0f}%) verified; critical unaccepted: {','.join(v['critical_unaccepted']) or 'none'}")
    print(f"Native CTests: {report['native']['count']}/{report['native']['expected']} recorded; all-pass={report['native']['passed']}")
    print(f"Release approved: {report['release_approved']}; report: {out}")
    for error in report['problems']:print(f"BLOCKER: {error}")
    return 0 if report['release_approved'] else 1

if __name__=='__main__':sys.exit(main())

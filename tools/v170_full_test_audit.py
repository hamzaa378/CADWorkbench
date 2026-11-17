#!/usr/bin/env python3
"""v1.70 complete test inventory and acceptance GAP audit.

This tool NEVER converts a native CTest PASS into end-to-end acceptance.
The CMake inventory, CTest JUnit and 60-case CSV are kept as distinct evidence.
"""
import argparse
import csv
import json
import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

CRITICAL={"P01","P02","P09","P10","P12","P13","P14","P18",
          "A03","A07","A08","A11","A12","A17",
          "D04","D08","D09","D17"}
TRACKS={"P":"Parametric modeling","A":"Assembly engineering","D":"Drawings and sheet metal"}
# Advisory *test leads*, not a coverage certification. Only exact CTest names are used.
TEST_LEADS={
    "P01":"cadcore_sketch_solver;cadcore_v170_advanced_sketch_geometry",
    "P02":"cadcore_sketch_solver;cadcore_v170_advanced_profile_validation",
    "P03":"cadcore_v167_sketch_polygon_features",
    "P04":"cadcore_v170_face_attached_sketch;cadcore_v170_face_lineage",
    "P05":"cadcore_oriented_extrusion;cadcore_custom_axis_revolve",
    "P06":"cadcore_v170_interleaved_history;cadcore_multi_edge_blends",
    "P07":"cadcore_elliptical_loft;cadcore_surface_modeling",
    "P08":"cadcore_sweeps;cadcore_helical_sweep",
    "P09":"cadcore_v170_interleaved_history;cadcore_v166_sparse_feature_regeneration",
    "P10":"cadcore_v170_mixed_feature_history;cadcore_v170_interleaved_history",
    "P11":"cadcore_v170_linked_body_edit;cadcore_v170_atomic_batch_edit",
    "P12":"cadcore_v170_face_lineage;cadcore_v170_curved_face_lineage",
    "P13":"cadcore_v170_inplace_ordered_stack;cadcore_v170_inplace_ordered_stack_rollback",
    "P14":"cadcore_v170_atomic_batch_persistence;cadcore_parametric_foundation",
    "P15":"cadcore_v170_parametric_study;cadcore_v170_guided_engineering_studies",
    "P16":"cadcore_universal_feature_tree;cadcore_v165_incremental_feature_graph",
    "P17":"cadcore_document_revision_history;cadcore_document_audit",
    "P18":"cadcore_v170_face_lineage;cadcore_v170_curved_face_lineage",
    "P19":"cadcore_v170_atomic_batch_preview;cadcore_v170_atomic_batch_edit",
    "P20":"cadcore_geometry_and_persistence;cadcore_parametric_feature_suppression",
    "A01":"cadcore_persistent_assemblies;cadcore_assembly_bom_full_scene",
    "A02":"cadcore_v170_closed_loop_rigid_network;cadcore_assembly_mate_solver",
    "A03":"cadcore_v170_closed_loop_scene;cadcore_v170_closed_loop_rigid_network",
    "A04":"cadcore_assembly_joints;cadcore_assembly_joint_coordinate_guards",
    "A05":"cadcore_v170_articulated_closed_loop;cadcore_v170_articulated_scene_transaction",
    "A06":"cadcore_assembly_mate_document;cadcore_assembly_mate_solver",
    "A07":"cadcore_v170_articulated_closed_loop;cadcore_v170_closed_loop_scene",
    "A08":"cadcore_v170_articulated_scene_transaction;cadcore_assembly_joint_persistence_guards",
    "A09":"cadcore_assembly_free_occurrence_yaw;cadcore_assembly_occurrence_pose_copy",
    "A10":"cadcore_assembly_joint_persistence_guards;cadcore_assembly_joints",
    "A11":"cadcore_v170_articulated_scene_transaction;cadcore_v170_articulated_closed_loop",
    "A12":"cadcore_v170_articulated_scene_transaction;cadcore_v170_assembly_fit_audit",
    "A13":"cadcore_interference;cadcore_minimum_distance",
    "A14":"cadcore_v170_assembly_fit_policy;cadcore_v170_assembly_fit_audit",
    "A15":"cadcore_v170_assembly_mate_document;cadcore_assembly_bom_full_scene",
    "A16":"cadcore_v170_articulated_scene_transaction;cadcore_v170_assembly_fit_audit",
    "A17":"cadcore_v170_articulated_scene_transaction;cadcore_v170_assembly_mate_document",
    "A18":"cadcore_persistent_assemblies;cadcore_assembly_occurrence_pose_copy",
    "A19":"cadcore_assembly_bom_full_scene;cadcore_assembly_bom_group_keys",
    "A20":"cadcore_step_assembly_import;cadcore_persistent_assemblies",
    "D01":"cadcore_orthographic_drawing;cadcore_planar_section",
    "D02":"cadcore_orthographic_drawing;cadcore_planar_section",
    "D03":"cadcore_orthographic_drawing",
    "D04":"cadcore_orthographic_drawing;cadcore_parametric_links",
    "D05":"cadcore_orthographic_drawing;cadcore_circular_hole_pattern",
    "D06":"cadcore_orthographic_drawing;cadcore_document_audit",
    "D07":"cadcore_orthographic_drawing;cadcore_v164_tolerance_stacks",
    "D08":"cadcore_orthographic_drawing;cadcore_v170_linked_body_edit",
    "D09":"cadcore_v170_face_lineage;cadcore_orthographic_drawing",
    "D10":"cadcore_orthographic_drawing;cadcore_document_revision_history",
    "D11":"cadcore_orthographic_drawing;cadcore_body_schedule_csv",
    "D12":"cadcore_orthographic_drawing;cadcore_geometry_and_persistence",
    "D13":"cadcore_sheet_metal_flat_pattern;cadcore_folded_sheet_metal",
    "D14":"cadcore_folded_sheet_metal",
    "D15":"cadcore_folded_sheet_metal",
    "D16":"cadcore_sheet_metal_flat_pattern;cadcore_folded_sheet_metal",
    "D17":"cadcore_folded_sheet_metal;cadcore_sheet_metal_flat_pattern",
    "D18":"cadcore_sheet_metal_flat_pattern;cadcore_folded_sheet_metal",
    "D19":"cadcore_folded_sheet_metal",
    "D20":"cadcore_sheet_metal_flat_pattern;cadcore_folded_sheet_metal",
}


def expected_tests(cmake_file):
    text=cmake_file.read_text(encoding="utf-8")
    names=re.findall(r"\badd_test\s*\(\s*NAME\s+([a-zA-Z0-9_\-]+)",text)
    if len(names)!=130 or len(set(names))!=130:
        raise ValueError(f"Expected 130 unique registered native tests; found {len(names)} entries, {len(set(names))} unique")
    return names


def native_from_junit(path, names):
    expected=set(names)
    rows={name:{"name":name,"status":"NOT_RUN","detail":"No CTest result","time_s":""} for name in names}
    issues=[];actual=[]
    if not path.is_file():
        issues.append("Native Windows JUnit report missing; do not reuse older reports")
        return rows,issues
    try:
        root=ET.parse(path).getroot()
    except (OSError,ET.ParseError) as err:
        issues.append(f"Could not parse JUnit: {err}")
        return rows,issues
    for el in root.iter("testcase"):
        name=el.get("name","")
        actual.append(name)
        if name not in expected:
            issues.append(f"Unregistered native test in JUnit: {name!r}")
            continue
        failed=[child for child in el if child.tag in ("failure","error")]
        skipped=any(child.tag=="skipped" for child in el)
        status="FAIL" if failed else ("SKIPPED" if skipped else "PASS")
        detail=" | ".join(((node.get("message") or "")+" "+(node.text or "")).strip()[:800]
                         for node in failed)
        rows[name]={"name":name,"status":status,"detail":detail,"time_s":el.get("time","")}
    duplicate=[k for k,v in Counter(actual).items() if v>1]
    if duplicate:issues.append("Duplicate native CTest results: "+", ".join(sorted(duplicate)))
    missing=expected-set(actual)
    if missing:issues.append(f"Missing {len(missing)} native results")
    if len(actual)!=len(names):issues.append(f"Native JUnit has {len(actual)} testcases; expected {len(names)}")
    return rows,issues


def acceptance_from_csv(path,root):
    with path.open(newline="",encoding="utf-8-sig") as fd:
        rows=list(csv.DictReader(fd))
    required={f"{kind}{i:02d}" for kind in "PAD" for i in range(1,21)}
    seen=[row.get("id","") for row in rows]
    if len(rows)!=60 or set(seen)!=required or len(set(seen))!=60:
        raise ValueError("Acceptance CSV must contain P01-P20, A01-A20, D01-D20 once each")
    issues=[]
    for row in rows:
        id_=row["id"]
        if row.get("workstream")!=id_[0] or row.get("critical")!=("true" if id_ in CRITICAL else "false"):
            issues.append(f"{id_}: inconsistent workstream/critical metadata")
        status=row.get("status","").strip().upper()
        if status not in ("NOT_RUN","PASS","FAIL"):
            issues.append(f"{id_}: unexpected acceptance status {status!r}")
        if status=="PASS":
            location=row.get("evidence_path","").strip()
            if not location:
                issues.append(f"{id_}: PASS without recorded evidence")
            else:
                witness=Path(location)
                if not witness.is_absolute():witness=root/location
                if not witness.is_file():issues.append(f"{id_}: referenced evidence file not present: {location}")
    return rows,issues


def write_csv(path,rows,fields):
    with path.open("w",newline="",encoding="utf-8") as fd:
        writer=csv.DictWriter(fd,fieldnames=fields,extrasaction="ignore")
        writer.writeheader();writer.writerows(rows)


def run(project,junit,acceptance,outdir):
    outdir.mkdir(parents=True,exist_ok=True)
    inventory=expected_tests(project/"CMakeLists.txt")
    native,native_problems=native_from_junit(junit,inventory)
    acceptance_rows,acceptance_problems=acceptance_from_csv(acceptance,project)
    problems=native_problems+acceptance_problems
    native_count=Counter(row["status"] for row in native.values())
    native_ok=(native_count["PASS"]==130 and not native_problems)
    matrix=[]
    for row in acceptance_rows:
        id_=row["id"]
        candidates=[n for n in TEST_LEADS[id_].split(";") if n in native]
        candidate_status=[native[n]["status"] for n in candidates]
        status=row["status"].strip().upper()
        missing_evidence=status=="PASS" and any(id_ in p for p in acceptance_problems)
        accepted=status=="PASS" and not missing_evidence
        if status=="FAIL":action="CORRECT_AND_RETEST"
        elif missing_evidence:action="ATTACH_VERIFIABLE_EVIDENCE"
        elif status=="NOT_RUN":action="EXECUTE_GUI_ACCEPTANCE"
        else:action="INDEPENDENT_REVIEW_REQUIRED"
        matrix.append({
            "id":id_,"workstream":TRACKS[id_[0]],"critical":row["critical"],
            "scenario":row["scenario"],"acceptance_status":status,
            "evidence_path":row.get("evidence_path","").strip(),
            "native_test_leads":";".join(candidates),
            "lead_native_states":";".join(candidate_status),
            "verified_acceptance":accepted,"next_action":action})
    if not native_ok:
        problems.append("Native Windows 130/130 gate is not yet satisfied")
    workstreams={}
    for kind in "PAD":
        subset=[r for r in matrix if r["id"].startswith(kind)]
        passed=sum(1 for r in subset if r["verified_acceptance"])
        critical_open=[r["id"] for r in subset if r["id"] in CRITICAL and not r["verified_acceptance"]]
        workstreams[kind]={"title":TRACKS[kind],"accepted":passed,"required":17 if kind in "PA" else 13,
             "critical_open":critical_open,"not_run":sum(r["acceptance_status"]=="NOT_RUN" for r in subset),
             "failed":sum(r["acceptance_status"]=="FAIL" for r in subset),
             "threshold_met":passed>=(17 if kind in "PA" else 13) and not critical_open}
    gate=(native_ok and not acceptance_problems and
          sum(r["accepted"]>=17 for r in workstreams.values())>=2 and
          all(r["accepted"]>=13 and not r["critical_open"] for r in workstreams.values()))
    payload={"generated_utc":datetime.now(timezone.utc).isoformat(),
         "project":str(project),"native_report":str(junit),"acceptance_csv":str(acceptance),
         "release_gate_evidence_satisfied":gate,
         "native_counts":{key:native_count[key] for key in ("PASS","FAIL","SKIPPED","NOT_RUN")},
         "native_complete":native_ok,"workstreams":workstreams,
         "problems":list(dict.fromkeys(problems)),
         "limitations":["Native CTest leads are advisory, not proof of GUI acceptance.",
           "A PASS in acceptance CSV is a recorded claim backed by a file, not independent engineering certification.",
           "No user interface automation or proprietary reference-CAD parity is inferred from native CTest.",
           "Estimated completion percentages from prior reports are not scored here."],
         "native_cases":list(native.values()),"acceptance_scenarios":matrix}
    (outdir/"v170_full_audit.json").write_text(json.dumps(payload,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    write_csv(outdir/"v170_native_130_matrix.csv",native.values(),["name","status","time_s","detail"])
    write_csv(outdir/"v170_acceptance_60_matrix.csv",matrix,
              ["id","workstream","critical","scenario","acceptance_status","evidence_path",
               "native_test_leads","lead_native_states","verified_acceptance","next_action"])
    text=["# CADWorkbench v1.70 — full test and functional gap audit", "",
          "Native tests and end-to-end acceptance are intentionally separate.", "",
          "## Native Windows tests", "",
          f"- Registered CTests: {len(inventory)}",
          f"- PASS: {native_count['PASS']}; FAIL: {native_count['FAIL']}; SKIPPED: {native_count['SKIPPED']}; NOT_RUN: {native_count['NOT_RUN']}",
          f"- Full native gate satisfied: **{'YES' if native_ok else 'NO'}**", ""]
    failed=[r for r in native.values() if r["status"] in ("FAIL","SKIPPED")]
    if failed:
        text.extend(["### Failed or skipped native cases", ""])
        for row in failed:text.append(f"- `{row['name']}` — {row['status']}: {row['detail'][:180] or 'See full CTest log'}")
        text.append("")
    text.extend(["## 60-scenario end-to-end acceptance", "",
      "| Workstream | Recorded PASS | Required | Critical still open | NOT_RUN | FAIL |",
      "|---|---:|---:|---:|---:|---:|"])
    for kind,r in workstreams.items():
        text.append(f"| {r['title']} | {r['accepted']}/20 | {r['required']}/20 | {len(r['critical_open'])} | {r['not_run']} | {r['failed']} |")
    text.extend(["",f"**Final release evidence gate: {'SATISFIED' if gate else 'NOT SATISFIED'}**", "",
      "### Outstanding scenarios (NOT_RUN/FAIL/missing evidence)", ""])
    for kind in "PAD":
        text.append(f"#### {TRACKS[kind]}")
        text.append("")
        for r in matrix:
            if r["id"].startswith(kind) and not r["verified_acceptance"]:
                critical=" **CRITICAL**" if r["id"] in CRITICAL else ""
                text.append(f"- **{r['id']}**{critical}: {r['scenario']} — {r['next_action']}")
        text.append("")
    text.extend(["## Interpretation and known engineering review priorities", "",
      "- A `NOT_RUN` scenario means *not yet demonstrated*, not necessarily missing from source code.",
      "- Parametric: test identity recovery through arbitrary face splits/merges, mixed feature replay, persistent sketch references and undo/reopen.",
      "- Assembly: test stored joint relationships across full closed-loop motion, simultaneous clearance, and saved hierarchy/STEP round-trip.",
      "- Drawings/sheet metal: test associative annotations after source edits, complex flanges/reliefs, fold-flat round trips, and export agreement.",
      "- Inspect each recorded evidence file and mark a scenario PASS only after observing the complete user workflow.",
      "- The presence of a related native PASS does **not** mark the corresponding acceptance scenario PASS.","",
      "## Recommended verification order","",
      "1. Full Release build and all 130 native CTests; correct failing cases without suppressing them.",
      "2. Open CADWorkbench and complete the 60 scenarios from `docs/V170_ACCEPTANCE_RESULTS.csv`.",
      "3. Save evidence under `build/acceptance_evidence/<scenario-id>/` and update `build/V170_ACCEPTANCE_RESULTS.csv`.",
      "4. Rerun this audit; compare the remaining scenario matrix and ensure the mandatory critical cases are accepted.",
      "5. Run the release-gate scorer; approve only after objective native tests and reviewed acceptance evidence are complete.",""])
    if problems:
        text.extend(["## Verification problems",""]+[f"- {issue}" for issue in dict.fromkeys(problems)]+[""])
    (outdir/"v170_remaining_work.md").write_text("\n".join(text),encoding="utf-8")
    print(f"Native {native_count['PASS']}/{len(inventory)} passed; failed={native_count['FAIL']}; missing={native_count['NOT_RUN']}; acceptance P/A/D={workstreams['P']['accepted']}/{workstreams['A']['accepted']}/{workstreams['D']['accepted']}; gate={gate}")
    print(f"Output directory: {outdir}")
    return 0 if gate else 1


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",type=Path,default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--junit",type=Path,default=None)
    parser.add_argument("--acceptance",type=Path,default=None)
    parser.add_argument("--output-dir",type=Path,default=None)
    args=parser.parse_args(argv)
    root=args.root.resolve()
    junit=args.junit or root/"build/cadworkbench_release_ctest.xml"
    acceptance=args.acceptance or root/"build/V170_ACCEPTANCE_RESULTS.csv"
    if not acceptance.is_file():acceptance=root/"docs/V170_ACCEPTANCE_RESULTS.csv"
    out=args.output_dir or root/"build/v170_comprehensive_audit"
    try:return run(root,junit,acceptance,out)
    except (ValueError,OSError) as exc:
        print(f"AUDIT INPUT ERROR: {exc}",file=sys.stderr)
        return 2

if __name__=="__main__":raise SystemExit(main())

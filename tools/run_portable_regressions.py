#!/usr/bin/env python3
"""Build and execute C++ standard-library-only regression suites.

Never replaces the 130 native Windows CTests. Artifacts go in a temporary
folder, not inside the source tree.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed

ROOT=Path(__file__).resolve().parents[1]
SOURCES=(
    "metric_clearance_and_search_portable_tests.cpp",
    "directional_hole_span_portable_tests.cpp",
    "face_machining_datum_portable_tests.cpp",
    "assembly_network_structure_audit_tests.cpp",
    "sheet_metal_bend_sequence_audit_tests.cpp",
    "sketch_constraint_diagnostics_portable_tests.cpp",
    "stock_sheet_layout_portable_tests.cpp",
    "stock_material_compatibility_portable_tests.cpp",
    "joint_drive_coupling_portable_tests.cpp",
    "joint_motion_interpolation_portable_tests.cpp",
    "joint_keyframe_rate_audit_portable_tests.cpp",
    "stock_contour_metrics_portable_tests.cpp",
    "stock_contour_nesting_portable_tests.cpp",
    "simultaneous_proximity_portable_tests.cpp",
    "articulated_frame_euler_portable_tests.cpp",
    "folded_contour_topology_portable_tests.cpp",
    "refold_geometry_audit_portable_tests.cpp",
    "manufacturing_hole_patterns_portable_tests.cpp",
    # The real model-dependency DAG and its causal rebuild trace are kernel-
    # independent but split across a .h/.cpp pair, so compile both here.
    "linked_body_graph_v170_tests.cpp",
    "plane_offset_mate_portable_tests.cpp",
    "five_feature_engineering_portable_tests.cpp",
    "bend_coupon_calibration_portable_tests.cpp",
    "design_browser_query_portable_tests.cpp",
    # Backend-only engineering increment: mate mobility, synchronized bounded
    # quintic motion and deterministic incremental regeneration waves.
    "backend_mobility_portable_tests.cpp",
    "backend_quintic_motion_portable_tests.cpp",
    "backend_rebuild_waves_portable_tests.cpp",
    "assembly_tolerance_budget_portable_tests.cpp",
    # Sprint 3: actual new engineering worksheet + drawing freshness APIs,
    # randomized deterministic property suites, and sheet bend stress.
    "tolerance_rescue_plan_portable_tests.cpp",
    "drawing_reference_audit_portable_tests.cpp",
    "tolerance_rescue_properties_portable_tests.cpp",
    "drawing_reference_stress_portable_tests.cpp",
    "sheet_metal_bend_properties_portable_tests.cpp",
    # Development pass 1: numeric witness and fit arithmetic hardening.
    "geometry_numeric_stability_portable_tests.cpp",
    # Development pass 2: constrained shop tolerance allocation.
    "shop_tolerance_allocation_portable_tests.cpp",
    # Development pass 3: continuous translational broad-phase motion safety.
    "motion_swept_clearance_portable_tests.cpp",
    # Sprint 6: production route-consensus check and staged commit invariants.
    "reference_route_consensus_portable_tests.cpp",
    "atomic_rebuild_guard_portable_tests.cpp",
    "hole_position_inspection_portable_tests.cpp",
    "rigid_motion_speed_bound_portable_tests.cpp",
    "hole_inspection_workflow_portable_tests.cpp",
    "order_invariant_swept_portable_tests.cpp",
    "hole_revision_impact_portable_tests.cpp",
    "drawing_revision_impact_portable_tests.cpp",
    "linked_flat_development_audit_portable_tests.cpp",
)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--compiler",default=None,help="g++ or clang++ (default: auto-detect)")
    parser.add_argument("--jobs",type=int,default=1,help="Parallel compile jobs (1-4; default sequential)")
    args=parser.parse_args()
    if args.jobs<1 or args.jobs>4:
        parser.error("--jobs must be between 1 and 4")
    compiler=args.compiler or next((name for name in ("g++","clang++") if shutil.which(name)),None)
    if not compiler or not shutil.which(compiler):
        raise SystemExit("Missing g++/clang++: portable regressions NOT run")
    with tempfile.TemporaryDirectory(prefix="cad_v170_portable_") as folder:
        def compile_and_run(name):
            source=ROOT/"tests"/name
            if not source.is_file():
                raise RuntimeError(f"Missing portable regression: {source}")
            executable=Path(folder)/source.stem
            extra=[str(ROOT/"src/core/LinkedBodyGraph.cpp")] if name in ("linked_body_graph_v170_tests.cpp","backend_rebuild_waves_portable_tests.cpp") else []
            if name in ("assembly_tolerance_budget_portable_tests.cpp",
                        "tolerance_rescue_plan_portable_tests.cpp",
                        "tolerance_rescue_properties_portable_tests.cpp",
                        "geometry_numeric_stability_portable_tests.cpp",
                        "shop_tolerance_allocation_portable_tests.cpp"):
                extra=[str(ROOT/"src/core/WorkshopEngineering.cpp")]
            if name=="rigid_motion_speed_bound_portable_tests.cpp":
                extra=[str(ROOT/"src/core/AdaptiveMotionScan.cpp")]
            if name=="plane_offset_mate_portable_tests.cpp":
                extra=[str(ROOT/"src/core/AssemblyMateSolver.cpp"),
                       str(ROOT/"src/core/AssemblyConstraintNetwork.cpp")]
            command=[compiler,"-std=c++20","-O1","-I",str(ROOT/"src"),str(source),*extra,"-o",str(executable)]
            build=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if build.returncode:
                raise RuntimeError(f"COMPILE FAILURE {name} (exit {build.returncode})\n{build.stdout}")
            run=subprocess.run([str(executable)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if run.returncode:
                raise RuntimeError(f"TEST FAILURE {name} (exit {run.returncode})\n{run.stdout}")
            return f"PASS {name}\n{run.stdout.strip()}"
        # Parallel jobs speed development feedback but every executable must
        # finish successfully before reporting a single aggregate PASS.
        if args.jobs==1:
            for name in SOURCES:
                print(compile_and_run(name),flush=True)
        else:
            with ThreadPoolExecutor(max_workers=args.jobs) as executor:
                futures={executor.submit(compile_and_run,name):name for name in SOURCES}
                errors=[]
                for future in as_completed(futures):
                    try:
                        print(future.result(),flush=True)
                    except Exception as exc:
                        errors.append((futures[future],str(exc)))
                if errors:
                    raise RuntimeError('\n'.join(f'{name}: {msg}' for name,msg in errors))
    print(f"PASS: {len(SOURCES)} portable C++ regression executables",flush=True)

if __name__=="__main__":
    main()

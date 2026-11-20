#!/usr/bin/env bash
# Portable C++ and independent Python checks only; NOT a Windows MSVC/Qt build.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
compiler="${1:-g++}"
if ! command -v "$compiler" >/dev/null; then echo "Compiler not found: $compiler" >&2; exit 2; fi
workspace="$(mktemp -d)"
trap 'rm -rf "$workspace"' EXIT
flags=(-std=c++20 -O0 -g0 -fno-omit-frame-pointer -I src)
if [[ "${CAD_SANITIZE:-1}" == "1" ]]; then flags+=(-fsanitize=address,undefined); fi
compile_run(){
  local label="$1"; shift
  echo "[portable] $compiler $label"
  "$compiler" "${flags[@]}" "$@" -o "$workspace/$label"
  ASAN_OPTIONS=detect_leaks=0 "$workspace/$label"
}
compile_run adaptive_motion src/core/AdaptiveMotionScan.cpp tests/adaptive_motion_scan_v170_tests.cpp
compile_run drilling_feasibility src/core/DrillingFeasibility.cpp tests/drilling_feasibility_v170_tests.cpp
compile_run feature_revisions src/core/FeatureDependencyPlan.cpp tests/feature_dependency_plan_v166_tests.cpp
compile_run circular_drawing_lineage tests/circular_edge_lineage_v170_tests.cpp
compile_run guided_tolerance_stack src/core/GuidedEngineeringStudies.cpp tests/guided_engineering_studies_v170_tests.cpp
compile_run nominal_fit_and_circle tests/assembly_fit_policy_v170_tests.cpp
compile_run sketch_constraint_diagnostics tests/sketch_constraint_diagnostics_portable_tests.cpp
compile_run folded_contour_topology tests/folded_contour_topology_portable_tests.cpp
compile_run folded_refold_geometry tests/refold_geometry_audit_portable_tests.cpp
compile_run boolean_ancestry tests/boolean_lineage_path_v170_tests.cpp
compile_run linked_body_graph src/core/LinkedBodyGraph.cpp tests/linked_body_graph_v170_tests.cpp
compile_run rigid_mates src/core/AssemblyMateSolver.cpp src/core/AssemblyConstraintNetwork.cpp tests/assembly_constraint_network_v170_tests.cpp
compile_run mate_structure tests/assembly_network_structure_audit_tests.cpp
compile_run bend_sequence tests/sheet_metal_bend_sequence_audit_tests.cpp
compile_run stock_sheet_layout tests/stock_sheet_layout_portable_tests.cpp
compile_run stock_contour_nesting tests/stock_contour_nesting_portable_tests.cpp
compile_run stock_contour_metrics tests/stock_contour_metrics_portable_tests.cpp
compile_run simultaneous_proximity tests/simultaneous_proximity_portable_tests.cpp
compile_run joint_frame_euler tests/articulated_frame_euler_portable_tests.cpp
compile_run joint_motion_interpolation tests/joint_motion_interpolation_portable_tests.cpp
compile_run joint_keyframe_rates tests/joint_keyframe_rate_audit_portable_tests.cpp
compile_run joint_drive_coupling tests/joint_drive_coupling_portable_tests.cpp
compile_run stock_material_gauge tests/stock_material_compatibility_portable_tests.cpp
compile_run articulated_mates src/core/AssemblyMateSolver.cpp src/core/AssemblyConstraintNetwork.cpp src/core/ArticulatedAssemblyNetwork.cpp tests/articulated_assembly_v170_tests.cpp
if [[ "${CAD_SKIP_PYTHON:-0}" != "1" ]]; then
  python -m unittest discover -s tests -p '*.py' -q
fi
printf '\nPortable checks complete. Native Windows build and CTests are still required.\n'

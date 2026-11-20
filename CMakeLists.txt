cmake_minimum_required(VERSION 3.24)
project(CADWorkbench VERSION 1.70.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if (NOT WIN32)
    message(FATAL_ERROR "CADWorkbench targets Windows (Win32 HWND). Build on Windows 10/11 with MSVC.")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Concurrent Svg PrintSupport)
find_package(OpenCASCADE 8.0.1 REQUIRED CONFIG)

add_library(cadcore STATIC
    src/core/SceneDocument.h
    src/core/SceneDocument.cpp
    src/core/AdaptiveMotionScan.h
    src/core/SimultaneousProximity.h
    src/core/MotionSweptClearance.h
    src/core/RigidMotionSpeedBound.h
    src/core/HolePositionInspection.h
    src/core/HoleInspectionWorkflow.h
    src/core/HoleRevisionImpact.h
    src/core/StockSheetLayout.h
    src/core/StockMaterialCompatibility.h
    src/core/AdaptiveMotionScan.cpp
    src/core/AssemblyFitPolicy.h
    src/core/AssemblyFitAudit.h
    src/core/AssemblyFitAudit.cpp
    src/core/AssemblyMateSolver.h
    src/core/AssemblyMateSolver.cpp
    src/core/AssemblyConstraintNetwork.h
    src/core/AssemblyConstraintNetwork.cpp
    src/core/ArticulatedAssemblyNetwork.h
    src/core/ArticulatedAssemblyNetwork.cpp
    src/core/LinkedBodyGraph.h
    src/core/LinkedBodyGraph.cpp
    src/core/PartsData.cpp
    src/core/DrawingSheet.cpp
    src/core/HoleTable.h
    src/core/HoleTable.cpp
    src/core/DrillingWorksheet.h
    src/core/DrillingWorksheet.cpp
    src/core/DrillingFeasibility.h
    src/core/DrillingFeasibility.cpp
    src/core/DrawingDxf.cpp
    src/core/SheetMetal.h
    src/core/LinkedFlatDevelopmentAudit.h
    src/core/FoldedContourTopology.h
    src/core/SheetMetal.cpp
    src/core/SurfaceModeling.h
    src/core/SurfaceModeling.cpp
    src/core/DocumentRevisionHistory.h
    src/core/DocumentRevisionHistory.cpp
    src/core/DocumentAudit.h
    src/core/DocumentAudit.cpp
    src/core/ManufacturingHandoff.h
    src/core/ManufacturingHandoff.cpp
    src/core/EngineeringDesignTools.h
    src/core/EngineeringDesignTools.cpp
    src/core/MassTensorAnalysis.h
    src/core/MassTensorAnalysis.cpp
    src/core/WorkshopEngineering.h
    src/core/AssemblyToleranceBudget.h
    src/core/ToleranceRescuePlan.h
    src/core/ShopToleranceAllocation.h
    src/core/DrawingReferenceAudit.h
    src/core/DrawingRevisionImpact.h
    src/core/WorkshopEngineering.cpp
    src/core/WorkshopGeometry.h
    src/core/WorkshopGeometry.cpp
    src/core/ParametricStudy.h
    src/core/ParametricStudy.cpp
    src/core/GuidedEngineeringStudies.h
    src/core/GuidedEngineeringStudies.cpp
    src/core/FeatureDependencyPlan.h
    src/core/FeatureDependencyPlan.cpp
    src/core/FeatureRebuildDiagnostics.h
    src/core/ReferenceRouteConsensus.h
    src/core/AtomicRebuildGuard.h
    src/core/FeatureRebuildDiagnostics.cpp
    src/core/PolygonFeatureProfile.h
    src/core/PolygonFeatureProfile.cpp
    src/core/AdvancedSketchProfiles.h
    src/core/AdvancedSketchProfiles.cpp
    src/core/ShapeFeatureTree.h
    src/core/CircularEdgeLineage.h
    src/core/ShapeFeatureTree.cpp
    src/core/BooleanLineagePath.h
    src/core/BranchReferenceAgreement.h
    src/core/BooleanReferenceLineage.h
    src/core/BooleanReferenceLineage.cpp
    src/core/MeshRepair.h
    src/core/MeshRepair.cpp
    src/core/SketchSolver.h
    src/core/SketchSolver.cpp
    src/core/SketchProfile.h
    src/core/SketchProfile.cpp
    src/core/FormFeatures.cpp
    src/core/EngineeringComponents.cpp
    src/core/AdvancedComponents.cpp
    src/core/SpecializedComponents.cpp
    src/core/IndustrialComponents.cpp
    src/core/ExtendedComponents.cpp
    src/core/PrecisionComponents.cpp
    src/core/ProductionComponents.cpp
    src/core/ModelingTools.cpp
)
target_include_directories(cadcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(cadcore PUBLIC
    Qt6::Core Qt6::Gui Qt6::Svg Qt6::PrintSupport
    TKernel TKMath TKG2d TKG3d TKBRep TKGeomBase TKGeomAlgo
    TKPrim TKTopAlgo TKBO TKBool TKOffset TKFillet TKShHealing TKMesh TKHLR
    TKDESTEP TKDEIGES TKDESTL TKXSBase TKCAF TKLCAF TKCDF TKXCAF
)

add_executable(CADWorkbench WIN32
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/CadToolIcons.h
    src/ui/SketchCanvasDialog.h
    src/ui/SketchCanvasDialog.cpp
    src/ui/EngineeringWorkbenchDialog.h
    src/ui/EngineeringWorkbenchDialog.cpp
    src/ui/GuidedStudyDialog.h
    src/ui/GuidedStudyDialog.cpp
    src/ui/WorkshopPlanningDialog.h
    src/ui/WorkshopPlanningDialog.cpp
    src/ui/MainWindow.cpp
    src/ui/CadViewport.h
    src/ui/CadViewport.cpp
)
target_link_libraries(CADWorkbench PRIVATE
    cadcore Qt6::Widgets Qt6::Gui Qt6::Concurrent
    TKV3d TKService TKOpenGl
)
target_compile_definitions(CADWorkbench PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN CADWORKBENCH_VERSION="${PROJECT_VERSION}")
if(MSVC)
    # UTF-8 source includes the degree/delta symbols used in tool dialogs.
    target_compile_options(cadcore PRIVATE /utf-8 /W4)
    # MainWindow.cpp is an extremely large Qt translation unit. MSVC's standard
    # COFF object section limit is insufficient (C1128); /bigobj is supported
    # by VS2026 and is target-scoped, never a compiler-wide workaround.
    target_compile_options(CADWorkbench PRIVATE /utf-8 /W4 /bigobj)
endif()

include(CTest)
if(BUILD_TESTING)
    # v1.70 foundation increment: trimmed-face lineage, in-place associative
    # sketches and six-DOF assembly mate solving.
    add_executable(assembly_constraint_network_v170_tests
        tests/assembly_constraint_network_v170_tests.cpp
        src/core/AssemblyConstraintNetwork.cpp src/core/AssemblyMateSolver.cpp)
    target_include_directories(assembly_constraint_network_v170_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(assembly_constraint_network_v170_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v170_closed_loop_rigid_network COMMAND assembly_constraint_network_v170_tests)
    add_executable(articulated_assembly_v170_tests
        tests/articulated_assembly_v170_tests.cpp
        src/core/AdaptiveMotionScan.cpp
        src/core/ArticulatedAssemblyNetwork.cpp
        src/core/AssemblyConstraintNetwork.cpp src/core/AssemblyMateSolver.cpp)
    target_include_directories(articulated_assembly_v170_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(articulated_assembly_v170_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v170_articulated_closed_loop COMMAND articulated_assembly_v170_tests)
    add_executable(articulated_assembly_scene_v170_tests
        tests/articulated_assembly_scene_v170_tests.cpp)
    target_link_libraries(articulated_assembly_scene_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_articulated_scene_transaction COMMAND articulated_assembly_scene_v170_tests)
    add_executable(interleaved_history_v170_tests tests/interleaved_history_v170_tests.cpp)
    target_link_libraries(interleaved_history_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_interleaved_history COMMAND interleaved_history_v170_tests)
    add_executable(assembly_constraint_network_scene_v170_tests
        tests/assembly_constraint_network_scene_v170_tests.cpp)
    target_link_libraries(assembly_constraint_network_scene_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_closed_loop_scene COMMAND assembly_constraint_network_scene_v170_tests)
    add_executable(assembly_mate_solver_v170_tests
        tests/assembly_mate_solver_v170_tests.cpp src/core/AssemblyMateSolver.cpp)
    target_include_directories(assembly_mate_solver_v170_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(assembly_mate_solver_v170_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v170_assembly_mate_solver COMMAND assembly_mate_solver_v170_tests)
    add_executable(assembly_mate_document_v170_tests tests/assembly_mate_document_v170_tests.cpp)
    target_link_libraries(assembly_mate_document_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_assembly_mate_document COMMAND assembly_mate_document_v170_tests)
    add_executable(mixed_feature_history_v170_tests tests/mixed_feature_history_v170_tests.cpp)
    target_link_libraries(mixed_feature_history_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_mixed_feature_history COMMAND mixed_feature_history_v170_tests)
    add_executable(curved_face_lineage_v170_tests tests/curved_face_lineage_v170_tests.cpp)
    target_link_libraries(curved_face_lineage_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_curved_face_lineage COMMAND curved_face_lineage_v170_tests)
    add_executable(face_lineage_v170_tests tests/face_lineage_v170_tests.cpp)
    target_link_libraries(face_lineage_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_face_lineage COMMAND face_lineage_v170_tests)
    add_executable(inplace_sketch_v170_tests tests/inplace_sketch_v170_tests.cpp)
    target_link_libraries(inplace_sketch_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_inplace_sketch_features COMMAND inplace_sketch_v170_tests operations)
    add_test(NAME cadcore_v170_inplace_sketch_persistence COMMAND inplace_sketch_v170_tests persistence)
    add_test(NAME cadcore_v170_inplace_two_stage COMMAND inplace_sketch_v170_tests two-stage)
    add_test(NAME cadcore_v170_inplace_two_stage_persistence COMMAND inplace_sketch_v170_tests two-stage-persistence)
    add_test(NAME cadcore_v170_inplace_two_stage_corruption COMMAND inplace_sketch_v170_tests two-stage-corruption)
    add_test(NAME cadcore_v170_inplace_ordered_stack COMMAND inplace_sketch_v170_tests ordered-stack)
    add_test(NAME cadcore_v170_inplace_ordered_stack_persistence COMMAND inplace_sketch_v170_tests ordered-stack-persistence)
    add_test(NAME cadcore_v170_inplace_ordered_stack_corruption COMMAND inplace_sketch_v170_tests ordered-stack-corruption)
    add_test(NAME cadcore_v170_inplace_ordered_stack_rollback COMMAND inplace_sketch_v170_tests ordered-stack-rollback)
    if(MSVC)
        foreach(native_target assembly_mate_solver_v170_tests assembly_mate_document_v170_tests
                              face_lineage_v170_tests inplace_sketch_v170_tests)
            target_compile_options(${native_target} PRIVATE /utf-8 /W4)
        endforeach()
    endif()
    add_executable(assembly_fit_policy_v170_tests tests/assembly_fit_policy_v170_tests.cpp)
    target_include_directories(assembly_fit_policy_v170_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(assembly_fit_policy_v170_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v170_assembly_fit_policy COMMAND assembly_fit_policy_v170_tests)
    add_executable(assembly_fit_audit_v170_tests tests/assembly_fit_audit_v170_tests.cpp)
    target_link_libraries(assembly_fit_audit_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_assembly_fit_audit COMMAND assembly_fit_audit_v170_tests)
    add_test(NAME cadcore_v170_assembly_fit_report_snapshot COMMAND assembly_fit_audit_v170_tests snapshot)
    if(MSVC)
        target_compile_options(assembly_fit_policy_v170_tests PRIVATE /utf-8 /W4)
        target_compile_options(assembly_fit_audit_v170_tests PRIVATE /utf-8 /W4)
    endif()
    # Source-linked sketch extrusions: real OCCT solids, propagation, rollback,
    # persistence and corruption rejection. Two separately reported CTests.
    add_executable(linked_sketch_extrusion_v170_tests tests/linked_sketch_extrusion_v170_tests.cpp)
    target_link_libraries(linked_sketch_extrusion_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_linked_sketch_chain COMMAND linked_sketch_extrusion_v170_tests operations)
    add_test(NAME cadcore_v170_linked_sketch_persistence COMMAND linked_sketch_extrusion_v170_tests persistence)
    add_test(NAME cadcore_v170_linked_sketch_retarget COMMAND linked_sketch_extrusion_v170_tests retarget)
    add_test(NAME cadcore_v170_associative_sketch_pocket COMMAND linked_sketch_extrusion_v170_tests pocket)
    add_test(NAME cadcore_v170_associative_sketch_pocket_persistence COMMAND linked_sketch_extrusion_v170_tests pocket-persistence)
    add_test(NAME cadcore_v170_associative_sketch_boss COMMAND linked_sketch_extrusion_v170_tests boss)
    add_test(NAME cadcore_v170_associative_sketch_boss_persistence COMMAND linked_sketch_extrusion_v170_tests boss-persistence)
    if(MSVC)
        target_compile_options(linked_sketch_extrusion_v170_tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(linked_body_graph_v170_tests
        tests/linked_body_graph_v170_tests.cpp src/core/LinkedBodyGraph.cpp)
    target_include_directories(linked_body_graph_v170_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(linked_body_graph_v170_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v170_linked_body_graph_planner COMMAND linked_body_graph_v170_tests)
    add_executable(linked_body_edit_v170_tests tests/linked_body_edit_v170_tests.cpp)
    target_link_libraries(linked_body_edit_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_linked_body_edit COMMAND linked_body_edit_v170_tests edit)
    add_test(NAME cadcore_v170_linked_body_persistence COMMAND linked_body_edit_v170_tests persistence)
    if(MSVC)
        target_compile_options(linked_body_graph_v170_tests PRIVATE /utf-8 /W4)
        target_compile_options(linked_body_edit_v170_tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(linked_body_batch_v170_tests tests/linked_body_batch_v170_tests.cpp)
    target_link_libraries(linked_body_batch_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_atomic_batch_edit COMMAND linked_body_batch_v170_tests batch)
    add_test(NAME cadcore_v170_atomic_batch_preview COMMAND linked_body_batch_v170_tests preview)
    add_test(NAME cadcore_v170_atomic_batch_persistence COMMAND linked_body_batch_v170_tests persistence)
    if(MSVC)
        target_compile_options(linked_body_batch_v170_tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(face_attached_sketch_v170_tests tests/face_attached_sketch_v170_tests.cpp)
    target_link_libraries(face_attached_sketch_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_face_attached_sketch COMMAND face_attached_sketch_v170_tests)
    if(MSVC)
        target_compile_options(face_attached_sketch_v170_tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(advanced_sketch_profile_v170_tests
        tests/advanced_sketch_profile_v170_tests.cpp
        src/core/AdvancedSketchProfiles.cpp src/core/PolygonFeatureProfile.cpp)
    target_include_directories(advanced_sketch_profile_v170_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(advanced_sketch_profile_v170_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v170_advanced_profile_validation COMMAND advanced_sketch_profile_v170_tests)
    add_executable(advanced_sketch_geometry_v170_tests tests/advanced_sketch_geometry_v170_tests.cpp)
    target_link_libraries(advanced_sketch_geometry_v170_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v170_advanced_sketch_geometry COMMAND advanced_sketch_geometry_v170_tests)
    if(MSVC)
        target_compile_options(advanced_sketch_profile_v170_tests PRIVATE /utf-8 /W4)
        target_compile_options(advanced_sketch_geometry_v170_tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(polygon_feature_profile_v167_tests
        tests/polygon_feature_profile_v167_tests.cpp src/core/PolygonFeatureProfile.cpp)
    target_include_directories(polygon_feature_profile_v167_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(polygon_feature_profile_v167_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v167_polygon_profile_validation COMMAND polygon_feature_profile_v167_tests)
    if(MSVC)
        target_compile_options(polygon_feature_profile_v167_tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(sketch_polygon_geometry_v167_tests tests/sketch_polygon_geometry_v167_tests.cpp)
    target_link_libraries(sketch_polygon_geometry_v167_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v167_sketch_polygon_features COMMAND sketch_polygon_geometry_v167_tests)
    # Retain the original 130-CTest inventory; extend the existing sketch test
    # with real offscreen QWidget regression checks rather than adding an extra CTest.
    add_executable(sketch_solver_tests
        tests/sketch_solver_tests.cpp tests/sketch_constraint_editor_cases.h
        src/ui/SketchCanvasDialog.cpp src/ui/SketchCanvasDialog.h)
    target_link_libraries(sketch_solver_tests PRIVATE cadcore Qt6::Widgets)
    add_test(NAME cadcore_sketch_solver COMMAND sketch_solver_tests)
    add_executable(surface_modeling_tests tests/surface_modeling_tests.cpp)
    target_link_libraries(surface_modeling_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_surface_modeling COMMAND surface_modeling_tests)
    add_executable(document_revision_history_tests tests/document_revision_history_tests.cpp)
    target_link_libraries(document_revision_history_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_document_revision_history COMMAND document_revision_history_tests)
    add_executable(mesh_repair_tests tests/mesh_repair_tests.cpp)
    target_link_libraries(mesh_repair_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_mesh_repair COMMAND mesh_repair_tests)
    add_executable(shape_feature_tree_tests tests/shape_feature_tree_tests.cpp)
    target_link_libraries(shape_feature_tree_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_universal_feature_tree COMMAND shape_feature_tree_tests)
    add_executable(dependency_sketch_v157_tests tests/dependency_sketch_v157_tests.cpp)
    target_link_libraries(dependency_sketch_v157_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_dependency_sketch_v157 COMMAND dependency_sketch_v157_tests)
    add_executable(associative_features_v159_tests tests/associative_features_v159_tests.cpp)
    target_link_libraries(associative_features_v159_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_associative_features_v159 COMMAND associative_features_v159_tests)
    add_executable(selected_edge_v160_tests tests/selected_edge_v160_tests.cpp)
    target_link_libraries(selected_edge_v160_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_selected_edge_v160 COMMAND selected_edge_v160_tests)
    add_executable(feature_dependency_plan_v166_tests
        tests/feature_dependency_plan_v166_tests.cpp src/core/FeatureDependencyPlan.cpp
        src/core/FeatureRebuildDiagnostics.cpp)
    target_include_directories(feature_dependency_plan_v166_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(feature_dependency_plan_v166_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v166_sparse_dependency_planner COMMAND feature_dependency_plan_v166_tests)
    add_executable(sparse_feature_regeneration_v166_tests tests/sparse_feature_regeneration_v166_tests.cpp)
    target_link_libraries(sparse_feature_regeneration_v166_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v166_sparse_feature_regeneration COMMAND sparse_feature_regeneration_v166_tests)
    if(MSVC)
        target_compile_options(feature_dependency_plan_v166_tests PRIVATE /utf-8 /W4)
        target_compile_options(sparse_feature_regeneration_v166_tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(incremental_feature_graph_v165_tests tests/incremental_feature_graph_v165_tests.cpp)
    target_link_libraries(incremental_feature_graph_v165_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v165_incremental_feature_graph COMMAND incremental_feature_graph_v165_tests)
    if(MSVC)
        target_compile_options(incremental_feature_graph_v165_tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(feature_upgrade_v158_tests tests/feature_upgrade_v158_tests.cpp)
    target_link_libraries(feature_upgrade_v158_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_feature_upgrade_v158 COMMAND feature_upgrade_v158_tests)
    add_executable(document_audit_tests tests/document_audit_tests.cpp)
    target_link_libraries(document_audit_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_document_audit COMMAND document_audit_tests)
    add_executable(engineering_design_tools_tests tests/engineering_design_tools_tests.cpp)
    target_link_libraries(engineering_design_tools_tests PRIVATE cadcore)
    add_test(NAME cadcore_engineering_preliminary_design COMMAND engineering_design_tools_tests)
    add_executable(parametric_study_v170_tests tests/parametric_study_v170_tests.cpp src/core/ParametricStudy.cpp)
    target_include_directories(parametric_study_v170_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME cadcore_v170_parametric_study COMMAND parametric_study_v170_tests)
    add_executable(guided_engineering_studies_v170_tests tests/guided_engineering_studies_v170_tests.cpp src/core/GuidedEngineeringStudies.cpp)
    target_include_directories(guided_engineering_studies_v170_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME cadcore_v170_guided_engineering_studies COMMAND guided_engineering_studies_v170_tests)
    add_executable(mass_tensor_math_v163_tests tests/mass_tensor_math_v163_tests.cpp src/core/MassTensorAnalysis.cpp)
    target_include_directories(mass_tensor_math_v163_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(mass_tensor_math_v163_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v163_inertia_math COMMAND mass_tensor_math_v163_tests)
    add_executable(mass_tensor_scene_v163_tests tests/mass_tensor_scene_v163_tests.cpp)
    target_link_libraries(mass_tensor_scene_v163_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v163_inertia_scene COMMAND mass_tensor_scene_v163_tests)
    if(MSVC)
        target_compile_options(mass_tensor_math_v163_tests PRIVATE /utf-8 /W4)
        target_compile_options(mass_tensor_scene_v163_tests PRIVATE /utf-8 /W4)
    endif()
    # These two portable test executables deliberately do not link Qt.
    set_target_properties(parametric_study_v170_tests guided_engineering_studies_v170_tests PROPERTIES AUTOMOC OFF)
    if(MSVC)
        target_compile_options(parametric_study_v170_tests PRIVATE /utf-8 /W4)
        target_compile_options(guided_engineering_studies_v170_tests PRIVATE /utf-8 /W4)
    endif()
    # Portable workshop calculations, three separately discoverable acceptance suites.
    add_executable(workshop_engineering_v164_tests
        tests/workshop_engineering_v164_tests.cpp src/core/WorkshopEngineering.cpp)
    target_include_directories(workshop_engineering_v164_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(workshop_engineering_v164_tests PROPERTIES AUTOMOC OFF)
    add_test(NAME cadcore_v164_section_properties COMMAND workshop_engineering_v164_tests sections)
    add_test(NAME cadcore_v164_machining_estimates COMMAND workshop_engineering_v164_tests machining)
    add_test(NAME cadcore_v164_tolerance_stacks COMMAND workshop_engineering_v164_tests tolerances)
    add_executable(workshop_geometry_v164_tests tests/workshop_geometry_v164_tests.cpp)
    target_link_libraries(workshop_geometry_v164_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_v164_cylindrical_inventory COMMAND workshop_geometry_v164_tests)
    if(MSVC)
        target_compile_options(workshop_engineering_v164_tests PRIVATE /utf-8 /W4)
        target_compile_options(workshop_geometry_v164_tests PRIVATE /utf-8 /W4)
    endif()
    add_executable(cadcore_tests tests/cadcore_tests.cpp)
    target_link_libraries(cadcore_tests PRIVATE cadcore Qt6::Core)
    add_test(NAME cadcore_modeling_tools COMMAND cadcore_tests --modeling-tools)
    add_test(NAME cadcore_wing_rib COMMAND cadcore_tests --wing-rib)
    add_test(NAME cadcore_sweeps COMMAND cadcore_tests --sweeps)
    add_test(NAME cadcore_tapered_helix COMMAND cadcore_tests --tapered-helix)
    add_test(NAME cadcore_form_features COMMAND cadcore_tests --form-features)
    add_test(NAME cadcore_vector_pattern COMMAND cadcore_tests --vector-pattern)
    add_test(NAME cadcore_vector_rectangular_pattern COMMAND cadcore_tests --vector-rectangular-pattern)
    add_test(NAME cadcore_minimum_distance COMMAND cadcore_tests --minimum-distance)
    add_test(NAME cadcore_interference COMMAND cadcore_tests --interference)
    add_test(NAME cadcore_body_diagnostics COMMAND cadcore_tests --body-diagnostics)
    add_test(NAME cadcore_density_mass_properties COMMAND cadcore_tests --density-mass-properties)
    add_test(NAME cadcore_body_materials COMMAND cadcore_tests --body-materials)
    add_test(NAME cadcore_material_engineering COMMAND cadcore_tests --material-engineering)
    add_test(NAME cadcore_planar_section COMMAND cadcore_tests --planar-section)
    add_test(NAME cadcore_surface_summary COMMAND cadcore_tests --surface-summary)
    add_test(NAME cadcore_face_area COMMAND cadcore_tests --face-area)
    add_test(NAME cadcore_face_angle COMMAND cadcore_tests --face-angle)
    add_test(NAME cadcore_cylindrical_face COMMAND cadcore_tests --cylindrical-face)
    add_test(NAME cadcore_edge_length COMMAND cadcore_tests --edge-length)
    add_test(NAME cadcore_vertex_coordinates COMMAND cadcore_tests --vertex-coordinates)
    add_test(NAME cadcore_vertex_distance COMMAND cadcore_tests --vertex-distance)
    add_test(NAME cadcore_circular_hole_pattern COMMAND cadcore_tests --circular-hole-pattern)
    add_test(NAME cadcore_helical_sweep COMMAND cadcore_tests --helical-sweep)
    add_test(NAME cadcore_variable_pitch_helix COMMAND cadcore_tests --variable-pitch-helix)
    add_test(NAME cadcore_dovetail_groove COMMAND cadcore_tests --dovetail-groove)
    add_test(NAME cadcore_t_slot_groove COMMAND cadcore_tests --t-slot-groove)
    add_test(NAME cadcore_rounded_slot COMMAND cadcore_tests --rounded-slot)
    add_test(NAME cadcore_keyhole_slot COMMAND cadcore_tests --keyhole-slot)
    add_test(NAME cadcore_rounded_slot_pattern COMMAND cadcore_tests --rounded-slot-pattern)
    add_test(NAME cadcore_circular_countersink_pattern COMMAND cadcore_tests --circular-countersink-pattern)
    add_test(NAME cadcore_circular_counterbore_pattern COMMAND cadcore_tests --circular-counterbore-pattern)
    add_test(NAME cadcore_linear_keyhole_slot_pattern COMMAND cadcore_tests --linear-keyhole-slot-pattern)
    add_test(NAME cadcore_linear_counterbore_pattern COMMAND cadcore_tests --linear-counterbore-pattern)
    add_test(NAME cadcore_linear_countersink_pattern COMMAND cadcore_tests --linear-countersink-pattern)
    add_test(NAME cadcore_elliptical_loft COMMAND cadcore_tests --elliptical-loft)
    add_test(NAME cadcore_nonuniform_scale COMMAND cadcore_tests --nonuniform-scale)
    add_test(NAME cadcore_custom_axis_circular_pattern COMMAND cadcore_tests --custom-axis-circular-pattern)
    add_test(NAME cadcore_asymmetric_chamfer COMMAND cadcore_tests --asymmetric-chamfer)
    add_test(NAME cadcore_multi_edge_blends COMMAND cadcore_tests --multi-edge-blends)
    add_test(NAME cadcore_oriented_extrusion COMMAND cadcore_tests --oriented-extrusion)
    add_test(NAME cadcore_custom_plane_extrusion COMMAND cadcore_tests --custom-plane-extrusion)
    add_test(NAME cadcore_custom_axis_revolve COMMAND cadcore_tests --custom-axis-revolve)
    if(MSVC)
        target_compile_options(cadcore_tests PRIVATE /utf-8 /W4)
    endif()
    add_test(NAME cadcore_geometry_and_persistence COMMAND cadcore_tests)
    add_test(NAME cadcore_body_schedule_csv COMMAND cadcore_tests --body-schedule-csv)
    add_test(NAME cadcore_bom_parts_standards COMMAND cadcore_tests --bom-parts-standards)
    add_test(NAME cadcore_persistent_assemblies COMMAND cadcore_tests --persistent-assemblies)
    add_test(NAME cadcore_component_occurrence_patterns COMMAND cadcore_tests --component-occurrence-patterns)
    add_test(NAME cadcore_occurrence_bom_metadata COMMAND cadcore_tests --occurrence-bom-metadata)
    add_test(NAME cadcore_assembly_bom_group_keys COMMAND cadcore_tests --assembly-bom-group-keys)
    add_test(NAME cadcore_assembly_bom_full_scene COMMAND cadcore_tests --assembly-bom-full-scene)
    add_test(NAME cadcore_assembly_free_occurrence_yaw COMMAND cadcore_tests --assembly-free-occurrence-yaw)
    add_test(NAME cadcore_assembly_joint_coordinate_guards COMMAND cadcore_tests --assembly-joint-coordinate-guards)
    add_test(NAME cadcore_assembly_joint_persistence_guards COMMAND cadcore_tests --assembly-joint-persistence-guards)
    add_test(NAME cadcore_assembly_sync_revolute_guard COMMAND cadcore_tests --assembly-sync-revolute-guard)
    add_test(NAME cadcore_assembly_occurrence_pose_copy COMMAND cadcore_tests --assembly-occurrence-pose-copy)
    add_test(NAME cadcore_step_assembly_import COMMAND cadcore_tests --step-assembly-import)
    add_test(NAME cadcore_stl_mesh_import COMMAND cadcore_tests --stl-mesh-import)
    add_test(NAME cadcore_iges_exchange COMMAND cadcore_tests --iges-exchange)
    add_test(NAME cadcore_obj_mesh_import COMMAND cadcore_tests --obj-mesh-import)
    add_test(NAME cadcore_parametric_foundation COMMAND cadcore_tests --parametric-foundation)
    add_test(NAME cadcore_parametric_links COMMAND cadcore_tests --parametric-links)
    add_test(NAME cadcore_parametric_blind_hole COMMAND cadcore_tests --parametric-blind-hole)
    add_test(NAME cadcore_parametric_feature_suppression COMMAND cadcore_tests --parametric-feature-suppression)
    add_test(NAME cadcore_surface_body_persistence COMMAND cadcore_tests --surface-body-persistence)
    add_test(NAME cadcore_assembly_joints COMMAND cadcore_tests --assembly-joints)
    add_test(NAME cadcore_sheet_metal_flat_pattern COMMAND cadcore_tests --sheet-metal-flat-pattern)
    add_test(NAME cadcore_folded_sheet_metal COMMAND cadcore_tests --folded-sheet-metal)
    add_test(NAME cadcore_orthographic_drawing COMMAND cadcore_tests --orthographic-drawing)
endif()

# Manual Windows install: deploy Qt with windeployqt + OCCT DLLs from vcpkg.
# Do not upload DLL binaries of third-party libraries as source code.

# Keep local Windows builds launchable and run GUI-backed core tests headlessly.
if(WIN32)
    set(_cadworkbench_qt_platform_plugins "")
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_cadworkbench_qt_platform_plugins
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/Qt6/plugins/platforms")
    endif()
    if(NOT EXISTS "${_cadworkbench_qt_platform_plugins}/qwindows.dll")
        get_target_property(_cadworkbench_qt_core_dll Qt6::Core IMPORTED_LOCATION_RELEASE)
        if(NOT _cadworkbench_qt_core_dll)
            get_target_property(_cadworkbench_qt_core_dll Qt6::Core IMPORTED_LOCATION)
        endif()
        if(_cadworkbench_qt_core_dll)
            get_filename_component(_cadworkbench_qt_bin_dir "${_cadworkbench_qt_core_dll}" DIRECTORY)
            get_filename_component(_cadworkbench_qt_triplet_dir "${_cadworkbench_qt_bin_dir}" DIRECTORY)
            set(_cadworkbench_qt_platform_plugins "${_cadworkbench_qt_triplet_dir}/Qt6/plugins/platforms")
        endif()
    endif()
    if(EXISTS "${_cadworkbench_qt_platform_plugins}/qwindows.dll")
        add_custom_command(TARGET CADWorkbench POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CADWorkbench>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_cadworkbench_qt_platform_plugins}"
                "$<TARGET_FILE_DIR:CADWorkbench>/platforms")
        if(BUILD_TESTING)
            add_custom_command(TARGET cadcore_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:cadcore_tests>/platforms"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_cadworkbench_qt_platform_plugins}"
                    "$<TARGET_FILE_DIR:cadcore_tests>/platforms")
            get_property(_cadworkbench_tests DIRECTORY PROPERTY TESTS)
            set_tests_properties(${_cadworkbench_tests} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${_cadworkbench_qt_platform_plugins}")
        endif()
    else()
        message(WARNING "Qt Windows platform plugins were not found; CADWorkbench.exe may fail to launch and Qt tests may show a platform-plugin dialog.")
    endif()
endif()


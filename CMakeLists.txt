cmake_minimum_required(VERSION 3.24)
project(CADWorkbench VERSION 1.55.0 LANGUAGES CXX)

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
    src/core/PartsData.cpp
    src/core/DrawingSheet.cpp
    src/core/DrawingDxf.cpp
    src/core/SheetMetal.h
    src/core/SheetMetal.cpp
    src/core/SurfaceModeling.h
    src/core/SurfaceModeling.cpp
    src/core/DocumentRevisionHistory.h
    src/core/DocumentRevisionHistory.cpp
    src/core/MeshRepair.h
    src/core/MeshRepair.cpp
    src/core/SketchSolver.h
    src/core/SketchSolver.cpp
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
    target_compile_options(CADWorkbench PRIVATE /utf-8 /W4)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(sketch_solver_tests tests/sketch_solver_tests.cpp)
    target_link_libraries(sketch_solver_tests PRIVATE cadcore Qt6::Core)
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


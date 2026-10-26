cmake_minimum_required(VERSION 3.24)
project(CADWorkbench VERSION 1.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if (NOT WIN32)
    message(FATAL_ERROR "v1.7 UI targets Windows (Win32 HWND). Run on Windows 10/11 with MSVC.")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
find_package(OpenCASCADE 8.0.1 REQUIRED CONFIG)

add_library(cadcore STATIC
    src/core/SceneDocument.h
    src/core/SceneDocument.cpp
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
    Qt6::Core Qt6::Gui
    TKernel TKMath TKG2d TKG3d TKBRep TKGeomBase TKGeomAlgo
    TKPrim TKTopAlgo TKBO TKBool TKOffset TKFillet TKShHealing TKMesh
    TKDESTEP TKDESTL TKXSBase
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
target_compile_definitions(CADWorkbench PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    # UTF-8 source includes the degree/delta symbols used in tool dialogs.
    target_compile_options(cadcore PRIVATE /utf-8 /W4)
    target_compile_options(CADWorkbench PRIVATE /utf-8 /W4)
endif()

include(CTest)
if(BUILD_TESTING)
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
    add_test(NAME cadcore_planar_section COMMAND cadcore_tests --planar-section)
    add_test(NAME cadcore_surface_summary COMMAND cadcore_tests --surface-summary)
    add_test(NAME cadcore_face_area COMMAND cadcore_tests --face-area)
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
    if(MSVC)
        target_compile_options(cadcore_tests PRIVATE /utf-8 /W4)
    endif()
    add_test(NAME cadcore_geometry_and_persistence COMMAND cadcore_tests)
endif()

# Manual Windows install: deploy Qt with windeployqt + OCCT DLLs from vcpkg.
# Do not upload DLL binaries of third-party libraries as source code.


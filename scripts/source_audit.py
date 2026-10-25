"""Dependency-free static source checks. These DO NOT verify compilation or native behavior."""
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
checks = {
    'version': ('CMakeLists.txt', 'VERSION 1.9.0'),
    'qt': ('CMakeLists.txt', 'Qt6::Widgets'),
    'occt': ('CMakeLists.txt', 'find_package(OpenCASCADE'),
    'core_geometry': ('src/core/SceneDocument.cpp', 'BRepCheck_Analyzer'),
    'core_v1_migration': ('src/core/SceneDocument.cpp', 'cadworkbench.scene.v1'),
    'core_v2_format': ('src/core/SceneDocument.cpp', 'cadworkbench.scene.v2'),
    'transactional_save': ('src/core/SceneDocument.cpp', 'QSaveFile'),
    'validity_on_edit': ('src/core/SceneDocument.cpp', 'validShape(result, error)'),
    'undo_branch': ('src/core/SceneDocument.cpp', 'redo_.clear();'),
    'atomic_delete': ('src/core/SceneDocument.cpp', 'eraseMany'),
    'selection_modes': ('src/ui/CadViewport.cpp', 'AIS_Shape::SelectionMode'),
    'edit_mode_guard': ('src/ui/MainWindow.cpp', 'requireBodyMode'),
    'grid': ('src/ui/CadViewport.cpp', 'ActivateGrid'),
    'camera': ('src/ui/CadViewport.cpp', 'fitSelection'),
    'display_visibility': ('src/ui/CadViewport.cpp', 'if (!part.visible) continue;'),
    'edge_reference': ('src/ui/CadViewport.cpp', 'selectedEdgeReferences'),
    'edge_fillet': ('src/core/SceneDocument.cpp', 'BRepFilletAPI_MakeFillet'),
    'fillet_preview': ('src/core/SceneDocument.cpp', 'buildFilletPreview'),
    'fillet_commit': ('src/core/SceneDocument.cpp', 'commitFillet'),
    'edge_chamfer': ('src/core/SceneDocument.cpp', 'BRepFilletAPI_MakeChamfer'),
    'chamfer_preview': ('src/core/SceneDocument.cpp', 'buildChamferPreview'),
    'chamfer_commit': ('src/core/SceneDocument.cpp', 'commitChamfer'),
    'sketch_extrusion': ('src/core/SceneDocument.cpp', 'BRepPrimAPI_MakePrism'),
    'extrusion_preview': ('src/core/SceneDocument.cpp', 'buildExtrusionPreview'),
    'extrusion_commit': ('src/core/SceneDocument.cpp', 'commitExtrusion'),
    'extrusion_modes': ('src/core/SceneDocument.h', 'enum class ExtrusionMode'),
    'extrusion_preview_copy': ('src/core/SceneDocument.cpp', 'BRepBuilderAPI_Copy(target->shape).Shape()'),
    'independent_history_snapshots': ('src/core/SceneDocument.cpp', 'snapshotParts = parts_'),
    'profile_revolve': ('src/core/SceneDocument.cpp', 'BRepPrimAPI_MakeRevol'),
    'mirror': ('src/core/SceneDocument.cpp', 'bool SceneDocument::mirror'),
    'linear_pattern': ('src/core/SceneDocument.cpp', 'bool SceneDocument::linearPattern'),
    'circular_pattern': ('src/core/SceneDocument.cpp', 'bool SceneDocument::circularPattern'),
    'hole_preview': ('src/core/SceneDocument.cpp', 'buildHolePreview'),
    'hole_commit': ('src/core/SceneDocument.cpp', 'commitHole'),
    'split_body': ('src/core/SceneDocument.cpp', 'bool SceneDocument::splitBody'),
    'shell_body': ('src/core/SceneDocument.cpp', 'bool SceneDocument::shellBody'),
    'cad_ribbon_modify': ('src/ui/MainWindow.cpp', 'addRibbonTab("Modify"'),
    'cad_ribbon_inspect': ('src/ui/MainWindow.cpp', 'addRibbonTab("Inspect"'),
    'mass_properties': ('src/ui/MainWindow.cpp', 'BRepGProp::SurfaceProperties(part->shape,areaProperties)'),
    'shell_kernel_link': ('CMakeLists.txt', 'TKOffset'),
    'async_preview': ('src/ui/MainWindow.cpp', 'QtConcurrent::run'),
    'qt_runtime_plugins': ('scripts/build_windows.ps1', 'windeployqt'),
    'stale_edge_guard': ('src/core/SceneDocument.cpp', 'selected edge is stale'),
    'fillet_link': ('CMakeLists.txt', 'TKFillet'),
    'native_tests': ('tests/cadcore_tests.cpp', 'void execute()'),
    'windows_ci': ('.github/workflows/windows-ci.yml', 'windows-2022'),
    'engineering_component_builder': ('src/core/EngineeringComponents.cpp', 'bool SceneDocument::addEngineeringPart'),
    'engineering_specs': ('src/core/EngineeringComponents.cpp', 'case P::FanImpeller'),
    'engineering_ui': ('src/ui/MainWindow.cpp', 'Mechanical Components'),
    'engineering_regression': ('tests/cadcore_tests.cpp', '30 v1.3 engineering components'),
    'advanced_component_builder': ('src/core/AdvancedComponents.cpp', 'bool SceneDocument::addAdvancedPart'),
    'advanced_component_specs': ('src/core/AdvancedComponents.cpp', 'case A::UniversalJoint'),
    'advanced_spring_sweep': ('src/core/AdvancedComponents.cpp', 'BRepOffsetAPI_MakePipe'),
    'advanced_component_ui': ('src/ui/MainWindow.cpp', 'Advanced Components'),
    'advanced_component_regression': ('tests/cadcore_tests.cpp', '30 v1.4 advanced components'),
    'specialized_component_enum': ('src/core/SceneDocument.h', 'enum class SpecializedPart'),
    'specialized_component_builder': ('src/core/SpecializedComponents.cpp', 'bool SceneDocument::addSpecializedPart'),
    'specialized_component_specs': ('src/core/SpecializedComponents.cpp', 'case S::CoolingFan'),
    'specialized_component_ui': ('src/ui/MainWindow.cpp', 'Specialized Components'),
    'specialized_component_regression': ('tests/cadcore_tests.cpp', '35 v1.5 specialized components'),
    'industrial_component_enum': ('src/core/SceneDocument.h', 'enum class IndustrialPart'),
    'industrial_component_builder': ('src/core/IndustrialComponents.cpp', 'bool SceneDocument::addIndustrialPart'),
    'industrial_component_specs': ('src/core/IndustrialComponents.cpp', 'Pipe hanger'),
    'industrial_component_ui': ('src/ui/MainWindow.cpp', 'Industrial Components'),
    'industrial_component_regression': ('tests/cadcore_tests.cpp', '70 v1.6 industrial components'),
    'extended_component_enum': ('src/core/SceneDocument.h', 'enum class ExtendedPart'),
    'extended_component_builder': ('src/core/ExtendedComponents.cpp', 'bool SceneDocument::addExtendedPart'),
    'extended_component_specs': ('src/core/ExtendedComponents.cpp', 'Sampling port housing'),
    'extended_component_ui': ('src/ui/MainWindow.cpp', 'Extended Components'),
    'extended_component_regression': ('tests/cadcore_tests.cpp', '70 v1.7 extended components'),
    'precision_component_enum': ('src/core/SceneDocument.h', 'enum class PrecisionPart'),
    'precision_component_builder': ('src/core/PrecisionComponents.cpp', 'bool SceneDocument::addPrecisionPart'),
    'precision_component_specs': ('src/core/PrecisionComponents.cpp', 'Drone motor mount'),
    'precision_component_ui': ('src/ui/MainWindow.cpp', 'Precision && Utility Components'),
    'production_component_enum': ('src/core/SceneDocument.h', 'enum class ProductionPart'),
    'production_component_builder': ('src/core/ProductionComponents.cpp', 'bool SceneDocument::addProductionPart'),
    'production_component_specs': ('src/core/ProductionComponents.cpp', 'Climbing anchor plate'),
    'production_component_ui': ('src/ui/MainWindow.cpp', 'Production Components'),
    'production_component_regression': ('tests/cadcore_tests.cpp', '100 v1.9 production components'),
    'modeling_tool_enum': ('src/core/SceneDocument.h', 'enum class ModelingTool'),
    'modeling_tool_builder': ('src/core/ModelingTools.cpp', 'bool SceneDocument::addModelingTool'),
    'modeling_tool_specifications': ('src/core/ModelingTools.cpp', 'Coil spring'),
    'modeling_tool_ui_menu': ('src/ui/MainWindow.cpp', 'Modeling Tools'),
    'modeling_tool_regression': ('tests/cadcore_tests.cpp', 'testModelingTools()'),
    'modeling_tool_documentation': ('docs/MODELING_TOOLS.md', 'PASS: 210 assertions across 30 modeling tools'),
    'feature_gap_counts': ('docs/FEATURE_GAP_COMPARISON.md', '6 capability areas implemented, 3 partial, and **21 not yet implemented**'),
    'feature_gap_30th_area': ('docs/FEATURE_GAP_COMPARISON.md', '30 | CAD standards and standards checking'),
}
problems=[]
for title,(filename,token) in checks.items():
    path=ROOT/filename
    if not path.is_file() or token not in path.read_text(encoding='utf-8'):
        problems.append(title)
cpp=list((ROOT/'src').rglob('*.cpp'))+list((ROOT/'src').rglob('*.h'))
for path in cpp:
    src=path.read_text(encoding='utf-8')
    if re.search(r'(?m)^\s*#include\s+["<]\.\./',src):
        problems.append('invalid_parent_include:'+path.name)
report={
    'audit': 'textual repository contract only; no compilation',
    'source_checks': len(checks),
    'passed': not problems,
    'failed': problems,
}
print(json.dumps(report,indent=2))
sys.exit(bool(problems))


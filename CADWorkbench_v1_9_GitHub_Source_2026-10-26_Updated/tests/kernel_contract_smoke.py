"""Independent geometry contract checks against OCP/CadQuery (not the Windows C++ binary).

Usage: python tests/kernel_contract_smoke.py
Results never imply native GUI or C++ compilation success.
"""
import json
import math
import pathlib
import tempfile

import cadquery as cq
from OCP.BRepCheck import BRepCheck_Analyzer


def close(got, want, rel=1e-5):
    assert math.isclose(got, want, rel_tol=rel, abs_tol=1e-7), (got, want)


def run():
    checks = {}
    box = cq.Solid.makeBox(10, 20, 30)
    cylinder = cq.Solid.makeCylinder(3, 10, cq.Vector(4, 4, 0))
    sphere = cq.Solid.makeSphere(4, angleDegrees1=-90, angleDegrees2=90)
    cone = cq.Solid.makeCone(6, 2, 10)
    torus = cq.Solid.makeTorus(12, 2)

    close(box.Volume(), 6000)
    close(cylinder.Volume(), math.pi * 3**2 * 10)
    close(sphere.Volume(), 4 * math.pi * 4**3 / 3)
    close(cone.Volume(), math.pi * 10 * (6**2 + 6 * 2 + 2**2) / 3)
    close(torus.Volume(), 2 * math.pi**2 * 12 * 2**2)
    checks['primitive_volume_five_types'] = 'PASS'

    for shape in (box, cylinder, sphere, cone, torus):
        assert BRepCheck_Analyzer(shape.wrapped).IsValid()
    checks['topology_five_types'] = 'PASS'

    moved = cone.translate((12, 0, 0))
    rotated = cone.rotate((0, 0, 0), (0, 0, 1), 45)
    scaled = cone.scale(2)
    close(moved.Volume(), cone.Volume())
    close(rotated.Volume(), cone.Volume())
    close(scaled.Volume(), 8 * cone.Volume())
    checks['translate_rotate_scale_volumes'] = 'PASS'

    cut = box.cut(cylinder)
    close(cut.Volume(), 6000 - 90 * math.pi, rel=1e-3)
    assert cut.isValid()
    checks['boolean_cut'] = 'PASS'

    union = box.fuse(cylinder)
    common = box.intersect(cylinder)
    close(union.Volume(), box.Volume())
    close(common.Volume(), cylinder.Volume())
    checks['boolean_fuse_intersect'] = 'PASS'

    with tempfile.TemporaryDirectory() as d:
        step = pathlib.Path(d, 'part.step')
        stl = pathlib.Path(d, 'part.stl')
        cq.exporters.export(cut, str(step), exportType='STEP')
        cq.exporters.export(cut, str(stl), exportType='STL')
        assert step.stat().st_size > 100 and stl.stat().st_size > 84
        imported = cq.importers.importStep(str(step)).val()
        close(imported.Volume(), cut.Volume(), rel=1e-3)
    checks['step_roundtrip_and_stl'] = 'PASS'
    return checks


if __name__ == '__main__':
    output = {
        'test_type': 'independent_OCP_kernel_geometry_reference',
        'native_Cpp_app_compiled': False,
        'native_Windows_UI_tested': False,
        'purpose': 'Underlying CAD operations reference, not a substitute for C++ regression tests',
        'checks': run(),
    }
    path = pathlib.Path(__file__).resolve().parents[1] / 'docs/kernel_reference_v02.json'
    path.write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps(output, indent=2))

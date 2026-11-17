"""Independent OpenCascade reference for generated pocket/cutter fixtures."""
import cadquery as cq

def check(condition, desc):
    assert condition, desc
    print('PASS', desc)
# CADWorkbench tests use a sketch rectangle at XY x=[0,w], y=[0,10],
# and a target box x=[0,12],y=[0,12], z=[0,10].
def cutter(width, height=10, depth=5, offset=8):
    return cq.Solid.makeBox(width,height,depth,cq.Vector(0,0,offset))
target=cq.Solid.makeBox(12,12,10)
check(abs(target.Volume()-1440)<1e-6,'target volume 1440 mm3')
for width,depth,offset,expect in [(10,5,8,1240),(8,5,8,1280),(8,4,7,1200),(9,5,8,1260)]:
    tool=cutter(width,depth=depth,offset=offset)
    cut=target.cut(tool)
    check(cut.isValid(),f'valid cut geometry w={width}, depth={depth}, offset={offset}')
    check(abs(cut.Volume()-expect)<1e-6,f'cut volume {expect} mm3')
disjoint=cutter(10,depth=5,offset=30)
check(abs(target.cut(disjoint).Volume()-target.Volume())<1e-6,'nonintersecting cutter has zero removal')
for width,expect_cut in [(8,840),(9,820)]:
    original=cq.Solid.makeBox(10,10,10)
    tool=cutter(width)
    check(abs(original.cut(tool).Volume()-expect_cut)<1e-6,f'retarget source width {width} and original target')
original=cq.Solid.makeBox(15,10,10)
check(abs(original.cut(cutter(8)).Volume()-1340)<1e-6,'changed old source / independent retarget branch')
check(abs(original.cut(cutter(9)).Volume()-1320)<1e-6,'changed retargeted sketch branch')

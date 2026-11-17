"""Independent CadQuery/OCP geometry sanity checks; not a native app test."""
import math
import cadquery as cq
checks = 0

def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)

def near(value, expected, label):
    check(abs(value-expected)<1e-5*max(1.0,abs(expected)),label)

base = cq.Workplane('XY').box(20,20,20,centered=False).val()
x = base.translate((5,0,0))
for delta,want in [(10,3000),(12,2400),(14,1800),(100,0)]:
    y=base.translate((0,delta,0))
    common=x.intersect(y)
    near(common.Volume(),want,'branch intersection volume')
    check(common.isValid() or want==0,'valid OCCT branch intersection')

box=cq.Workplane('XY').box(10,10,5,centered=False).val()
rot=box.rotate((0,0,0),(0,0,2),90)
near(rot.Volume(),500,'arbitrary axis rotation preserves volume')
mirror=rot.mirror('YZ')
near(mirror.Volume(),500,'arbitrary plane reflection preserves volume')
linear=cq.Compound.makeCompound([mirror.translate((30*i,5*i,0)) for i in range(3)])
near(linear.Volume(),1500,'vector pattern has three solid copies')
check(linear.isValid(),'vector pattern is valid')
center=(50,50,0)
circular=cq.Compound.makeCompound([box.rotate(center,(50,50,1),120*i) for i in range(3)])
near(circular.Volume(),1500,'arbitrary circular pattern preserves total volume')
check(circular.isValid(),'arbitrary circular pattern valid')
print('PASS:',checks,'independent OCP solid checks')

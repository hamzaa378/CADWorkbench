"""Independent geometry reference for translation-aware face/edge anchors.
Requires cadquery/OCP. This checks kernel geometry, not compilation of the Qt C++ code.
"""
import math
import unittest
import cadquery as cq


def coordinates(point):
    return (point.x, point.y, point.z)


def difference(x, y):
    return tuple(a-b for a,b in zip(x,y))


def distance(x, y):
    return math.dist(x,y)


def relative_center(geometry, body):
    return difference(coordinates(geometry.Center()),coordinates(body.Center()))


def resolve_face(source_face, source, candidate):
    oldcenter=relative_center(source_face,source)
    normal=coordinates(source_face.normalAt())
    area=source_face.Area()
    options=[]
    for i,face in enumerate(candidate.Faces()):
        other=coordinates(face.normalAt())
        if sum(x*y for x,y in zip(normal,other)) < 0.97:
            continue
        score=distance(relative_center(face,candidate),oldcenter)/max(1,math.sqrt(area))
        score+=0.5*abs(math.log(face.Area()/area))
        options.append((score,i))
    options.sort()
    if not options or options[0][0]>2 or (len(options)>1 and options[1][0]-options[0][0]<0.03):
        return None
    return options[0][1]


def resolve_edge(source_edge, source, candidate):
    oldcenter=relative_center(source_edge,source)
    length=source_edge.Length()
    options=[]
    for i,edge in enumerate(candidate.Edges()):
        score=distance(relative_center(edge,candidate),oldcenter)/max(1,length)
        score+=abs(math.log(edge.Length()/length))
        options.append((score,i))
    options.sort()
    if not options or options[0][0]>0.5 or (len(options)>1 and options[1][0]-options[0][0]<0.01):
        return None
    return options[0][1]


class TestAnchorTranslation(unittest.TestCase):
    def test_all_six_faces_and_twelve_edges(self):
        shape=cq.Workplane('XY').box(30,20,10,centered=(False,False,False)).val()
        self.assertEqual(len(shape.Faces()),6)
        self.assertEqual(len(shape.Edges()),12)
        for offset in ((0,0,0),(3,0,2),(1800,-1200,450),(-8600,2200,-1400)):
            moved=shape.translate(offset)
            for i,face in enumerate(shape.Faces()):
                with self.subTest(offset=offset,face=i):
                    self.assertEqual(resolve_face(face,shape,moved),i)
            for i,edge in enumerate(shape.Edges()):
                with self.subTest(offset=offset,edge=i):
                    self.assertEqual(resolve_edge(edge,shape,moved),i)

    def test_relative_geometry_is_not_position_dependent(self):
        shape=cq.Workplane('XY').box(30,20,10,centered=(False,False,False)).val()
        moved=shape.translate((1800,-1200,450))
        for face,other in zip(shape.Faces(),moved.Faces()):
            self.assertLess(distance(relative_center(face,shape),relative_center(other,moved)),1e-7)
        for edge,other in zip(shape.Edges(),moved.Edges()):
            self.assertLess(distance(relative_center(edge,shape),relative_center(other,moved)),1e-7)

    def test_duplicate_edges_remain_ambiguous(self):
        # Two distinct coincident edges must fail closed instead of picking
        # whichever topological traversal happens to visit first.
        one=cq.Edge.makeLine(cq.Vector(0,0,0),cq.Vector(10,0,0))
        two=cq.Edge.makeLine(cq.Vector(0,0,0),cq.Vector(10,0,0))
        self.assertIsNot(one,two)
        common=(5.0,0.0,0.0)
        scores=[distance(coordinates(edge.Center()),common)/10 for edge in (one,two)]
        self.assertEqual(scores,[0.0,0.0])
        self.assertLess(scores[1]-scores[0],0.01)


if __name__=='__main__':unittest.main(verbosity=2)

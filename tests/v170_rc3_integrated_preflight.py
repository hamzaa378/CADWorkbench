"""Portable source contracts and independent OpenCascade shape references.

These checks do not compile or execute the Windows Qt CAD application.
"""
from pathlib import Path
import unittest
import math

ROOT = Path(__file__).resolve().parents[1]


def source(path):
    return (ROOT / path).read_text(encoding='utf-8')


class ConsolidatedEngineeringSourceContract(unittest.TestCase):
    def test_verified_hole_audit_compiles_into_cadcore(self):
        cmake = source('CMakeLists.txt')
        self.assertIn('src/core/HoleTable.cpp', cmake)
        self.assertIn('src/core/HoleTable.h', cmake)
        header = source('src/core/HoleTable.h')
        self.assertIn('inspectAxialHoles(', header)
        self.assertIn('buildAxialHoleTableCsv(', header)
        self.assertIn('exportAxialHoleTableCsv(', header)

    def test_hole_audit_rejects_uncertain_geometry(self):
        code = source('src/core/HoleTable.cpp')
        for contract in ('face.Orientation()!=TopAbs_REVERSED',
                         'std::numbers::pi', 'rims.size()<2',
                         'solidCount!=1', 'QSaveFile file(path)',
                         'candidate.sourceGeometryToken=part.geometryToken',
                         'value.prepend(QChar(0x27))',
                         'catch(const Standard_Failure&'):
            with self.subTest(contract=contract):
                self.assertIn(contract, code)
        ui = source('src/ui/MainWindow.cpp')
        self.assertIn('&MainWindow::exportHoleTableCsv', ui)
        self.assertIn('cad::exportAxialHoleTableCsv(', ui)

    def test_live_rigid_mates_have_multiple_editable_connections(self):
        core = source('src/core/SceneDocument.cpp')
        ui = source('src/ui/MainWindow.cpp')
        for name in ('appendLiveRigidMateConnection(',
                     'removeLiveRigidMateConnection(',
                     'liveMateWorldObservations(',
                     'previouslyConstrained'):
            self.assertIn(name, core)
        self.assertIn('&MainWindow::manageLiveRigidMates', ui)
        self.assertIn('document_.appendLiveRigidMateConnection(', ui)

    def test_live_articulated_refresher_does_not_accumulate(self):
        code = source('src/core/SceneDocument.cpp')
        self.assertIn('liveArticulatedReferencePoses_', code)
        self.assertIn('mates::inverseRigid(liveArticulatedReferencePoses_.value(', code)
        self.assertIn('stageLiveArticulatedRegeneration(', code)
        self.assertIn('if(changed){checkpoint();parts_=std::move(staged);}', code)
        self.assertIn('root.insert("live_articulated_mates",network)', code)
        self.assertIn('Saved articulated reference count does not match graph components', code)

    def test_driven_joint_edits_commit_atomically(self):
        code = source('src/core/SceneDocument.cpp')
        start = code.index('bool SceneDocument::updateLiveArticulatedJointCoordinate(')
        end = code.index('bool SceneDocument::removeLiveArticulatedNetwork(', start)
        operation = code[start:end]
        self.assertLess(operation.index('stageLiveArticulatedRegeneration(candidate,'),
                        operation.index('checkpoint();'))
        self.assertLess(operation.index('checkpoint();'),
                        operation.index('liveArticulatedJoints_=std::move(candidate)'))
        ui = source('src/ui/MainWindow.cpp')
        self.assertIn('&MainWindow::editSavedArticulatedJoint', ui)
        self.assertIn('document_.updateLiveArticulatedJointCoordinate(', ui)

    def test_snapshot_restores_saved_constraint_definitions(self):
        code = source('src/core/SceneDocument.cpp')
        for phrase in ('previous.liveArticulatedReferencePoses',
                       'next.liveArticulatedReferencePoses',
                       'liveArticulatedJoints_=std::move(loadedArticulatedJoints)',
                       'if(!mates::solveRigidNetwork(',
                       'if(!articulated::solveNetwork('):
            self.assertIn(phrase, code)

    def test_existing_native_test_registration_preserved(self):
        cmake = source('CMakeLists.txt')
        self.assertIn('assembly_mate_document_v170_tests', cmake)
        self.assertIn('articulated_assembly_scene_v170_tests', cmake)
        self.assertIn('cadcore_tests', cmake)
        native = source('tests/articulated_assembly_scene_v170_tests.cpp')
        self.assertIn('updateLiveArticulatedJointCoordinate(', native)
        self.assertIn('editable.undo()', native)
        self.assertIn('reloadedDrive.updateLiveArticulatedJointCoordinate(', native)


class IndependentOCCTHoleGeometries(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
        from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut
        from OCP.gp import gp_Ax2, gp_Pnt, gp_Dir
        from OCP.TopExp import TopExp_Explorer
        from OCP.TopAbs import TopAbs_FACE, TopAbs_REVERSED
        from OCP.TopoDS import TopoDS
        from OCP.BRepAdaptor import BRepAdaptor_Surface
        from OCP.GeomAbs import GeomAbs_Cylinder
        cls.Box = BRepPrimAPI_MakeBox
        cls.Cylinder = BRepPrimAPI_MakeCylinder
        cls.Cut = BRepAlgoAPI_Cut
        cls.Ax2 = gp_Ax2
        cls.Pnt = gp_Pnt
        cls.Dir = gp_Dir
        cls.Explorer = TopExp_Explorer
        cls.Face = TopAbs_FACE
        cls.Reversed = TopAbs_REVERSED
        cls.AsFace = TopoDS.Face_s
        cls.Adaptor = BRepAdaptor_Surface
        cls.CylType = GeomAbs_Cylinder

    def cylindrical_walls(self, shape):
        result = []
        exp = self.Explorer(shape, self.Face)
        while exp.More():
            face = self.AsFace(exp.Current())
            surf = self.Adaptor(face)
            if surf.GetType() == self.CylType:
                result.append((face.Orientation(), surf.Cylinder()))
            exp.Next()
        return result

    def test_z_axis_through_hole_inner_wall(self):
        box = self.Box(25, 25, 12).Shape()
        drill = self.Cylinder(self.Ax2(self.Pnt(12, 12, 0), self.Dir(0, 0, 1)), 3, 12).Shape()
        final = self.Cut(box, drill).Shape()
        holes = [(o, cyl) for o, cyl in self.cylindrical_walls(final)
                 if o == self.Reversed and abs(cyl.Axis().Direction().Z()) > .9999]
        self.assertEqual(len(holes), 1)
        self.assertAlmostEqual(holes[0][1].Radius(), 3)

    def test_axial_blind_hole_inner_wall(self):
        box = self.Box(25, 25, 12).Shape()
        drill = self.Cylinder(self.Ax2(self.Pnt(12, 12, 4), self.Dir(0, 0, 1)), 3, 8).Shape()
        final = self.Cut(box, drill).Shape()
        holes = [(o, cyl) for o, cyl in self.cylindrical_walls(final)
                 if o == self.Reversed and abs(cyl.Axis().Direction().Z()) > .9999]
        self.assertEqual(len(holes), 1)

    def test_exterior_cylinder_not_detected_as_hole(self):
        boss = self.Cylinder(4, 12).Shape()
        self.assertEqual(len([(o, _) for o, _ in self.cylindrical_walls(boss)
                              if o == self.Reversed]), 0)

    def test_horizontal_bore_excluded_from_z_inspection(self):
        box = self.Box(25, 25, 12).Shape()
        drill = self.Cylinder(self.Ax2(self.Pnt(0, 12, 6), self.Dir(1, 0, 0)), 3, 25).Shape()
        final = self.Cut(box, drill).Shape()
        self.assertEqual(len([(o, cyl) for o, cyl in self.cylindrical_walls(final)
                              if o == self.Reversed and abs(cyl.Axis().Direction().Z()) > .9999]), 0)


if __name__ == '__main__':
    unittest.main()

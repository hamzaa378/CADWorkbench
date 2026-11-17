"""Independent OCCT and source-contract checks for v1.70 generated-face/drawing increment.

The OpenCascade examples verify geometric preconditions separately; native Qt/C++
integration tests must still run on Windows.
"""
import math
import unittest
from pathlib import Path
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.BRepAdaptor import BRepAdaptor_Surface
from OCP.BRepGProp import BRepGProp
from OCP.BRepClass import BRepClass_FaceClassifier
from OCP.GeomAbs import GeomAbs_Plane
from OCP.GProp import GProp_GProps
from OCP.TopAbs import TopAbs_FACE, TopAbs_IN
from OCP.TopExp import TopExp
from OCP.TopTools import TopTools_IndexedMapOfShape
from OCP.TopoDS import TopoDS
from OCP.gp import gp_Pnt

ROOT=Path(__file__).resolve().parents[1]

def faces(shape):
    mapping=TopTools_IndexedMapOfShape()
    TopExp.MapShapes_s(shape,TopAbs_FACE,mapping)
    return [TopoDS.Face_s(mapping.FindKey(i)) for i in range(1,mapping.Extent()+1)]

def center(shape):
    mass=GProp_GProps();BRepGProp.VolumeProperties_s(shape,mass)
    p=mass.CentreOfMass()
    return (p.X(),p.Y(),p.Z())

def face_info(body,face):
    surf=BRepAdaptor_Surface(face)
    if surf.GetType()!=GeomAbs_Plane:return None
    props=GProp_GProps();BRepGProp.SurfaceProperties_s(face,props)
    p=props.CentreOfMass();c=(p.X(),p.Y(),p.Z())
    n=surf.Plane().Axis().Direction();normal=(n.X(),n.Y(),n.Z())
    body_center=center(body)
    if sum(normal[i]*(c[i]-body_center[i]) for i in range(3))<0:
        normal=tuple(-v for v in normal)
    return c,normal,props.Mass()

def contains(face,coords):
    p=gp_Pnt(*coords)
    adaptor=BRepAdaptor_Surface(face)
    return adaptor.GetType()==GeomAbs_Plane and adaptor.Plane().Distance(p)<1e-5 and BRepClass_FaceClassifier(face,p,1e-5).State()==TopAbs_IN

def moved_cap_candidates(old,new):
    original=next(face_info(old,f) for f in faces(old) if contains(f,(5,5,10)))
    old_center,n,old_area=original
    result=[]
    for face in faces(new):
        info=face_info(new,face)
        if info is None:continue
        c,normal,area=info
        if sum(n[i]*normal[i] for i in range(3))<0.9999:continue
        if abs(math.log(area/old_area))>0.02:continue
        delta=[c[i]-old_center[i] for i in range(3)]
        shift=sum(delta[i]*n[i] for i in range(3))
        lateral=math.sqrt(sum((delta[i]-n[i]*shift)**2 for i in range(3)))
        if lateral>max(1e-4,0.005*math.sqrt(old_area)):continue
        point=tuple((5,5,10)[i]+n[i]*shift for i in range(3))
        if contains(face,point):result.append((face,shift))
    return result

class GeneratedFaceAndDrawingChecks(unittest.TestCase):
    def test_verified_moving_cap_in_open_cascade(self):
        original=BRepPrimAPI_MakeBox(10,10,10).Shape()
        raised=BRepPrimAPI_MakeBox(10,10,14).Shape()
        matches=moved_cap_candidates(original,raised)
        self.assertEqual(len(matches),1)
        self.assertAlmostEqual(matches[0][1],4.0,places=5)
        self.assertTrue(contains(matches[0][0],(5,5,14)))

    def test_lateral_and_area_changes_do_not_match(self):
        original=BRepPrimAPI_MakeBox(10,10,10).Shape()
        moved=BRepPrimAPI_MakeBox(gp_Pnt(4,0,0),10,10,14).Shape()
        widened=BRepPrimAPI_MakeBox(14,10,14).Shape()
        self.assertFalse(moved_cap_candidates(original,moved))
        self.assertFalse(moved_cap_candidates(original,widened))

    def test_stable_witnesses_are_validated_and_transformed(self):
        core=(ROOT/'src/core/ShapeFeatureTree.cpp').read_text(encoding="utf-8")
        rebasing=(ROOT/'src/core/BooleanReferenceLineage.cpp').read_text(encoding="utf-8")
        self.assertIn('kFaceWitness2Fields',core)
        self.assertIn('face_anchor_witness2_x',core)
        self.assertIn('hasWitness2&&!hasWitness',core)
        self.assertIn('hasFaceWitness2&&!hasFaceWitness',core)
        self.assertIn('pointFields(&moved,"face_anchor_witness2_",tr);',rebasing)
        self.assertIn('if(matches>1){',core)
        self.assertIn('secondMatches==1',core)

    def test_normal_only_motion_and_design_point_are_checked(self):
        lineage=(ROOT/'src/core/BooleanReferenceLineage.cpp').read_text(encoding="utf-8")
        self.assertIn('findDirectStageTranslatedPlanarFace',lineage)
        self.assertIn('normal.Dot(n)<0.9999',lineage)
        self.assertIn('std::abs(std::log(newArea/area))>0.02',lineage)
        self.assertIn('insideTrimmedPlanarFace(candidate,translatedWitness)',lineage)
        self.assertIn('design.Translate(oldNormal.Multiplied(normalShift));',lineage)
        self.assertIn('if(!insideTrimmedPlanarFace(selected,design))',lineage)

    def test_exported_drawing_detects_metadata_only_changes(self):
        shared=(ROOT/'src/core/DrawingSourceFingerprint.h').read_text(encoding="utf-8")
        document=(ROOT/'src/core/SceneDocument.cpp').read_text(encoding="utf-8")
        drawing=(ROOT/'src/core/DrawingSheet.cpp').read_text(encoding="utf-8")
        native=(ROOT/'tests/cadcore_tests.cpp').read_text(encoding="utf-8")
        self.assertIn('drawingSourceMetadataFingerprint(std::move(sources))',document)
        self.assertIn('drawingSourceMetadataFingerprint(metadataSources)',drawing)
        self.assertIn('drawingSourceMetadataFingerprint(selected)',drawing)
        self.assertIn('data-cadworkbench-source-data-token=',drawing)
        self.assertIn('DRAWING_SOURCE_METADATA_STALE',drawing)
        for field in ('revision','materialName','partNumber','youngsModulusGPa',
                      'materialDensityKgPerM3','supplierPartNumber'):
            self.assertIn(field,shared)
        self.assertIn('new-manufacturing-revision',native)

    def test_drawing_revision_is_published_only_after_export(self):
        ui=(ROOT/'src/ui/MainWindow.cpp').read_text(encoding="utf-8")
        segment=ui.split('void MainWindow::refreshSavedDrawingSource() {',1)[1].split('void MainWindow::exportDrawingSvg()',1)[0]
        self.assertIn('Regenerate saved drawing from current model',segment)
        self.assertIn('exportOrthographicDrawingSvg(composite,path,&error',segment)
        self.assertIn('prepareDrawingSheetSettings(masterId,updated,&prepared,&error)',segment)
        self.assertIn('updated.sourceDataToken=document_.drawingSourceDataToken',segment)
        self.assertNotIn('refreshDrawingSheetSource(',segment)
        self.assertLess(segment.index('exportOrthographicDrawingSvg(composite,path,&error'),
                        segment.index('setDrawingSheetSettings(masterId,prepared,&error)'))
        self.assertIn('if(!exported)',ui)  # Legacy drawing export still checks failures.

if __name__=='__main__':unittest.main()

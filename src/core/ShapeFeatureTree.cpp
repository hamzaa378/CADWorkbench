#include "ShapeFeatureTree.h"
#include "FeatureDependencyPlan.h"
#include "CircularEdgeLineage.h"
#include "PolygonFeatureProfile.h"
#include "AdvancedSketchProfiles.h"
#include "DirectionalHoleSpan.h"
#include "FaceMachiningDatum.h"
#include "ManufacturingHolePatterns.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Circle.hxx>
#include <gp_Circ.hxx>
#include <NCollection_HArray1.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <TopAbs_State.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRepTools.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffset_Mode.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <NCollection_List.hxx>
#include <gp_Pln.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Sphere.hxx>
#include <gp_Vec.hxx>
#include <QSet>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <set>
#include <exception>
#include <utility>

namespace cad {
namespace {
void fail(QString* error,const QString& text){if(error)*error=text;}
const QMap<QString,QStringList>& specifications(){
    static const QMap<QString,QStringList> table={
        {"Translate",{"x","y","z"}},
        {"Rotate X",{"degrees"}}, {"Rotate Y",{"degrees"}}, {"Rotate Z",{"degrees"}},
        {"Rotate arbitrary axis",{"degrees","axis_x","axis_y","axis_z","pivot_x","pivot_y","pivot_z"}},
        {"Uniform scale",{"factor"}},
        {"Fillet all edges",{"radius"}}, {"Chamfer all edges",{"distance"}},
        {"Fillet selected edge",{"edge_index","radius"}},
        {"Chamfer selected edge",{"edge_index","distance"}},
        // One editable operation for a selected edge GROUP, each with its own
        // persisted geometric witness. This avoids fragile positional lists.
        {"Fillet selected edges",{"edge_count","radius"}},
        {"Chamfer selected edges",{"edge_count","distance"}},
        {"Cylinder cut Z",{"x","y","z","radius","depth"}},
        {"Cylinder cut arbitrary axis",{"x","y","z","axis_x","axis_y","axis_z","radius","depth"}},
        {"Cylinder through all arbitrary axis",{"x","y","z","axis_x","axis_y","axis_z","radius"}},
        {"Face through-all hole",{"face_index","u","v","radius"}},
        {"Face counterbore through all",{"face_index","u","v","radius","head_radius","recess_depth"}},
        {"Face countersink through all",{"face_index","u","v","radius","head_radius","included_angle"}},
        {"Face slot through all",{"face_index","u","v","radius","center_spacing","rotation_degrees"}},
        {"Face slot pocket",{"face_index","u","v","radius","center_spacing","rotation_degrees","depth"}},
        {"Face hole grid through all",{"face_index","u","v","radius","columns","rows","pitch_u","pitch_v","rotation_degrees"}},
        {"Face bolt circle through all",{"face_index","u","v","radius","count","pcd","start_angle_degrees"}},
        {"Face normal pocket",{"face_index","u","v","radius","depth"}},
        {"Face normal boss",{"face_index","u","v","radius","depth"}},
        {"Face rectangle pocket",{"face_index","u","v","width","height","depth"}},
        {"Face rectangle boss",{"face_index","u","v","width","height","depth"}},
        {"Cylinder cut X",{"x","y","z","radius","depth"}},
        {"Cylinder cut Y",{"x","y","z","radius","depth"}},
        {"Mirror XY",{}}, {"Mirror XZ",{}}, {"Mirror YZ",{}},
        {"Mirror arbitrary plane",{"normal_x","normal_y","normal_z","origin_x","origin_y","origin_z"}},
        {"Linear pattern X",{"count","spacing"}},
        {"Linear pattern Y",{"count","spacing"}},
        {"Linear pattern Z",{"count","spacing"}},
        {"Circular pattern Z",{"count","sweep_degrees"}},
        {"Linear pattern vector",{"count","dx","dy","dz"}},
        {"Circular pattern arbitrary axis",{"count","sweep_degrees","axis_x","axis_y","axis_z","pivot_x","pivot_y","pivot_z"}},
        {"Open-face shell",{"face_index","thickness"}},
        {"Draft face Z",{"face_index","angle_degrees","neutral_z"}},
        {"Sketch polygon face boss",{}}, {"Sketch polygon face pocket",{}},
        {"Sketch polygon extrude Z",{}},
        {"Sketch polygon pocket Z",{}},
        {"Sketch polygon revolve Z",{}},
        {"Sketch polygons loft Z",{}},
        {"Sketch polygon sweep 3D",{}},
        {"Sketch polygon sweep spline 3D",{}},
        {"Sketch multi-section loft Z",{}},
        {"Sketch polygon revolve axis",{}},
        {"Revolve annulus Z",{"inner_radius","outer_radius","height"}},
        {"Sweep circle path",{"radius","path_length","lateral_offset","vertical_rise"}},
        {"Loft rectangles",{"width0","depth0","width1","depth1","height","offset_x","offset_y"}},
        {"Fuse branches",{}}, {"Subtract branches",{}}, {"Intersect branches",{}}
    };
    return table;
}
const QStringList kFaceAnchorFields={"face_anchor_x","face_anchor_y","face_anchor_z",
    "face_anchor_nx","face_anchor_ny","face_anchor_nz","face_anchor_area"};
const QStringList kFaceBodyFields={"face_anchor_body_x","face_anchor_body_y","face_anchor_body_z"};
// New anchors carry an interior witness on the support plane. Unlike area or
// centroid, its identity survives supported face splits and co-planar merges.
const QStringList kFaceWitnessFields={"face_anchor_witness_x","face_anchor_witness_y","face_anchor_witness_z"};
// Optional v1.70+ discriminator for generated or merged planar faces. Both
// witnesses must be trimmed-interior points of the originally selected face.
// Older saved scenes (which only contain one witness) are still valid.
const QStringList kFaceWitness2Fields={"face_anchor_witness2_x","face_anchor_witness2_y","face_anchor_witness2_z"};
bool witnessOnFace(const TopoDS_Face& face,const gp_Pnt& point,double tolerance){
    const BRepAdaptor_Surface adaptor(face);
    if(adaptor.GetType()!=GeomAbs_Plane)return false;
    // OCCT's 3D classifier resolves the face's own underlying UV mapping,
    // avoiding mismatches caused by reversed/located planar parameter frames.
    if(adaptor.Plane().Distance(point)>tolerance)return false;
    BRepClass_FaceClassifier classifier(face,point,tolerance);
    return classifier.State()==TopAbs_IN;
}

// Persisted curved support has 14 required scalars and one extra scalar for
// cones and tori. It is restricted to shell openings: face machining remains
// planar, as its local UV frame contract is not defined on curved surfaces.
const QStringList kCurvedAnchorFields={"curved_type","curved_body_x","curved_body_y","curved_body_z",
    "curved_center_x","curved_center_y","curved_center_z",
    "curved_axis_x","curved_axis_y","curved_axis_z","curved_radius",
    "curved_witness_x","curved_witness_y","curved_witness_z"};
// Optional second interior point for new spline-shell anchors. The old 14-field
// descriptor is still readable, but newly captured freeform surfaces require
// two interior regions to agree before an attachment can be regenerated.
const QStringList kCurvedWitness2Fields={"curved_witness2_x","curved_witness2_y","curved_witness2_z"};
const QStringList kEdgeAnchorFields={"edge_x","edge_y","edge_z","edge_length"};
const QStringList kEdgeBodyFields={"edge_body_x","edge_body_y","edge_body_z"};
// v2 witnessed trimmed-edge references distinguish geometric support from
// traversal order and length. A witness must remain strictly in the interior
// of a unique successor segment, never on a split boundary.
const QStringList kEdgeWitnessFields={"edge_anchor_version","edge_curve_kind",
    "edge_witness_x","edge_witness_y","edge_witness_z",
    "edge_tangent_x","edge_tangent_y","edge_tangent_z","edge_curvature"};
// The reference center is translation-covariant and works for solid bodies,
// open surfaces and standalone edge compounds. Older saved anchors omit it.
gp_Pnt shapeReferenceCenter(const TopoDS_Shape& shape){
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape,props);
    if(std::abs(props.Mass())>1e-12)return props.CentreOfMass();
    BRepGProp::SurfaceProperties(shape,props);
    if(std::abs(props.Mass())>1e-12)return props.CentreOfMass();
    BRepGProp::LinearProperties(shape,props);
    if(std::abs(props.Mass())>1e-12)return props.CentreOfMass();
    return gp_Pnt(0,0,0);
}
bool completeOptionalFields(const QMap<QString,double>& values,const QStringList& keys){
    int count=0;
    for(const QString& key:keys)if(values.contains(key))++count;
    return count==0||count==keys.size();
}

bool usesStableEdge(const QString& type){
    return type=="Fillet selected edge"||type=="Chamfer selected edge";
}
bool usesPlanarFace(const QString& type){
    return type=="Sketch polygon face boss"||type=="Sketch polygon face pocket"||
           type=="Face through-all hole"||type=="Face counterbore through all"||
           type=="Face countersink through all"||type.startsWith("Face slot ")||
           type=="Face hole grid through all"||type=="Face bolt circle through all"||
           type.startsWith("Face normal ")||type.startsWith("Face rectangle ")||
           type=="Open-face shell"||type=="Draft face Z";
}
// The orientation is canonicalized using the solid center; this survives
// index reorderings and most local face edits, but not ambiguous symmetric faces.
bool faceDescriptor(const TopoDS_Shape& body,const TopoDS_Face& face,
                    QMap<QString,double>* anchor){
    if(face.IsNull())return false;
    BRepAdaptor_Surface adaptor(face);
    if(adaptor.GetType()!=GeomAbs_Plane)return false;
    GProp_GProps faceProps;
    BRepGProp::SurfaceProperties(face,faceProps);
    if(faceProps.Mass()<1e-10)return false;
    const gp_Pnt center=faceProps.CentreOfMass();
    const gp_Pnt reference=shapeReferenceCenter(body);
    gp_Vec direction(adaptor.Plane().Axis().Direction());
    if(direction.Dot(gp_Vec(reference,center))<0)direction.Reverse();
    *anchor={{"face_anchor_x",center.X()},{"face_anchor_y",center.Y()},
        {"face_anchor_z",center.Z()},{"face_anchor_nx",direction.X()},
        {"face_anchor_ny",direction.Y()},{"face_anchor_nz",direction.Z()},
        {"face_anchor_area",faceProps.Mass()},
        {"face_anchor_body_x",reference.X()},{"face_anchor_body_y",reference.Y()},
        {"face_anchor_body_z",reference.Z()}};
    // A concave face (or a face with an internal hole) may have its centroid
    // outside the trimming wires. Find a real trimmed-interior witness rather
    // than storing an invalid persistent reference or falling back to an index.
    gp_Pnt witness=center;
    bool hasWitness=witnessOnFace(face,witness,1e-7);
    if(!hasWitness){
        double uMin=0,uMax=0,vMin=0,vMax=0;
        BRepTools::UVBounds(face,uMin,uMax,vMin,vMax);
        if(std::isfinite(uMin)&&std::isfinite(uMax)&&std::isfinite(vMin)&&std::isfinite(vMax)&&
           uMax>uMin&&vMax>vMin){
            // Interior samples are deterministically ordered by distance
            // from the central UV point. A boundary sample is never accepted.
            for(int ring=0;ring<=4&&!hasWitness;++ring){
                for(int i=-ring;i<=ring&&!hasWitness;++i)
                    for(int j=-ring;j<=ring&&!hasWitness;++j){
                        if(std::max(std::abs(i),std::abs(j))!=ring)continue;
                        const double u=uMin+(0.5+double(i)/10.0)*(uMax-uMin);
                        const double v=vMin+(0.5+double(j)/10.0)*(vMax-vMin);
                        const gp_Pnt candidate=adaptor.Value(u,v);
                        if(witnessOnFace(face,candidate,1e-7)){
                            witness=candidate;hasWitness=true;
                        }
                    }
            }
        }
    }
    if(hasWitness){
        anchor->insert("face_anchor_witness_x",witness.X());
        anchor->insert("face_anchor_witness_y",witness.Y());
        anchor->insert("face_anchor_witness_z",witness.Z());
        // Capture a SECOND *nearby* interior point on the same trimmed face.
        // This is a disambiguator only: a split may legitimately discard this
        // point while preserving the primary design witness. Never use it to
        // override a unique primary match or to relocate the design point.
        const gp_Vec n(direction);
        gp_Vec tangent=std::abs(n.X())<0.9?gp_Vec(1,0,0):gp_Vec(0,1,0);
        tangent-=n.Multiplied(tangent.Dot(n));tangent.Normalize();
        const gp_Vec transverse=n.Crossed(tangent);
        const double step=std::max(1e-4,0.025*std::sqrt(faceProps.Mass()));
        bool hasSecond=false;gp_Pnt second;
        for(const gp_Vec& axis:{tangent,transverse}){
            for(const double sign:{1.0,-1.0}){
                gp_Pnt trial=witness;
                trial.Translate(axis.Multiplied(step*sign));
                if(witnessOnFace(face,trial,1e-7)){
                    second=trial;hasSecond=true;break;
                }
            }
            if(hasSecond)break;
        }
        if(hasSecond){
            anchor->insert("face_anchor_witness2_x",second.X());
            anchor->insert("face_anchor_witness2_y",second.Y());
            anchor->insert("face_anchor_witness2_z",second.Z());
        }
    }
    return true;
}
// A face centroid is not a persistent machining datum: a Boolean may merge
// coplanar faces and change their centroid without moving the selected design
// region. Preserve the original face-local origin when the saved interior
// witness survives unchanged. Only translate the origin when that witness is
// no longer present but its body-displaced copy is uniquely resolved. Legacy
// index-only operations keep the original centroid-based placement contract.
gp_Pnt witnessedMachiningOrigin(const TopoDS_Shape& current,
                               const TopoDS_Face& selected,
                               const ShapeFeatureNode& feature,
                               const gp_Pnt& legacyCenter){
    const auto& p=feature.parameters;
    if(!p.contains("face_anchor_witness_x")||
       !p.contains("face_anchor_body_x"))return legacyCenter;
    const gp_Pnt oldWitness(p.value("face_anchor_witness_x"),
        p.value("face_anchor_witness_y"),p.value("face_anchor_witness_z"));
    const gp_Pnt previousBody(p.value("face_anchor_body_x"),
        p.value("face_anchor_body_y"),p.value("face_anchor_body_z"));
    const gp_Vec shift(previousBody,shapeReferenceCenter(current));
    gp_Pnt movedWitness=oldWitness;
    movedWitness.Translate(shift);
    // A translation *within* the source face can leave both the old and
    // displaced witnesses inside the new face. Compare invariant area and
    // centroid displacement to distinguish that rigid edit from a coplanar
    // split/merge (which changes the surviving trimmed face's area).
    GProp_GProps faceMass;
    BRepGProp::SurfaceProperties(selected,faceMass);
    gp_Pnt expectedCenter(p.value("face_anchor_x"),p.value("face_anchor_y"),
                          p.value("face_anchor_z"));
    expectedCenter.Translate(shift);
    const double savedArea=p.value("face_anchor_area");
    const bool rigidTranslation=shift.Magnitude()>1e-8&&savedArea>1e-8&&
        std::abs(faceMass.Mass()-savedArea)<=std::max(1e-5,savedArea*1e-6)&&
        faceMass.CentreOfMass().Distance(expectedCenter)<=1e-5;
    machining_datum::Vec3 originPosition;
    if(!machining_datum::chooseOrigin(
           {p.value("face_anchor_x"),p.value("face_anchor_y"),p.value("face_anchor_z")},
           {shift.X(),shift.Y(),shift.Z()},
           witnessOnFace(selected,oldWitness,1e-5),
           witnessOnFace(selected,movedWitness,1e-5),
           rigidTranslation,&originPosition))
        return legacyCenter;  // Should be unreachable after findFace succeeded.
    gp_Pnt origin(originPosition.x,originPosition.y,originPosition.z);
    // Compensate for harmless normal-only support-plane displacement.
    // Tangential placement remains tied to the witnessed physical datum.
    const gp_Pln support=BRepAdaptor_Surface(selected).Plane();
    const gp_Vec normal(support.Axis().Direction());
    const gp_Vec toPlane(origin,support.Location());
    origin.Translate(normal.Multiplied(normal.Dot(toPlane)));
    return origin;
}
bool findFace(const TopoDS_Shape& body,const ShapeFeatureNode& node,TopoDS_Face* selected,QString* error){
    if(node.parameters.contains("curved_type")){
        QMap<QString,double> anchor;
        for(const QString& key:kCurvedAnchorFields)anchor.insert(key,node.parameters.value(key));
        if(node.parameters.contains("curved_aux"))anchor.insert("curved_aux",node.parameters.value("curved_aux"));
        for(const QString& key:kCurvedWitness2Fields)
            if(node.parameters.contains(key))anchor.insert(key,node.parameters.value(key));
        return resolveCurvedFaceAnchor(body,anchor,selected,error);
    }
    const int index=static_cast<int>(node.parameters.value("face_index"));
    if(!node.parameters.contains("face_anchor_x")){
        int i=0;
        for(TopExp_Explorer e(body,TopAbs_FACE);e.More();e.Next())
            if(++i==index){*selected=TopoDS::Face(e.Current());return true;}
        fail(error,"Referenced face index is missing after upstream rebuild.");return false;
    }
    const gp_Vec target(node.parameters.value("face_anchor_nx"),
        node.parameters.value("face_anchor_ny"),node.parameters.value("face_anchor_nz"));
    gp_Pnt previous(node.parameters.value("face_anchor_x"),
        node.parameters.value("face_anchor_y"),node.parameters.value("face_anchor_z"));
    // The newer descriptors include the source body's reference center.
    // Compensate for arbitrarily large pure translations of the feature input
    // without making legacy seven-field project files unreadable.
    if(node.parameters.contains("face_anchor_body_x")){
        const gp_Pnt oldBody(node.parameters.value("face_anchor_body_x"),
            node.parameters.value("face_anchor_body_y"),node.parameters.value("face_anchor_body_z"));
        previous.Translate(gp_Vec(oldBody,shapeReferenceCenter(body)));
    }
    const double oldArea=node.parameters.value("face_anchor_area");
    // For v1.70 witness anchors, trimmed-point containment is the first
    // identity discriminator. It tolerates face centroid/area changes caused
    // by a split or coplanar merge. More than one matching successor is
    // ambiguous (e.g. a witness on a split boundary): never guess.
    if(node.parameters.contains("face_anchor_witness_x")){
        gp_Pnt witness(node.parameters.value("face_anchor_witness_x"),
                       node.parameters.value("face_anchor_witness_y"),
                       node.parameters.value("face_anchor_witness_z"));
        gp_Pnt compensated=witness;
        if(node.parameters.contains("face_anchor_body_x")){
            const gp_Pnt oldBody(node.parameters.value("face_anchor_body_x"),
                node.parameters.value("face_anchor_body_y"),
                node.parameters.value("face_anchor_body_z"));
            compensated.Translate(gp_Vec(oldBody,shapeReferenceCenter(body)));
        }
        for(const gp_Pnt& trial:{witness,compensated}){
            TopoDS_Face match;int matches=0;
            for(TopExp_Explorer e(body,TopAbs_FACE);e.More();e.Next()){
                const TopoDS_Face face=TopoDS::Face(e.Current());
                QMap<QString,double> descriptor;
                if(!faceDescriptor(body,face,&descriptor))continue;
                const gp_Vec normal(descriptor.value("face_anchor_nx"),
                    descriptor.value("face_anchor_ny"),descriptor.value("face_anchor_nz"));
                if(normal.Dot(target)<0.97||!witnessOnFace(face,trial,1e-5))continue;
                match=face;++matches;
            }
            if(matches==1){
                // A secondary, body-relative witness can coincide with a
                // DIFFERENT copied/duplicated planar face even though the
                // unchanged-world witness also survives. In that case there
                // is no evidence which copy owns this machining feature.
                // Do not silently attach it to the first traversal result.
                if(trial.Distance(witness)<1e-9&&
                   compensated.Distance(witness)>1e-7){
                    int displacedMatches=0;
                    bool disagrees=false;
                    for(TopExp_Explorer rival(body,TopAbs_FACE);rival.More();rival.Next()){
                        const TopoDS_Face other=TopoDS::Face(rival.Current());
                        QMap<QString,double> descriptor;
                        if(!faceDescriptor(body,other,&descriptor))continue;
                        const gp_Vec normal(descriptor.value("face_anchor_nx"),
                            descriptor.value("face_anchor_ny"),descriptor.value("face_anchor_nz"));
                        if(normal.Dot(target)<0.97||
                           !witnessOnFace(other,compensated,1e-5))continue;
                        ++displacedMatches;
                        if(!other.IsSame(match))disagrees=true;
                    }
                    if(disagrees||displacedMatches>1){
                        fail(error,"Face lineage conflicts between the original and body-shifted witnesses; duplicated planar support must be explicitly rebound.");
                        return false;
                    }
                }
                *selected=match;return true;
            }
            if(matches>1){
                // Two generated/copied faces can both contain the original
                // witness. A secondary local witness may distinguish them,
                // but must never resolve an ambiguous primary by proximity.
                if(completeOptionalFields(node.parameters,kFaceWitness2Fields)&&
                   node.parameters.contains("face_anchor_witness2_x")){
                    gp_Pnt second(node.parameters.value("face_anchor_witness2_x"),
                        node.parameters.value("face_anchor_witness2_y"),
                        node.parameters.value("face_anchor_witness2_z"));
                    if(trial.Distance(compensated)<1e-9&&trial.Distance(witness)>1e-9)
                        second.Translate(gp_Vec(witness,compensated));
                    TopoDS_Face candidate;int secondMatches=0;
                    for(TopExp_Explorer e(body,TopAbs_FACE);e.More();e.Next()){
                        const TopoDS_Face possible=TopoDS::Face(e.Current());
                        QMap<QString,double> descriptor;
                        if(!faceDescriptor(body,possible,&descriptor))continue;
                        const gp_Vec normal(descriptor.value("face_anchor_nx"),
                            descriptor.value("face_anchor_ny"),descriptor.value("face_anchor_nz"));
                        if(normal.Dot(target)>=0.97&&witnessOnFace(possible,trial,1e-5)&&
                           witnessOnFace(possible,second,1e-5)){
                            candidate=possible;++secondMatches;
                        }
                    }
                    if(secondMatches==1){*selected=candidate;return true;}
                }
                fail(error,"Face lineage is ambiguous after a split/merge; reselect the witness face.");return false;
            }
            // Try the body-relative witness if the unchanged-world witness
            // no longer lies on the model after a pure rigid translation.
        }
        fail(error,"Face witness no longer lies on a unique planar successor; reselect the face.");return false;
    }
    double best=1e100,runnerUp=1e100;
    for(TopExp_Explorer e(body,TopAbs_FACE);e.More();e.Next()){
        QMap<QString,double> value;
        const TopoDS_Face face=TopoDS::Face(e.Current());
        if(!faceDescriptor(body,face,&value))continue;
        const gp_Vec current(value.value("face_anchor_nx"),value.value("face_anchor_ny"),value.value("face_anchor_nz"));
        if(current.Dot(target)<0.97)continue;
        const gp_Pnt center(value.value("face_anchor_x"),value.value("face_anchor_y"),value.value("face_anchor_z"));
        const double length=std::max(1.,std::sqrt(oldArea));
        const double score=center.Distance(previous)/length+
             0.5*std::abs(std::log(value.value("face_anchor_area")/oldArea));
        if(score<best){runnerUp=best;best=score;*selected=face;}
        else if(score<runnerUp)runnerUp=score;
    }
    if(selected->IsNull()||best>2.0||runnerUp-best<0.03){
        fail(error,"Planar face anchor could not be uniquely resolved after upstream geometry changed; reselect face.");
        return false;
    }
    return true;
}
} // namespace (internal CAD feature helpers)

// The following two functions are part of the public cad::ShapeFeatureTree
// API. They must have external linkage for consumers in other translation
// units, including BooleanReferenceLineage and MainWindow.
// Analytic and spline/Bezier curved-face witness lineage is independent from
// polygon features' planar frame contract. The support surface is checked as
// well as the trimmed face containing the witness; area/order are not identity.
bool captureCurvedFaceAnchor(const TopoDS_Shape& shape,int faceIndex,
                             QMap<QString,double>* anchor,QString* error){
    if(!anchor||faceIndex<1||shape.IsNull()){
        fail(error,"Select a non-null body and a positive curved-face index.");return false;
    }
    TopoDS_Face selected;int i=0;
    for(TopExp_Explorer e(shape,TopAbs_FACE);e.More();e.Next())
        if(++i==faceIndex){selected=TopoDS::Face(e.Current());break;}
    if(selected.IsNull()){
        fail(error,"Curved face index no longer exists.");return false;
    }
    BRepAdaptor_Surface surface(selected);
    const auto type=surface.GetType();
    if(type!=GeomAbs_Cylinder&&type!=GeomAbs_Sphere&&
       type!=GeomAbs_Cone&&type!=GeomAbs_Torus&&
       type!=GeomAbs_BSplineSurface&&type!=GeomAbs_BezierSurface){
        fail(error,"Curved-face lineage supports analytic, B-spline and Bezier surfaces.");return false;
    }
    const gp_Pnt bodyCenter=shapeReferenceCenter(shape);
    QMap<QString,double> candidate{
        {"curved_type",type==GeomAbs_Cylinder?1.0:type==GeomAbs_Sphere?2.0:
                       type==GeomAbs_Cone?3.0:type==GeomAbs_Torus?4.0:
                       type==GeomAbs_BSplineSurface?5.0:6.0},
        {"curved_body_x",bodyCenter.X()},{"curved_body_y",bodyCenter.Y()},
        {"curved_body_z",bodyCenter.Z()}};
    if(type==GeomAbs_Cylinder){
        const auto cylinder=surface.Cylinder();
        const gp_Pnt origin=cylinder.Axis().Location();
        const gp_Dir direction=cylinder.Axis().Direction();
        candidate.insert("curved_center_x",origin.X());
        candidate.insert("curved_center_y",origin.Y());
        candidate.insert("curved_center_z",origin.Z());
        candidate.insert("curved_axis_x",direction.X());
        candidate.insert("curved_axis_y",direction.Y());
        candidate.insert("curved_axis_z",direction.Z());
        candidate.insert("curved_radius",cylinder.Radius());
    }else if(type==GeomAbs_Sphere){
        const auto sphere=surface.Sphere();
        const gp_Pnt origin=sphere.Location();
        candidate.insert("curved_center_x",origin.X());
        candidate.insert("curved_center_y",origin.Y());
        candidate.insert("curved_center_z",origin.Z());
        candidate.insert("curved_axis_x",0.0);
        candidate.insert("curved_axis_y",0.0);
        candidate.insert("curved_axis_z",0.0);
        candidate.insert("curved_radius",sphere.Radius());
    }
    if(type==GeomAbs_Cone){
        const auto cone=surface.Cone();
        const gp_Pnt origin=cone.Axis().Location();
        const gp_Dir direction=cone.Axis().Direction();
        candidate.insert("curved_center_x",origin.X());candidate.insert("curved_center_y",origin.Y());
        candidate.insert("curved_center_z",origin.Z());
        candidate.insert("curved_axis_x",direction.X());candidate.insert("curved_axis_y",direction.Y());
        candidate.insert("curved_axis_z",direction.Z());
        candidate.insert("curved_radius",cone.RefRadius());
        candidate.insert("curved_aux",cone.SemiAngle());
    }
    if(type==GeomAbs_Torus){
        const auto torus=surface.Torus();
        const gp_Pnt origin=torus.Location();
        const gp_Dir direction=torus.Axis().Direction();
        candidate.insert("curved_center_x",origin.X());candidate.insert("curved_center_y",origin.Y());
        candidate.insert("curved_center_z",origin.Z());
        candidate.insert("curved_axis_x",direction.X());candidate.insert("curved_axis_y",direction.Y());
        candidate.insert("curved_axis_z",direction.Z());
        candidate.insert("curved_radius",torus.MajorRadius());
        candidate.insert("curved_aux",torus.MinorRadius());
    }
    double u0=0,u1=0,v0=0,v1=0;
    BRepTools::UVBounds(selected,u0,u1,v0,v1);
    if(!std::isfinite(u0)||!std::isfinite(u1)||!std::isfinite(v0)||
       !std::isfinite(v1)||u1<=u0||v1<=v0){
        fail(error,"Curved face has no finite UV domain for a stable witness.");return false;
    }
    // Search the actual face interior, not its surface centroid (which is
    // often inside a cylinder/sphere rather than ON its curved support).
    bool found=false;gp_Pnt witness;
    for(int ring=0;ring<8&&!found;++ring)
        for(int i=-ring;i<=ring&&!found;++i)
            for(int j=-ring;j<=ring&&!found;++j){
                if(std::max(std::abs(i),std::abs(j))!=ring)continue;
                const double u=u0+(0.5+double(i)/18.0)*(u1-u0);
                const double v=v0+(0.5+double(j)/18.0)*(v1-v0);
                const gp_Pnt point=surface.Value(u,v);
                BRepClass_FaceClassifier classification(selected,point,1e-6);
                if(classification.State()==TopAbs_IN){witness=point;found=true;}
            }
    if(!found){fail(error,"Unable to find a trimmed-interior witness on this curved face.");return false;}
    if(type==GeomAbs_BSplineSurface||type==GeomAbs_BezierSurface){
        // Spline supports do not have a fixed cylinder-like origin or radius.
        // Store a bounded local geometric signature: interior point, local
        // normal and a size scale for limiting later permitted deformation.
        GeomAPI_ProjectPointOnSurf projection(witness,BRep_Tool::Surface(selected));
        if(projection.NbPoints()<1){fail(error,"Cannot parameterize spline face witness.");return false;}
        Standard_Real u=0,v=0;
        projection.LowerDistanceParameters(u,v);
        gp_Pnt point;gp_Vec du,dv;
        surface.D1(u,v,point,du,dv);
        gp_Vec normal=du.Crossed(dv);
        if(normal.SquareMagnitude()<1e-16){fail(error,"Spline witness is at a surface singularity.");return false;}
        normal.Normalize();
        GProp_GProps area;BRepGProp::SurfaceProperties(selected,area);
        if(!std::isfinite(area.Mass())||area.Mass()<1e-10){
            fail(error,"Spline face has no reliable surface area.");return false;
        }
        const double scale=std::sqrt(area.Mass());
        candidate.insert("curved_center_x",witness.X());
        candidate.insert("curved_center_y",witness.Y());
        candidate.insert("curved_center_z",witness.Z());
        candidate.insert("curved_axis_x",normal.X());
        candidate.insert("curved_axis_y",normal.Y());
        candidate.insert("curved_axis_z",normal.Z());
        candidate.insert("curved_radius",scale);
    }
    if(type==GeomAbs_BSplineSurface||type==GeomAbs_BezierSurface){
        // A second independent trimmed-interior point helps distinguish two
        // nearby patches with essentially identical normals and curvature.
        // Do not issue a weak single-point anchor when no second point exists.
        bool secondFound=false;gp_Pnt second;
        for(const double du:{0.075,-0.075,0.15,-0.15}){
            for(const double dv:{0.0,0.075,-0.075}){
                const double u=u0+(0.5+du)*(u1-u0);
                const double v=v0+(0.5+dv)*(v1-v0);
                const gp_Pnt trial=surface.Value(u,v);
                const BRepClass_FaceClassifier inside(selected,trial,1e-6);
                if(inside.State()==TopAbs_IN&&trial.Distance(witness)>1e-3){
                    second=trial;secondFound=true;break;
                }
            }
            if(secondFound)break;
        }
        if(!secondFound){fail(error,"Freeform face lacks a second stable interior region.");return false;}
        candidate.insert("curved_witness2_x",second.X());
        candidate.insert("curved_witness2_y",second.Y());
        candidate.insert("curved_witness2_z",second.Z());
    }
    candidate.insert("curved_witness_x",witness.X());
    candidate.insert("curved_witness_y",witness.Y());
    candidate.insert("curved_witness_z",witness.Z());
    *anchor=std::move(candidate);return true;
}
bool resolveCurvedFaceAnchor(const TopoDS_Shape& shape,
                             const QMap<QString,double>& anchor,
                             TopoDS_Face* face,QString* error){
    if(!face||shape.IsNull()||(anchor.size()!=14&&anchor.size()!=15&&anchor.size()!=17)){
        fail(error,"Curved face descriptor is missing or has unexpected fields.");return false;
    }
    const QStringList keys={"curved_type","curved_body_x","curved_body_y","curved_body_z",
        "curved_center_x","curved_center_y","curved_center_z",
        "curved_axis_x","curved_axis_y","curved_axis_z","curved_radius",
        "curved_witness_x","curved_witness_y","curved_witness_z"};
    for(const QString& key:keys)
        if(!anchor.contains(key)||!std::isfinite(anchor.value(key))){
            fail(error,"Curved face descriptor is incomplete or nonfinite.");return false;
        }
    const double type=anchor.value("curved_type"),radius=anchor.value("curved_radius");
    const bool extra=(type==3.0||type==4.0);
    const bool second=anchor.contains("curved_witness2_x");
    if(second&&(type!=5.0&&type!=6.0)){
        fail(error,"Second curved witness is valid for freeform faces only.");return false;
    }
    if(second)for(const QString& key:kCurvedWitness2Fields)
        if(!anchor.contains(key)||!std::isfinite(anchor.value(key))){
            fail(error,"Second freeform witness is incomplete.");return false;
        }
    const double aux=anchor.value("curved_aux",0.0);
    if((type!=1.0&&type!=2.0&&type!=3.0&&type!=4.0&&type!=5.0&&type!=6.0)||
       anchor.size()!=(extra?15:(second?17:14))||!std::isfinite(aux)||
       (extra&&!anchor.contains("curved_aux"))||radius<=1e-8||radius>1e6||
       (type==3.0&&(std::abs(aux)<1e-8||std::abs(aux)>1.5))||
       (type==4.0&&(aux<=1e-8||aux>=radius))){
        fail(error,"Unsupported curved surface type or invalid support parameters.");return false;
    }
    const gp_Pnt oldBody(anchor.value("curved_body_x"),anchor.value("curved_body_y"),anchor.value("curved_body_z"));
    const gp_Vec shift(oldBody,shapeReferenceCenter(shape));
    const gp_Pnt savedWitness(anchor.value("curved_witness_x"),anchor.value("curved_witness_y"),anchor.value("curved_witness_z"));
    const gp_Pnt savedCenter(anchor.value("curved_center_x"),anchor.value("curved_center_y"),anchor.value("curved_center_z"));
    gp_Vec oldAxis(anchor.value("curved_axis_x"),anchor.value("curved_axis_y"),anchor.value("curved_axis_z"));
    if(type!=2.0&&std::abs(oldAxis.Magnitude()-1.0)>1e-5){
        fail(error,"Invalid cylindrical axis direction.");return false;
    }
    // Edits may change the solid center of mass without moving this face;
    // rigid translations move both. Attempt original-world then translated
    // witnesses, never guessing if either trial has multiple successors.
    // Freeform faces may deform slightly near their saved witnesses, but a
    // displaced, unrelated surface cannot be accepted solely because its
    // bounding-box center moved by the same amount. Explicit rigid stages
    // apply their own verified transforms before resolving the anchor.
    const int trials=(type==5.0||type==6.0)?1:2;
    for(int trial=0;trial<trials;++trial){
    gp_Pnt witness=savedWitness,oldCenter=savedCenter;
    if(trial){witness.Translate(shift);oldCenter.Translate(shift);}
    int matches=0;TopoDS_Face selected;
    for(TopExp_Explorer e(shape,TopAbs_FACE);e.More();e.Next()){
        const TopoDS_Face candidate=TopoDS::Face(e.Current());
        BRepAdaptor_Surface support(candidate);
        if((type==1.0&&support.GetType()!=GeomAbs_Cylinder)||
           (type==2.0&&support.GetType()!=GeomAbs_Sphere)||
           (type==3.0&&support.GetType()!=GeomAbs_Cone)||
           (type==4.0&&support.GetType()!=GeomAbs_Torus)||
           (type==5.0&&support.GetType()!=GeomAbs_BSplineSurface)||
           (type==6.0&&support.GetType()!=GeomAbs_BezierSurface))continue;
        if(type==5.0||type==6.0){
            // A limited shape deformation is tolerable, but a nearby,
            // unrelated surface must not silently capture the reference.
            // All candidates are checked and ambiguity remains a hard error.
            GeomAPI_ProjectPointOnSurf projection(witness,BRep_Tool::Surface(candidate));
            if(projection.NbPoints()<1||
               projection.LowerDistance()>std::max(1e-5,std::min(0.25,radius*0.01)))continue;
            Standard_Real u=0,v=0;
            projection.LowerDistanceParameters(u,v);
            gp_Pnt point;gp_Vec du,dv;
            support.D1(u,v,point,du,dv);
            gp_Vec normal=du.Crossed(dv);
            if(normal.SquareMagnitude()<1e-16)continue;
            normal.Normalize();
            if(std::abs(normal.Dot(oldAxis))<0.98)continue;
            BRepClass_FaceClassifier classification(candidate,point,1e-5);
            if(classification.State()!=TopAbs_IN)continue;
            if(second){
                gp_Pnt secondWitness(anchor.value("curved_witness2_x"),
                    anchor.value("curved_witness2_y"),anchor.value("curved_witness2_z"));
                if(trial)secondWitness.Translate(shift);
                GeomAPI_ProjectPointOnSurf other(secondWitness,BRep_Tool::Surface(candidate));
                if(other.NbPoints()<1||
                   other.LowerDistance()>std::max(1e-5,std::min(0.25,radius*0.01)))continue;
                Standard_Real su=0,sv=0;other.LowerDistanceParameters(su,sv);
                const gp_Pnt secondPoint=support.Value(su,sv);
                const BRepClass_FaceClassifier otherInside(candidate,secondPoint,1e-5);
                if(otherInside.State()!=TopAbs_IN||secondPoint.Distance(point)<1e-4)continue;
            }
            selected=candidate;++matches;
            continue;
        }
        double currentRadius=0,currentAux=0;gp_Pnt currentCenter;
        if(type==3.0){
            const auto cone=support.Cone();
            currentRadius=cone.RefRadius();currentAux=cone.SemiAngle();
            currentCenter=cone.Axis().Location();
            const gp_Vec currentAxis(cone.Axis().Direction());
            if(oldAxis.Dot(currentAxis)<1.0-1e-5||currentCenter.Distance(oldCenter)>1e-4)continue;
        }else if(type==4.0){
            const auto torus=support.Torus();
            currentRadius=torus.MajorRadius();currentAux=torus.MinorRadius();
            currentCenter=torus.Location();
            const gp_Vec currentAxis(torus.Axis().Direction());
            if(std::abs(oldAxis.Dot(currentAxis))<1.0-1e-5||currentCenter.Distance(oldCenter)>1e-4)continue;
        }
        if(type==1.0){
            const auto cylinder=support.Cylinder();
            currentRadius=cylinder.Radius();currentCenter=cylinder.Axis().Location();
            const gp_Vec currentAxis(cylinder.Axis().Direction());
            if(std::abs(oldAxis.Dot(currentAxis))<1.0-1e-5)continue;
            const gp_Vec delta(oldCenter,currentCenter);
            const gp_Vec perpendicular=delta-oldAxis.Multiplied(delta.Dot(oldAxis));
            if(perpendicular.Magnitude()>1e-4)continue;
        }else if(type==2.0){
            const auto sphere=support.Sphere();
            currentRadius=sphere.Radius();currentCenter=sphere.Location();
            if(currentCenter.Distance(oldCenter)>1e-4)continue;
        }
        if(std::abs(currentRadius-radius)>std::max(1e-5,radius*1e-6)||
           (extra&&std::abs(currentAux-aux)>std::max(1e-6,std::abs(aux)*1e-6)))continue;
        BRepClass_FaceClassifier classification(candidate,witness,1e-5);
        if(classification.State()!=TopAbs_IN)continue;
        selected=candidate;++matches;
    }
    if(matches==1){*face=selected;return true;}
    if(matches>1){
        fail(error,"Curved-face lineage is ambiguous after topology changes.");return false;
    }
    }
    fail(error,"Curved-face witness was removed or is no longer uniquely inside a successor.");
    return false;
}
namespace {
bool polygonFeature(const QString& type){
    return type=="Sketch polygon face boss"||type=="Sketch polygon face pocket"||
           type=="Sketch polygon extrude Z"||type=="Sketch polygon pocket Z"||
           type=="Sketch polygon revolve Z"||type=="Sketch polygons loft Z"||
           type=="Sketch polygon sweep 3D"||type=="Sketch polygon sweep spline 3D"||
           type=="Sketch multi-section loft Z"||
           type=="Sketch polygon revolve axis";
}
std::vector<polygon::Point> polygonPoints(const QMap<QString,double>& values,int count,const QString& prefix){
    std::vector<polygon::Point> points;points.reserve(static_cast<size_t>(count));
    for(int i=0;i<count;++i)
        points.push_back({values.value(prefix+QString::number(i)+"_x"),
                          values.value(prefix+QString::number(i)+"_y")});
    return points;
}
bool validateAdvancedPolygonFeature(const ShapeFeatureNode& node,QString* error){
    const bool spline=node.type=="Sketch polygon sweep spline 3D";
    const bool sweep=node.type=="Sketch polygon sweep 3D"||spline;
    const bool loft=node.type=="Sketch multi-section loft Z";
    const bool revolve=node.type=="Sketch polygon revolve axis";
    const double count=node.parameters.value("profile_count",-1);
    if(!std::isfinite(count)||count<3||count>24||std::floor(count)!=count){
        fail(error,"Advanced sketch requires 3 to 24 profile vertices.");return false;
    }
    const int n=static_cast<int>(count);
    const auto present=[&](const QString& k){return node.parameters.contains(k);};
    for(auto i=node.parameters.cbegin();i!=node.parameters.cend();++i)
        if(!std::isfinite(i.value())||std::abs(i.value())>1e5){
            fail(error,"Advanced sketch contains an invalid or out-of-range number.");return false;
        }
    std::vector<polygon::Point> profile;
    if(!loft){
        profile.reserve(static_cast<size_t>(n));
        for(int i=0;i<n;++i){const QString k="p"+QString::number(i);
            if(!present(k+"_x")||!present(k+"_y")){
                fail(error,"Sketch profile is missing a coordinate.");return false;
            }
            profile.push_back({node.parameters.value(k+"_x"),node.parameters.value(k+"_y")});
        }
    }
    std::string why;bool ok=false;
    if(sweep){
        const double length=node.parameters.value("path_count",-1);
        if(!std::isfinite(length)||length<2||length>24||std::floor(length)!=length){
            fail(error,"Sweep requires 2 to 24 3D path points.");return false;
        }
        const int m=static_cast<int>(length);
        if(node.parameters.size()!=2+2*n+3*m){fail(error,"Unexpected or missing sweep keys.");return false;}
        if(spline&&m<3){fail(error,"Spline-guided sweep requires at least 3 guide positions.");return false;}
        std::vector<advancedprofile::Point3> path;path.reserve(static_cast<size_t>(m));
        for(int i=0;i<m;++i){const QString p="t"+QString::number(i);
            if(!present(p+"_x")||!present(p+"_y")||!present(p+"_z")){
                fail(error,"Sweep path has an incomplete 3D point.");return false;
            }
            path.push_back({node.parameters.value(p+"_x"),node.parameters.value(p+"_y"),node.parameters.value(p+"_z")});
        }
        ok=advancedprofile::validateSweep(profile,path,&why);
    }else if(loft){
        const double countSections=node.parameters.value("section_count",-1);
        if(!std::isfinite(countSections)||countSections<2||countSections>8||std::floor(countSections)!=countSections){
            fail(error,"Loft requires 2 to 8 sections.");return false;
        }
        const int m=static_cast<int>(countSections);
        if(node.parameters.size()!=2+m*(1+2*n)){
            fail(error,"Unexpected or missing multi-section loft keys.");return false;
        }
        std::vector<advancedprofile::LoftSection> sections;sections.reserve(static_cast<size_t>(m));
        for(int i=0;i<m;++i){
            const QString prefix="s"+QString::number(i);
            if(!present(prefix+"_z")){fail(error,"Loft section is missing its elevation.");return false;}
            advancedprofile::LoftSection section;
            section.z=node.parameters.value(prefix+"_z");
            for(int j=0;j<n;++j){const QString p=prefix+"_p"+QString::number(j);
                if(!present(p+"_x")||!present(p+"_y")){
                    fail(error,"Loft section is missing a vertex.");return false;
                }
                section.points.push_back({node.parameters.value(p+"_x"),node.parameters.value(p+"_y")});
            }
            sections.push_back(std::move(section));
        }
        ok=advancedprofile::validateMultiLoft(sections,&why);
    }else if(revolve){
        const QStringList extra={"axis_x","axis_y","axis_z","origin_x","origin_y","origin_z","sweep_degrees"};
        if(node.parameters.size()!=1+2*n+extra.size()){
            fail(error,"Unexpected or missing revolve parameters.");return false;
        }
        for(const auto& k:extra)if(!present(k)){
            fail(error,"Revolve axis has a missing coordinate or angle.");return false;
        }
        ok=advancedprofile::validateAxisRevolve(profile,
            {node.parameters.value("origin_x"),node.parameters.value("origin_y"),node.parameters.value("origin_z")},
            {node.parameters.value("axis_x"),node.parameters.value("axis_y"),node.parameters.value("axis_z")},
            node.parameters.value("sweep_degrees"),&why);
    }
    if(!ok)fail(error,QString::fromStdString(why));
    return ok;
}
bool validatePolygonFeature(const ShapeFeatureNode& node,QString* error){
    const bool loft=node.type=="Sketch polygons loft Z";
    const bool revolve=node.type=="Sketch polygon revolve Z";
    const bool attached=node.type=="Sketch polygon face boss"||node.type=="Sketch polygon face pocket";
    const bool hasAnchor=node.parameters.contains("face_anchor_x");
    const bool hasBodyAnchor=node.parameters.contains("face_anchor_body_x");
    const bool hasWitness=node.parameters.contains("face_anchor_witness_x");
    const bool hasWitness2=node.parameters.contains("face_anchor_witness2_x");
    if(!completeOptionalFields(node.parameters,kFaceWitness2Fields)||
       (hasWitness2&&!hasWitness)||!completeOptionalFields(node.parameters,kFaceWitnessFields)||
       (hasWitness&&!hasAnchor)||
       !completeOptionalFields(node.parameters,kFaceBodyFields)||
       (hasBodyAnchor&&!hasAnchor)){
        fail(error,"Face reference center must be complete and accompanied by a face anchor.");return false;
    }
    if(hasAnchor&&!attached){
        fail(error,"Face anchor is only supported for face-attached polygon features.");return false;
    }
    const double length=node.parameters.value("profile_count",-1.0);
    if(!std::isfinite(length)||length<3||length>24||std::floor(length)!=length){
        fail(error,"Sketch polygon profile_count must be an integer from 3 to 24.");return false;
    }
    const int n=static_cast<int>(length);
    const int expected=1+2*n*(loft?2:1)+(attached?4:(revolve?0:2))+
        (hasAnchor?int(kFaceAnchorFields.size()):0)+
        (hasBodyAnchor?int(kFaceBodyFields.size()):0)+
        (hasWitness?int(kFaceWitnessFields.size()):0)+
        (hasWitness2?int(kFaceWitness2Fields.size()):0);
    if(node.parameters.size()!=expected){
        fail(error,"Sketch polygon parameters are missing or unexpected.");return false;
    }
    for(auto it=node.parameters.cbegin();it!=node.parameters.cend();++it)
        if(!std::isfinite(it.value())||std::abs(it.value())>1e5){
            fail(error,"Sketch polygon parameters must be finite and within the safe range.");return false;
        }
    for(int i=0;i<n;++i){
        const QString p="p"+QString::number(i);
        if(!node.parameters.contains(p+"_x")||!node.parameters.contains(p+"_y")){
            fail(error,"Sketch polygon is missing a profile vertex.");return false;
        }
        if(loft){const QString q="q"+QString::number(i);
            if(!node.parameters.contains(q+"_x")||!node.parameters.contains(q+"_y")){
                fail(error,"Sketch loft is missing an upper profile vertex.");return false;
            }
        }
    }
    if(attached){
        if(!node.parameters.contains("face_index")||!node.parameters.contains("depth")||
           !node.parameters.contains("u")||!node.parameters.contains("v")||
           node.parameters.value("face_index")<1||node.parameters.value("face_index")>10000||
           std::floor(node.parameters.value("face_index"))!=node.parameters.value("face_index")||
           node.parameters.value("depth")<1e-4||node.parameters.value("depth")>100000){
            fail(error,"Face-attached polygon requires a face index, local offsets, and positive depth.");return false;
        }
        if(hasAnchor){
            for(const QString& key:kFaceAnchorFields)
                if(!node.parameters.contains(key)){
                    fail(error,"Face-attached polygon has an incomplete face anchor.");return false;
                }
            for(const QStringList& fields:{kFaceWitnessFields,kFaceWitness2Fields})
                for(const QString& key:fields)
                    if(node.parameters.contains(fields.first())&&
                       (!node.parameters.contains(key)||!std::isfinite(node.parameters.value(key))||
                        std::abs(node.parameters.value(key))>1e9)){
                        fail(error,"Face witness data is incomplete or invalid.");return false;
                    }
            for(const QString& key:kFaceBodyFields)
                if(hasBodyAnchor&&!node.parameters.contains(key)){
                    fail(error,"Face-attached polygon has an incomplete reference center.");return false;
                }
            const double nx=node.parameters.value("face_anchor_nx");
            const double ny=node.parameters.value("face_anchor_ny");
            const double nz=node.parameters.value("face_anchor_nz");
            if(std::abs(nx*nx+ny*ny+nz*nz-1.)>1e-4||
               node.parameters.value("face_anchor_area")<=1e-8){
                fail(error,"Face-attached polygon has an invalid face anchor.");return false;
            }
        }
    }
    if(!revolve&&!attached&&(!node.parameters.contains("base_z")||!node.parameters.contains("height")||
                  node.parameters.value("height")<1e-4)){
        fail(error,"Sketch feature needs a positive height and base_z.");return false;
    }
    const auto lower=polygonPoints(node.parameters,n,"p");
    std::string issue;
    const bool okay=loft?polygon::validateLoft(lower,polygonPoints(node.parameters,n,"q"),
                                               node.parameters.value("height"),&issue):
        polygon::validate(lower,revolve,&issue);
    if(!okay){fail(error,QString::fromStdString(issue));return false;}
    return true;
}
bool validateOne(const ShapeFeatureNode& node,QString* error){
    if(node.id.isEmpty()||node.id.size()>64||!specifications().contains(node.type)){
        fail(error,"Unknown feature operation or invalid feature ID.");return false;
    }
    if(node.type=="Sketch polygon sweep 3D"||node.type=="Sketch polygon sweep spline 3D"||
       node.type=="Sketch multi-section loft Z"||
       node.type=="Sketch polygon revolve axis")return validateAdvancedPolygonFeature(node,error);
    if(polygonFeature(node.type))return validatePolygonFeature(node,error);
    if(node.type=="Fillet selected edges"||node.type=="Chamfer selected edges"){
        const double rawCount=node.parameters.value("edge_count",-1);
        if(!std::isfinite(rawCount)||std::floor(rawCount)!=rawCount||rawCount<2||rawCount>32){
            fail(error,"Edge groups require 2-32 distinct witnessed edges.");return false;
        }
        const QString sizeKey=node.type=="Fillet selected edges"?"radius":"distance";
        const double size=node.parameters.value(sizeKey,-1);
        if(!std::isfinite(size)||size<1e-5||size>1e6){
            fail(error,"Edge group size must be finite and positive.");return false;
        }
        const int count=static_cast<int>(rawCount);
        // Validate every member through the unchanged single-edge contract;
        // this enforces complete witnesses, length, curvature and finite values.
        int consumed=2;
        for(int i=0;i<count;++i){
            const QString prefix=QStringLiteral("member_%1_").arg(i);
            ShapeFeatureNode member;
            member.id=QStringLiteral("w%1").arg(i);
            member.type=sizeKey=="radius"?"Fillet selected edge":"Chamfer selected edge";
            member.parameters.insert(sizeKey,size);
            for(auto it=node.parameters.cbegin();it!=node.parameters.cend();++it)
                if(it.key().startsWith(prefix)){
                    if(it.key()==prefix+"size"){
                        if(!std::isfinite(it.value())||it.value()<1e-5||it.value()>1e6){
                            fail(error,"Individual edge sizes must be finite and positive.");return false;
                        }
                    }else member.parameters.insert(it.key().mid(prefix.size()),it.value());
                    ++consumed;
                }
            if(!member.parameters.contains("edge_index")||
               !member.parameters.contains("edge_anchor_version")||
               !member.parameters.contains("edge_body_x")||
               !validateOne(member,error)){
                if(error&&error->isEmpty())*error="Edge group member has an invalid or missing witness.";
                return false;
            }
        }
        if(consumed!=node.parameters.size()){
            fail(error,"Edge group contains unknown or unowned parameters.");return false;
        }
        return true;
    }
    const QStringList fields=specifications().value(node.type);
    const bool hasAnchor=node.parameters.contains("face_anchor_x");
    const bool hasCurved=node.parameters.contains("curved_type");
    const bool hasEdgeAnchor=node.parameters.contains("edge_x");
    const bool hasFaceBody=node.parameters.contains("face_anchor_body_x");
    const bool hasFaceWitness=node.parameters.contains("face_anchor_witness_x");
    const bool hasFaceWitness2=node.parameters.contains("face_anchor_witness2_x");
    const bool hasEdgeBody=node.parameters.contains("edge_body_x");
    const bool hasEdgeWitness=node.parameters.contains("edge_anchor_version");
    if(!completeOptionalFields(node.parameters,kEdgeWitnessFields)||
       (hasEdgeWitness&&!hasEdgeAnchor)||
       !completeOptionalFields(node.parameters,kFaceWitnessFields)||
       !completeOptionalFields(node.parameters,kFaceWitness2Fields)||
       (hasFaceWitness2&&!hasFaceWitness)||
       (hasFaceWitness&&!hasAnchor)||
       !completeOptionalFields(node.parameters,kFaceBodyFields)||
       !completeOptionalFields(node.parameters,kEdgeBodyFields)||
       (hasFaceBody&&!hasAnchor)||(hasEdgeBody&&!hasEdgeAnchor)){
        fail(error,"Incomplete or orphaned geometry reference center.");return false;
    }
    if(hasCurved){
        if(node.type!=QStringLiteral("Open-face shell")||hasAnchor||hasEdgeAnchor){
            fail(error,"Curved face references are currently supported for shell openings only.");return false;
        }
        const bool extra=node.parameters.value("curved_type")==3.0||node.parameters.value("curved_type")==4.0;
        const bool second=node.parameters.contains("curved_witness2_x");
        const int expected=int(kCurvedAnchorFields.size())+(extra?1:0)+(second?3:0);
        if(second&&(node.parameters.value("curved_type")!=5.0&&node.parameters.value("curved_type")!=6.0)){
            fail(error,"Second curved witness requires a freeform shell face.");return false;
        }
        if(second)for(const QString& key:kCurvedWitness2Fields)
            if(!node.parameters.contains(key)||!std::isfinite(node.parameters.value(key))){
                fail(error,"Second curved face witness is incomplete.");return false;
            }
        if(node.parameters.size()!=fields.size()+expected){
            fail(error,"Curved shell face anchor is incomplete or contains unknown fields.");return false;
        }
        for(const QString& key:kCurvedAnchorFields){
            if(!node.parameters.contains(key)||!std::isfinite(node.parameters.value(key))||
               std::abs(node.parameters.value(key))>1e9){
                fail(error,"Curved shell reference contains invalid support values.");return false;
            }
        }
        if((extra&&(!node.parameters.contains("curved_aux")||!std::isfinite(node.parameters.value("curved_aux"))))||
           node.parameters.value("curved_radius")<=1e-8||
           (node.parameters.value("curved_type")!=1.0&&node.parameters.value("curved_type")!=2.0&&
            node.parameters.value("curved_type")!=3.0&&node.parameters.value("curved_type")!=4.0&&
            node.parameters.value("curved_type")!=5.0&&node.parameters.value("curved_type")!=6.0)){
            fail(error,"Curved shell support descriptor is invalid.");return false;
        }
    }
    const int required=fields.size()+(hasCurved?int(kCurvedAnchorFields.size())+
                              (node.parameters.contains("curved_aux")?1:0)+
                              (node.parameters.contains("curved_witness2_x")?3:0):0)+
        (hasAnchor?int(kFaceAnchorFields.size()):0)+
        (hasEdgeAnchor?int(kEdgeAnchorFields.size()):0)+
        (hasFaceBody?int(kFaceBodyFields.size()):0)+
        (hasFaceWitness?int(kFaceWitnessFields.size()):0)+
        (hasFaceWitness2?int(kFaceWitness2Fields.size()):0)+
        (hasEdgeBody?int(kEdgeBodyFields.size()):0)+
        (hasEdgeWitness?int(kEdgeWitnessFields.size()):0);
    if(node.parameters.size()!=required||(hasAnchor&&!usesPlanarFace(node.type))||
       (hasEdgeAnchor&&!usesStableEdge(node.type))||(hasAnchor&&hasEdgeAnchor)){
        fail(error,"Feature parameter names do not match the operation.");return false;
    }
    if(hasEdgeAnchor){
        if(hasEdgeWitness){
            for(const QString& key:kEdgeWitnessFields)
                if(!node.parameters.contains(key)||!std::isfinite(node.parameters.value(key))||
                   std::abs(node.parameters.value(key))>1e9){
                    fail(error,"Incomplete or invalid witnessed edge reference.");return false;
                }
            if((node.parameters.value("edge_anchor_version")!=2.0&&
                node.parameters.value("edge_anchor_version")!=3.0)||
               (node.parameters.value("edge_curve_kind")==3.0&&
                node.parameters.value("edge_anchor_version")!=3.0)||
               (node.parameters.value("edge_curve_kind")!=1.0&&
                node.parameters.value("edge_curve_kind")!=2.0&&
                node.parameters.value("edge_curve_kind")!=3.0)||
               node.parameters.value("edge_curvature")<0.0||
               (node.parameters.value("edge_curve_kind")==1.0&&
                node.parameters.value("edge_curvature")!=0.0)||
               std::hypot(node.parameters.value("edge_tangent_x"),
                   std::hypot(node.parameters.value("edge_tangent_y"),
                              node.parameters.value("edge_tangent_z")))<0.99){
                fail(error,"Invalid witnessed edge geometry.");return false;
            }
        }
        for(const QString& key:kEdgeBodyFields)
            if(hasEdgeBody&&(!node.parameters.contains(key)||
                !std::isfinite(node.parameters.value(key))||
                std::abs(node.parameters.value(key))>1e9)){
                fail(error,"Invalid edge reference center.");return false;
            }
        for(const QString& key:kEdgeAnchorFields){
            const double value=node.parameters.value(key);
            if(!node.parameters.contains(key)||!std::isfinite(value)||std::abs(value)>1e9){
                fail(error,"Incomplete or invalid stable edge anchor.");return false;
            }
        }
        if(node.parameters.value("edge_length")<=1e-9){
            fail(error,"Stable edge anchor must have a positive length.");return false;
        }
    }
    if(hasAnchor){
        for(const QStringList& fields:{kFaceWitnessFields,kFaceWitness2Fields})
            for(const QString& key:fields)
                if(node.parameters.contains(fields.first())&&
                   (!node.parameters.contains(key)||!std::isfinite(node.parameters.value(key))||
                    std::abs(node.parameters.value(key))>1e9)){
                    fail(error,"Invalid planar face witness.");return false;
                }
        for(const auto& key:kFaceBodyFields)
            if(hasFaceBody&&(!node.parameters.contains(key)||
                !std::isfinite(node.parameters.value(key))||
                std::abs(node.parameters.value(key))>1e9)){
                fail(error,"Invalid planar face reference center.");return false;
            }
        for(const auto& key:kFaceAnchorFields)
            if(!node.parameters.contains(key)||!std::isfinite(node.parameters.value(key))||
               std::abs(node.parameters.value(key))>1e9){
                fail(error,"Incomplete planar face anchor.");return false;
            }
        const double squared=node.parameters.value("face_anchor_nx")*node.parameters.value("face_anchor_nx")+
            node.parameters.value("face_anchor_ny")*node.parameters.value("face_anchor_ny")+
            node.parameters.value("face_anchor_nz")*node.parameters.value("face_anchor_nz");
        if(std::abs(squared-1.)>1e-4||node.parameters.value("face_anchor_area")<=1e-8){
            fail(error,"Invalid planar face anchor orientation or area.");return false;
        }
    }
    for(const QString& name:fields){
        if(!node.parameters.contains(name)||!std::isfinite(node.parameters.value(name))){
            fail(error,QStringLiteral("Feature '%1' has a missing/non-finite parameter: %2.").arg(node.id,name));return false;
        }
        const double v=node.parameters.value(name);
        if(std::abs(v)>1e6 || ((name=="outer_radius"||name=="inner_radius"||
            name=="width0"||name=="depth0"||name=="width1"||name=="depth1"||
            name=="path_length")&&v<1e-5) ||
           ((name=="face_index"||name=="edge_index")&&
            (v<1||v>10000||std::floor(v)!=v)) ||
           (name=="count"&&(v<2||v>24||std::floor(v)!=v)) ||
           (name=="sweep_degrees"&&(v<=0||v>360)) ||
           (name=="angle_degrees"&&(std::abs(v)>45||std::abs(v)<0.01)) ||
           (name=="factor"&&(v<1e-4||v>1e4)) ||
           ((name=="radius"||name=="distance"||name=="depth"||name=="spacing"||
             name=="center_spacing"||name=="pitch_u"||name=="pitch_v"||name=="pcd"||
             name=="width"||name=="height"||name=="thickness"||
             name=="head_radius"||name=="recess_depth")&&(v<1e-5||v>1e6))){
            fail(error,QStringLiteral("Feature '%1' has an out-of-range %2.").arg(node.id,name));return false;
        }
    }
    if(node.type=="Face counterbore through all"||node.type=="Face countersink through all"){
        if(node.parameters.value("head_radius")<=node.parameters.value("radius")+1e-6){
            fail(error,"Fastener recess radius must exceed the through-bore radius.");return false;
        }
        if(node.type=="Face countersink through all"){
            const double angle=node.parameters.value("included_angle");
            if(angle<60.0||angle>120.0){
                fail(error,"Countersink included angle must be from 60 to 120 degrees.");return false;
            }
        }
    }
    if(node.type=="Face hole grid through all"||node.type=="Face bolt circle through all"){
        std::vector<cad::hole_patterns::Point2> locations;
        std::string issue;
        bool okay=false;
        if(node.type=="Face hole grid through all"){
            const double columns=node.parameters.value("columns"),rows=node.parameters.value("rows");
            if(std::floor(columns)==columns&&std::floor(rows)==rows)
                okay=cad::hole_patterns::grid(node.parameters.value("u"),node.parameters.value("v"),
                    node.parameters.value("radius"),static_cast<int>(columns),static_cast<int>(rows),
                    node.parameters.value("pitch_u"),node.parameters.value("pitch_v"),
                    node.parameters.value("rotation_degrees"),&locations,&issue);
        }else{
            const double count=node.parameters.value("count");
            if(std::floor(count)==count)
                okay=cad::hole_patterns::boltCircle(node.parameters.value("u"),node.parameters.value("v"),
                    node.parameters.value("radius"),static_cast<int>(count),node.parameters.value("pcd"),
                    node.parameters.value("start_angle_degrees"),&locations,&issue);
        }
        if(!okay){fail(error,QString::fromStdString(issue.empty()?"Pattern counts must be integral and within limits.":issue));return false;}
    }
    // Avoid constructing gp_Dir from null vectors, which raises an OCCT
    // exception only after the user has committed the feature edit.
    const auto lengthSquared=[&](const QString& x,const QString& y,const QString& z){
        const double a=node.parameters.value(x),b=node.parameters.value(y),c=node.parameters.value(z);
        return a*a+b*b+c*c;
    };
    if((node.type=="Rotate arbitrary axis"||node.type=="Circular pattern arbitrary axis")&&
       lengthSquared("axis_x","axis_y","axis_z")<1e-12){
        fail(error,"Custom rotation axis must have a nonzero direction.");return false;
    }
    if((node.type=="Cylinder cut arbitrary axis"||
        node.type=="Cylinder through all arbitrary axis")&&
       lengthSquared("axis_x","axis_y","axis_z")<1e-12){
        fail(error,"Cylinder cut needs a nonzero drill axis direction.");return false;
    }
    if(node.type=="Mirror arbitrary plane"&&
       lengthSquared("normal_x","normal_y","normal_z")<1e-12){
        fail(error,"Custom mirror plane must have a nonzero normal.");return false;
    }
    if(node.type=="Linear pattern vector"&&lengthSquared("dx","dy","dz")<1e-12){
        fail(error,"Vector pattern needs a nonzero spacing vector.");return false;
    }
    return true;
}
bool geometryOkay(const TopoDS_Shape& shape,bool surface,QString* error){
    if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid()){
        fail(error,"Feature produced invalid B-rep geometry.");return false;
    }
    if(!surface){
        int solidCount=0;
        for(TopExp_Explorer e(shape,TopAbs_SOLID);e.More();e.Next())++solidCount;
        if(!solidCount){fail(error,"Solid feature did not produce a solid.");return false;}
        GProp_GProps mass;BRepGProp::VolumeProperties(shape,mass);
        if(!std::isfinite(mass.Mass())||mass.Mass()<=1e-9){fail(error,"Feature removed the entire solid.");return false;}
    }
    return true;
}
}
namespace {
QMap<QString,double> edgeDescriptor(const TopoDS_Edge& edge){
    GProp_GProps props;BRepGProp::LinearProperties(edge,props);
    const gp_Pnt at=props.CentreOfMass();
    return {{"edge_x",at.X()},{"edge_y",at.Y()},{"edge_z",at.Z()},
            {"edge_length",props.Mass()}};
}
}
// MapShapes visits topologically distinct edges once. TopExp_Explorer can
// encounter the same TShape once per adjacent face; treating those occurrences
// as separate candidates makes an otherwise unambiguous anchor fail.
// Both capture and resolution use the same 1-based unique-edge index scheme.
bool captureStableEdgeAnchor(const TopoDS_Shape& body,int edgeIndex,
                             QMap<QString,double>* anchor,QString* error){
    if(!anchor||body.IsNull()||edgeIndex<=0){fail(error,"Invalid edge selection.");return false;}
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(body,TopAbs_EDGE,edges);
    if(edgeIndex>edges.Extent()){
        fail(error,"Selected edge does not exist.");return false;
    }
    const auto descriptor=edgeDescriptor(TopoDS::Edge(edges.FindKey(edgeIndex)));
    if(descriptor.value("edge_length")<=1e-9){
        fail(error,"Cannot anchor a zero-length edge.");return false;
    }
    QMap<QString,double> captured=descriptor;
    const gp_Pnt reference=shapeReferenceCenter(body);
    captured.insert("edge_body_x",reference.X());
    captured.insert("edge_body_y",reference.Y());
    captured.insert("edge_body_z",reference.Z());
    const TopoDS_Edge edge=TopoDS::Edge(edges.FindKey(edgeIndex));
    const BRepAdaptor_Curve curve(edge);
    // v2 line/circle witnesses and v3 spline witnesses use the same fixed
    // persisted fields. No existing project file needs migration. A v3 spline
    // witness is a local point/tangent on the actual trimmed curve; it does
    // not claim stability if a parameter edit moves the witnessed region.
    if(curve.GetType()==GeomAbs_Line||curve.GetType()==GeomAbs_Circle||
       curve.GetType()==GeomAbs_BSplineCurve||curve.GetType()==GeomAbs_BezierCurve){
        const double first=curve.FirstParameter(),last=curve.LastParameter();
        if(std::isfinite(first)&&std::isfinite(last)&&last-first>1e-8){
            gp_Pnt witness;gp_Vec tangent;
            curve.D1((first+last)*0.5,witness,tangent);
            if(tangent.SquareMagnitude()>1e-18){
                tangent.Normalize();
                const bool circle=curve.GetType()==GeomAbs_Circle;
                const bool spline=curve.GetType()==GeomAbs_BSplineCurve||
                                  curve.GetType()==GeomAbs_BezierCurve;
                captured.insert("edge_anchor_version",spline?3.0:2.0);
                captured.insert("edge_curve_kind",spline?3.0:(circle?2.0:1.0));
                captured.insert("edge_witness_x",witness.X());
                captured.insert("edge_witness_y",witness.Y());
                captured.insert("edge_witness_z",witness.Z());
                captured.insert("edge_tangent_x",tangent.X());
                captured.insert("edge_tangent_y",tangent.Y());
                captured.insert("edge_tangent_z",tangent.Z());
                double curvature=circle?1.0/curve.Circle().Radius():0.0;
                if(spline){
                    gp_Pnt at;gp_Vec d1,d2;
                    curve.D2((first+last)*0.5,at,d1,d2);
                    if(d1.SquareMagnitude()>1e-18)
                        curvature=d1.Crossed(d2).Magnitude()/std::pow(d1.Magnitude(),3.0);
                }
                captured.insert("edge_curvature",curvature);
            }
        }
    }
    *anchor=std::move(captured);
    return true;
}
bool resolveStableEdgeAnchor(const TopoDS_Shape& body,const QMap<QString,double>& anchor,
                             int* edgeIndex,QString* error){
    const bool hasBody=anchor.contains("edge_body_x");
    const bool witnessed=anchor.contains("edge_anchor_version");
    if(!edgeIndex||body.IsNull()||!completeOptionalFields(anchor,kEdgeBodyFields)||
       !completeOptionalFields(anchor,kEdgeWitnessFields)||
       anchor.size()!=(hasBody?7:4)+(witnessed?int(kEdgeWitnessFields.size()):0)||
       !anchor.contains("edge_x")||
       !anchor.contains("edge_y")||!anchor.contains("edge_z")||!anchor.contains("edge_length")){
        fail(error,"Incomplete edge anchor.");return false;
    }
    for(auto it=anchor.cbegin();it!=anchor.cend();++it)
        if(!std::isfinite(it.value())||std::abs(it.value())>1e9){
            fail(error,"Invalid edge anchor values.");return false;
        }
    const double oldLength=anchor.value("edge_length");
    if(oldLength<=1e-9){fail(error,"Edge anchor length must be positive.");return false;}
    gp_Pnt old(anchor.value("edge_x"),anchor.value("edge_y"),anchor.value("edge_z"));
    if(hasBody){
        const gp_Pnt oldBody(anchor.value("edge_body_x"),anchor.value("edge_body_y"),anchor.value("edge_body_z"));
        old.Translate(gp_Vec(oldBody,shapeReferenceCenter(body)));
    }
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(body,TopAbs_EDGE,edges);
    if(witnessed){
        const double version=anchor.value("edge_anchor_version");
        const double kind=anchor.value("edge_curve_kind");
        if((version!=2.0&&version!=3.0)||
           (kind!=1.0&&kind!=2.0&&kind!=3.0)||
           (kind==3.0&&version!=3.0)||
           anchor.value("edge_curvature")<0.0){
            fail(error,"Invalid witnessed edge anchor version or curve type.");return false;
        }
        const gp_Pnt savedWitness(anchor.value("edge_witness_x"),anchor.value("edge_witness_y"),
                                  anchor.value("edge_witness_z"));
        const gp_Vec translation=hasBody?
            gp_Vec(gp_Pnt(anchor.value("edge_body_x"),anchor.value("edge_body_y"),
                          anchor.value("edge_body_z")),shapeReferenceCenter(body)):gp_Vec(0,0,0);
        const gp_Vec direction(anchor.value("edge_tangent_x"),anchor.value("edge_tangent_y"),
                               anchor.value("edge_tangent_z"));
        if(direction.SquareMagnitude()<0.99||direction.SquareMagnitude()>1.01||
           (anchor.value("edge_curve_kind")==1.0&&anchor.value("edge_curvature")!=0.0)){
            fail(error,"Malformed witnessed edge direction.");return false;
        }
        // A length-proportional tolerance without an upper bound can be
        // millimetres wide for long machine rails, turning distinct edges
        // into the same witnessed reference. A millimetre-domain geometric
        // identity must remain a *local* tolerance, not a fraction of stock.
        const double tolerance=std::clamp(oldLength*1e-6,1e-5,1e-3);
        // Both absolute and body-relative witness placement are legitimate
        // interpretations of a persisted old CAD edge. Do NOT return after
        // the first unique placement: another occurrence can be equally valid
        // under the second interpretation after a split/merge or compound
        // duplication. All available placements must identify one *same*
        // edge; otherwise explicit re-selection is safer.
        int agreedEdge=0;
        for(int trial=0;trial<(hasBody?2:1);++trial){
        gp_Pnt witness=savedWitness;
        if(trial)witness.Translate(translation);
        int matches=0,selected=0;
        for(int position=1;position<=edges.Extent();++position){
            const TopoDS_Edge edge=TopoDS::Edge(edges.FindKey(position));
            const BRepAdaptor_Curve adapted(edge);
            const bool circle=kind==2.0;
            const bool compatible=kind==3.0?
                (adapted.GetType()==GeomAbs_BSplineCurve||adapted.GetType()==GeomAbs_BezierCurve):
                adapted.GetType()==(circle?GeomAbs_Circle:GeomAbs_Line);
            if(!compatible)continue;
            if(circle&&std::abs(1.0/adapted.Circle().Radius()-anchor.value("edge_curvature"))>
                std::max(1e-6,anchor.value("edge_curvature")*1e-4))continue;
            Standard_Real first=0,last=0;
            const Handle(Geom_Curve) geometry=BRep_Tool::Curve(edge,first,last);
            if(geometry.IsNull()||!std::isfinite(first)||!std::isfinite(last)||last-first<1e-8)continue;
            GeomAPI_ProjectPointOnCurve projected(witness,geometry,first,last);
            if(projected.NbPoints()<1||projected.LowerDistance()>tolerance)continue;
            const double at=projected.LowerDistanceParameter();
            // Avoid reattaching to either half if a split crosses the witness.
            if(at<=first+1e-7*(last-first)||at>=last-1e-7*(last-first))continue;
            gp_Pnt point;gp_Vec tangent;
            adapted.D1(at,point,tangent);
            if(tangent.SquareMagnitude()<1e-18)continue;
            if(kind==3.0){
                gp_Pnt atPoint;gp_Vec d1,d2;
                adapted.D2(at,atPoint,d1,d2);
                if(d1.SquareMagnitude()<1e-18)continue;
                const double curvature=d1.Crossed(d2).Magnitude()/std::pow(d1.Magnitude(),3.0);
                const double saved=anchor.value("edge_curvature");
                if(!std::isfinite(curvature)||std::abs(curvature-saved)>
                   std::max(1e-4,saved*0.05))continue;
            }
            tangent.Normalize();
            if(std::abs(direction.Dot(tangent))<0.995)continue;
            ++matches;selected=position;
        }
        if(matches>1){
            fail(error,"Edge lineage is ambiguous after a topology change.");return false;
        }
        if(matches==1){
            if(agreedEdge!=0&&agreedEdge!=selected){
                fail(error,"Absolute and body-relative edge witnesses resolve different successors; reselect the edge.");
                return false;
            }
            agreedEdge=selected;
        }
        }
        if(agreedEdge!=0){*edgeIndex=agreedEdge;if(error)error->clear();return true;}
        fail(error,"Edge witness is missing or lies on a split boundary; reselect the edge.");
        return false;
    }
    double best=1e100,runnerUp=1e100;
    int selected=0;
    for(int position=1;position<=edges.Extent();++position){
        const auto desc=edgeDescriptor(TopoDS::Edge(edges.FindKey(position)));
        const double length=desc.value("edge_length");
        if(length<=1e-9)continue;
        const gp_Pnt at(desc.value("edge_x"),desc.value("edge_y"),desc.value("edge_z"));
        const double score=at.Distance(old)/std::max(1.,oldLength)+std::abs(std::log(length/oldLength));
        if(score<best){runnerUp=best;best=score;selected=position;}
        else runnerUp=std::min(runnerUp,score);
    }
    if(!selected||best>0.5||runnerUp-best<0.01){
        fail(error,"Edge geometry could not be uniquely matched; reselect the edge.");return false;
    }
    *edgeIndex=selected;
    return true;
}


// Special-purpose full-circle identity used ONLY when updating a saved
// diameter, hole center, or center-distance drawing annotation. A circular
// change moves the old on-edge witness radially, so the generic resolver is
// correctly strict and cannot follow it. Preserve the plane center and normal
// instead, with an explicit size bound and strict unique-candidate rule.
bool resolveResizedFullCircleForDrawing(const TopoDS_Shape& body,
                                       const QMap<QString,double>& anchor,
                                       int* edgeIndex,QString* error){
    if(!edgeIndex||body.IsNull()||anchor.value("edge_anchor_version")!=2.0||
       anchor.value("edge_curve_kind")!=2.0||
       !completeOptionalFields(anchor,kEdgeBodyFields)||
       !completeOptionalFields(anchor,kEdgeWitnessFields)||
       anchor.size()!=4+3+int(kEdgeWitnessFields.size())){
        fail(error,"This drawing annotation lacks a complete circular witness.");return false;
    }
    // The stored anchor must have every required field. The size check above
    // alone would otherwise permit an unknown key to replace a required one.
    for(const auto& key:kEdgeAnchorFields)
        if(!anchor.contains(key)){
            fail(error,"Missing circle edge descriptor field.");return false;
        }
    for(const auto& key:kEdgeBodyFields)
        if(!anchor.contains(key)){
            fail(error,"Missing source-body reference position.");return false;
        }
    for(const auto& key:kEdgeWitnessFields)
        if(!anchor.contains(key)){
            fail(error,"Missing circle witness field.");return false;
        }
    for(auto it=anchor.cbegin();it!=anchor.cend();++it)
        if(!std::isfinite(it.value())||std::abs(it.value())>1e9){
            fail(error,"The circular drawing reference contains invalid values.");return false;
        }
    const double curvature=anchor.value("edge_curvature");
    if(curvature<=1e-12){fail(error,"The drawing reference has no valid circle radius.");return false;}
    const double radius=1.0/curvature;
    const double pi=std::acos(-1.0);
    const double oldLength=anchor.value("edge_length");
    if(!std::isfinite(radius)||radius>1e9||
       std::abs(oldLength-2*pi*radius)>std::max(1e-4,2*pi*radius*5e-4)){
        fail(error,"Radius recovery requires a saved full-circle edge.");return false;
    }
    const gp_Pnt center(anchor.value("edge_x"),anchor.value("edge_y"),anchor.value("edge_z"));
    const gp_Pnt witness(anchor.value("edge_witness_x"),anchor.value("edge_witness_y"),
                         anchor.value("edge_witness_z"));
    gp_Vec radial(center,witness);
    gp_Vec tangent(anchor.value("edge_tangent_x"),anchor.value("edge_tangent_y"),
                   anchor.value("edge_tangent_z"));
    if(radial.SquareMagnitude()<1e-14||
       std::abs(radial.Magnitude()-radius)>std::max(1e-4,radius*1e-4)||
       std::abs(tangent.SquareMagnitude()-1.0)>0.01){
        fail(error,"Malformed saved circle center, radius or tangent.");return false;
    }
    radial.Normalize();
    if(std::abs(radial.Dot(tangent))>0.01){
        fail(error,"Saved circle radius and tangent are inconsistent.");return false;
    }
    gp_Vec normal=radial.Crossed(tangent);
    normal.Normalize();
    const auto point=[](const gp_Pnt& p){
        return circular_lineage::Point3{p.X(),p.Y(),p.Z()};
    };
    const auto axis=[](const gp_Dir& dir){
        return circular_lineage::Point3{dir.X(),dir.Y(),dir.Z()};
    };
    const circular_lineage::Reference original{point(center),
        {normal.X(),normal.Y(),normal.Z()},radius};
    const gp_Pnt previousBody(anchor.value("edge_body_x"),anchor.value("edge_body_y"),
                              anchor.value("edge_body_z"));
    const gp_Vec bodyShift(previousBody,shapeReferenceCenter(body));
    const gp_Pnt shifted(center.X()+bodyShift.X(),center.Y()+bodyShift.Y(),center.Z()+bodyShift.Z());
    const circular_lineage::Reference moved{point(shifted),original.axis,radius};
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(body,TopAbs_EDGE,edges);
    int matches=0,selected=0;
    for(int i=1;i<=edges.Extent();++i){
        const BRepAdaptor_Curve current(TopoDS::Edge(edges.FindKey(i)));
        if(current.GetType()!=GeomAbs_Circle||
           !std::isfinite(current.FirstParameter())||
           !std::isfinite(current.LastParameter())||
           std::abs((current.LastParameter()-current.FirstParameter())-2*pi)>1e-4)continue;
        const gp_Circ circle=current.Circle();
        const circular_lineage::Reference candidate{point(circle.Location()),
            axis(circle.Axis().Direction()),circle.Radius()};
        if(!circular_lineage::compatible(original,candidate)&&
           !circular_lineage::compatible(moved,candidate))continue;
        ++matches;selected=i;
    }
    if(matches!=1){
        fail(error,matches>1?"More than one circle could match the edited drawing reference.":
            "The drawing circle moved, changed too much, or was removed; reselect it.");
        return false;
    }
    *edgeIndex=selected;
    if(error)error->clear();
    return true;
}

bool planShapeFeatureReuse(const QVector<ShapeFeatureNode>& previous,
                           const QVector<ShapeFeatureNode>& proposed,
                           const QVector<bool>& previousOutputAvailable,
                           QVector<int>* previousIndex,QString* error){
    if(!previousIndex){fail(error,"Missing feature reuse plan output.");return false;}
    const auto convert=[](const QVector<ShapeFeatureNode>& nodes){
        std::vector<dependency::FeatureRecord> result;
        result.reserve(static_cast<size_t>(nodes.size()));
        for(const auto& node:nodes){
            dependency::FeatureRecord record;
            record.id=node.id.toUtf8().toStdString();
            record.type=node.type.toUtf8().toStdString();
            record.suppressed=node.suppressed;
            for(auto it=node.parameters.cbegin();it!=node.parameters.cend();++it)
                record.parameters.emplace_back(it.key().toUtf8().toStdString(),it.value());
            for(const QString& source:node.inputs)
                record.inputs.push_back(source.toUtf8().toStdString());
            result.push_back(std::move(record));
        }
        return result;
    };
    std::vector<bool> available;
    available.reserve(static_cast<size_t>(previousOutputAvailable.size()));
    for(bool value:previousOutputAvailable)available.push_back(value);
    dependency::ReusePlan plan;
    std::string issue;
    if(!dependency::planReuse(convert(previous),convert(proposed),available,&plan,&issue)){
        fail(error,QString::fromStdString(issue));return false;
    }
    QVector<int> result;
    result.reserve(static_cast<qsizetype>(plan.previousIndex.size()));
    for(int old:plan.previousIndex)result.append(old);
    *previousIndex=std::move(result);
    return true;
}

bool validateShapeFeatureNodes(const QVector<ShapeFeatureNode>& nodes,QString* error){
    if(nodes.size()>40){fail(error,"A feature tree supports at most 40 nodes.");return false;}
    QSet<QString> identities;
    for(const auto& node:nodes){
        if(!validateOne(node,error))return false;
        if(node.id=="@base"||identities.contains(node.id)){
            fail(error,"Feature IDs must be unique and cannot be '@base'.");return false;
        }
        const bool combine=node.type=="Fuse branches"||node.type=="Subtract branches"||node.type=="Intersect branches";
        if((combine&&node.inputs.size()!=2)||(!combine&&node.inputs.size()>1)){
            fail(error,"A Boolean combine needs two sources; other nodes accept zero or one.");return false;
        }
        QSet<QString> once;
        for(const auto& source:node.inputs){
            if(source.isEmpty()||once.contains(source)||(source!="@base"&&!identities.contains(source))){
                fail(error,QStringLiteral("Feature '%1' references an unknown, later, or duplicate input: %2.").arg(node.id,source));
                return false;
            }
            once.insert(source);
        }
        identities.insert(node.id);
    }
    return true;
}
bool capturePlanarFaceAnchor(const TopoDS_Shape& shape,int faceIndex,
                             QMap<QString,double>* anchor,QString* error){
    if(!anchor||shape.IsNull()||faceIndex<=0){fail(error,"Invalid face selection.");return false;}
    int current=0;
    for(TopExp_Explorer e(shape,TopAbs_FACE);e.More();e.Next()){
        if(++current!=faceIndex)continue;
        if(faceDescriptor(shape,TopoDS::Face(e.Current()),anchor))return true;
        fail(error,"Stable anchor capture requires a planar face.");return false;
    }
    fail(error,"Face index is not part of the selected solid.");return false;
}
bool resolvePlanarFaceAnchor(const TopoDS_Shape& body,const QMap<QString,double>& anchor,
                             TopoDS_Face* face,QString* error){
    if(!face||body.IsNull()||!anchor.contains("face_anchor_x")){
        fail(error,"Missing saved planar face anchor or output.");return false;
    }
    ShapeFeatureNode node;node.parameters=anchor;
    node.parameters.insert("face_index",1.0);
    TopoDS_Face matched;
    if(!findFace(body,node,&matched,error))return false;
    *face=matched;return true;
}
bool rebuildShapeFeatureTree(const TopoDS_Shape& source,bool surface,
                             const QVector<ShapeFeatureNode>& nodes,TopoDS_Shape* output,QString* error,
                             const QVector<TopoDS_Shape>* cachedPrefix,
                             QVector<TopoDS_Shape>* perNodeOutputs,
                             const QMap<QString,TopoDS_Shape>* reusableOutputs,
                             std::shared_ptr<BRepBuilderAPI_MakeShape>* kernelHistory){
    if(!output){fail(error,"Missing feature rebuild output.");return false;}
    if(kernelHistory&&nodes.size()!=1){
        fail(error,"Recording kernel history requires exactly one feature stage.");return false;
    }
    if(!validateShapeFeatureNodes(nodes,error)||!geometryOkay(source,surface,error))return false;
    const int skip=cachedPrefix?int(cachedPrefix->size()):0;
    if(skip>nodes.size()){
        fail(error,"Cached feature prefix exceeds the feature tree.");return false;
    }
    try{
        QMap<QString,TopoDS_Shape> outputs;
        outputs.insert("@base",source);
        QVector<TopoDS_Shape> intermediate;
        intermediate.reserve(nodes.size());
        std::shared_ptr<BRepBuilderAPI_MakeShape> producedHistory;
        TopoDS_Shape previous=source;
        for(int i=0;i<skip;++i){
            const TopoDS_Shape& saved=cachedPrefix->at(i);
            if(!geometryOkay(saved,surface,error)){
                fail(error,"The cached feature prefix is invalid; rebuild from the base.");return false;
            }
            outputs.insert(nodes[i].id,saved);
            intermediate.append(saved);
            previous=saved;
        }
        for(int i=skip;i<nodes.size();++i){
            const auto& op=nodes[i];
            if(reusableOutputs&&reusableOutputs->contains(op.id)){
                // Only SceneDocument, which owns and validates the old cache,
                // should authorize non-prefix reuse. Check geometry even then.
                const TopoDS_Shape saved=reusableOutputs->value(op.id);
                if(!geometryOkay(saved,surface,error)){
                    fail(error,"A dependency-reused geometry cache is invalid; rebuild from the base.");
                    return false;
                }
                outputs.insert(op.id,saved);
                intermediate.append(saved);
                previous=saved;
                continue;
            }
            const TopoDS_Shape input=op.inputs.isEmpty()?previous:outputs.value(op.inputs.first());
            const bool combine=op.type=="Fuse branches"||op.type=="Subtract branches"||op.type=="Intersect branches";
            TopoDS_Shape next;
            if(op.suppressed){next=input;}
            else if(combine){
                if(surface){fail(error,"Boolean branch combining requires solid bodies.");return false;}
                const TopoDS_Shape tool=outputs.value(op.inputs[1]);
                if(op.type=="Fuse branches"){
                    BRepAlgoAPI_Fuse fuse(input,tool);fuse.Build();
                    if(!fuse.IsDone()){fail(error,"Branch fusion failed.");return false;}
                    next=fuse.Shape();
                }else if(op.type=="Subtract branches"){
                    BRepAlgoAPI_Cut cut(input,tool);cut.Build();
                    if(!cut.IsDone()||cut.HasErrors()){fail(error,"Branch subtraction failed.");return false;}
                    next=cut.Shape();
                    GProp_GProps before,after;BRepGProp::VolumeProperties(input,before);BRepGProp::VolumeProperties(next,after);
                    if(after.Mass()>=before.Mass()-std::max(1e-7,before.Mass()*1e-9)){
                        fail(error,"Subtract branches did not remove material.");return false;
                    }
                }else{
                    BRepAlgoAPI_Common common(input,tool);common.Build();
                    if(!common.IsDone()||common.HasErrors()){
                        fail(error,"Branch intersection failed.");return false;
                    }
                    next=common.Shape(); // geometryOkay rejects an empty overlap
                }
            }else{
                const auto v=[&](const char* key){return op.parameters.value(QString::fromLatin1(key));};
                if(op.type=="Translate"){
                    gp_Trsf tr;tr.SetTranslation(gp_Vec(v("x"),v("y"),v("z")));
                    next=BRepBuilderAPI_Transform(input,tr,true).Shape();
                }else if(op.type.startsWith("Rotate ")){
                    const bool custom=op.type=="Rotate arbitrary axis";
                    const gp_Dir axis=custom?gp_Dir(v("axis_x"),v("axis_y"),v("axis_z")):
                        gp_Dir(op.type=="Rotate X"?1:0,op.type=="Rotate Y"?1:0,op.type=="Rotate Z"?1:0);
                    const gp_Pnt pivot=custom?gp_Pnt(v("pivot_x"),v("pivot_y"),v("pivot_z")):gp_Pnt(0,0,0);
                    gp_Trsf tr;tr.SetRotation(gp_Ax1(pivot,axis),v("degrees")*std::acos(-1.0)/180.0);
                    next=BRepBuilderAPI_Transform(input,tr,true).Shape();
                }else if(op.type=="Uniform scale"){
                    gp_Trsf tr;tr.SetScale(gp_Pnt(0,0,0),v("factor"));
                    next=BRepBuilderAPI_Transform(input,tr,true).Shape();
                }else if(op.type.startsWith("Mirror ")){
                    gp_Dir normal(0,0,1);
                    gp_Pnt origin(0,0,0);
                    if(op.type=="Mirror XZ")normal=gp_Dir(0,1,0);
                    if(op.type=="Mirror YZ")normal=gp_Dir(1,0,0);
                    if(op.type=="Mirror arbitrary plane"){
                        normal=gp_Dir(v("normal_x"),v("normal_y"),v("normal_z"));
                        origin=gp_Pnt(v("origin_x"),v("origin_y"),v("origin_z"));
                    }
                    gp_Trsf reflection;reflection.SetMirror(gp_Ax2(origin,normal));
                    next=BRepBuilderAPI_Transform(input,reflection,true).Shape();
                }else if(op.type.startsWith("Linear pattern ")||op.type=="Circular pattern Z"||
                         op.type=="Circular pattern arbitrary axis"){
                    // The pattern is a B-rep compound of copies (not a fused solid).
                    // Its body identity stays stable through save/reopen and undo.
                    const int count=static_cast<int>(v("count"));
                    BRep_Builder builder;TopoDS_Compound compound;builder.MakeCompound(compound);
                    for(int copy=0;copy<count;++copy){
                        gp_Trsf position;
                        if(op.type=="Circular pattern Z"||op.type=="Circular pattern arbitrary axis"){
                            const double sweep=v("sweep_degrees");
                            const double increment=sweep>=360.0-1e-8?sweep/count:sweep/(count-1);
                            const bool custom=op.type=="Circular pattern arbitrary axis";
                            const gp_Pnt center=custom?gp_Pnt(v("pivot_x"),v("pivot_y"),v("pivot_z")):gp_Pnt(0,0,0);
                            const gp_Dir axis=custom?gp_Dir(v("axis_x"),v("axis_y"),v("axis_z")):gp_Dir(0,0,1);
                            position.SetRotation(gp_Ax1(center,axis),
                                increment*copy*std::acos(-1.0)/180.0);
                        }else if(op.type=="Linear pattern vector"){
                            position.SetTranslation(gp_Vec(copy*v("dx"),copy*v("dy"),copy*v("dz")));
                        }else{
                            const double delta=copy*v("spacing");
                            position.SetTranslation(gp_Vec(op.type=="Linear pattern X"?delta:0,
                                op.type=="Linear pattern Y"?delta:0,
                                op.type=="Linear pattern Z"?delta:0));
                        }
                        const TopoDS_Shape instance=copy==0?input:
                            BRepBuilderAPI_Transform(input,position,true).Shape();
                        builder.Add(compound,instance);
                    }
                    next=compound;
                }else if(op.type=="Sketch polygon sweep 3D"||
                         op.type=="Sketch polygon sweep spline 3D"||
                         op.type=="Sketch multi-section loft Z"||
                         op.type=="Sketch polygon revolve axis"){
                    if(surface){fail(error,"Advanced sketch forms require a solid body.");return false;}
                    const int n=static_cast<int>(v("profile_count"));
                    const bool spline=op.type=="Sketch polygon sweep spline 3D";
                    const bool sweep=op.type=="Sketch polygon sweep 3D"||spline;
                    const bool loft=op.type=="Sketch multi-section loft Z";
                    const auto point3=[&](int i){const QString p="t"+QString::number(i);
                        return gp_Pnt(op.parameters.value(p+"_x"),
                                      op.parameters.value(p+"_y"),
                                      op.parameters.value(p+"_z"));
                    };
                    const auto profile2D=polygonPoints(op.parameters,n,"p");
                    TopoDS_Shape addition;
                    if(sweep){
                        const int m=static_cast<int>(v("path_count"));
                        TopoDS_Wire spine;
                        gp_Pnt start=point3(0);
                        gp_Vec tangent(start,point3(1));
                        if(spline){
                            Handle(NCollection_HArray1<gp_Pnt>) stations=new NCollection_HArray1<gp_Pnt>(1,m);
                            for(int j=0;j<m;++j)stations->SetValue(j+1,point3(j));
                            GeomAPI_Interpolate interpolation(stations,false,1e-6);
                            interpolation.Perform();
                            if(!interpolation.IsDone()){
                                fail(error,"3D spline guide interpolation failed.");return false;
                            }
                            const Handle(Geom_BSplineCurve) curve=interpolation.Curve();
                            curve->D1(curve->FirstParameter(),start,tangent);
                            if(tangent.SquareMagnitude()<1e-12){
                                fail(error,"3D spline guide has a zero initial tangent.");return false;
                            }
                            spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(curve).Edge()).Wire();
                        }else{
                            BRepBuilderAPI_MakePolygon path;
                            for(int j=0;j<m;++j)path.Add(point3(j));
                            spine=path.Wire();
                        }
                        tangent.Normalize();
                        gp_Vec reference=std::abs(tangent.Z())<0.9?gp_Vec(0,0,1):gp_Vec(0,1,0);
                        gp_Vec ux=reference.Crossed(tangent);ux.Normalize();
                        gp_Vec uy=tangent.Crossed(ux);uy.Normalize();
                        BRepBuilderAPI_MakePolygon section;
                        for(const auto& pt:profile2D){
                            gp_Pnt position=start;
                            position.Translate(ux.Multiplied(pt.x)+uy.Multiplied(pt.y));
                            section.Add(position);
                        }
                        section.Close();
                        BRepOffsetAPI_MakePipeShell pipe(spine);
                        pipe.Add(section.Wire());
                        pipe.Build();
                        if(!pipe.IsDone()||!pipe.MakeSolid()){
                            fail(error,"3D sketch-path sweep did not form a solid; inspect bends and profile size.");return false;
                        }
                        addition=pipe.Shape();
                    }else if(loft){
                        const int m=static_cast<int>(v("section_count"));
                        BRepOffsetAPI_ThruSections maker(true,false);
                        for(int iSection=0;iSection<m;++iSection){
                            const QString prefix="s"+QString::number(iSection);
                            const double elevation=op.parameters.value(prefix+"_z");
                            BRepBuilderAPI_MakePolygon section;
                            for(int j=0;j<n;++j){const QString p=prefix+"_p"+QString::number(j);
                                section.Add(gp_Pnt(op.parameters.value(p+"_x"),
                                                   op.parameters.value(p+"_y"),elevation));
                            }
                            section.Close();maker.AddWire(section.Wire());
                        }
                        maker.Build();
                        if(!maker.IsDone()){
                            fail(error,"Multi-section loft failed; check orientation and cross-section compatibility.");return false;
                        }
                        addition=maker.Shape();
                    }else{
                        const gp_Pnt center(v("origin_x"),v("origin_y"),v("origin_z"));
                        const gp_Dir axis(v("axis_x"),v("axis_y"),v("axis_z"));
                        const gp_Vec along(axis);
                        const gp_Vec reference=std::abs(axis.Z())<0.9?gp_Vec(0,0,1):gp_Vec(0,1,0);
                        gp_Vec radial=reference.Crossed(along);radial.Normalize();
                        BRepBuilderAPI_MakePolygon section;
                        for(const auto& pt:profile2D){
                            gp_Pnt position=center;
                            position.Translate(radial.Multiplied(pt.x)+along.Multiplied(pt.y));
                            section.Add(position);
                        }
                        section.Close();
                        BRepPrimAPI_MakeRevol maker(BRepBuilderAPI_MakeFace(section.Wire()).Face(),
                            gp_Ax1(center,axis),v("sweep_degrees")*std::acos(-1.0)/180.0,true);
                        maker.Build();
                        if(!maker.IsDone()){
                            fail(error,"Custom-axis sketch revolve failed.");return false;
                        }
                        addition=maker.Shape();
                    }
                    if(!geometryOkay(addition,false,error))return false;
                    BRepAlgoAPI_Fuse fused(input,addition);fused.Build();
                    if(!fused.IsDone()||fused.HasErrors()){
                        fail(error,"Advanced sketch additive Boolean failed.");return false;
                    }
                    next=fused.Shape();
                }else if(op.type=="Sketch polygon face boss"||op.type=="Sketch polygon face pocket"){
                    if(surface){fail(error,"Face-attached sketch extrusion requires a solid body.");return false;}
                    TopoDS_Face chosen;
                    if(!findFace(input,op,&chosen,error))return false;
                    const BRepAdaptor_Surface plane(chosen);
                    if(plane.GetType()!=GeomAbs_Plane){
                        fail(error,"Attached sketch face is no longer planar.");return false;
                    }
                    GProp_GProps faceProps,before,after;
                    BRepGProp::SurfaceProperties(chosen,faceProps);
                    BRepGProp::VolumeProperties(input,before);
                    gp_Vec normal(plane.Plane().Axis().Direction());
                    if(normal.Dot(gp_Vec(before.CentreOfMass(),faceProps.CentreOfMass()))<0)
                        normal.Reverse();
                    // Reproduce the established face-local frame used by existing
                    // circle/rectangle attachments: offsets and polygon vertices
                    // are relative to the face surface centroid, in millimetres.
                    gp_Vec x=std::abs(normal.X())<0.9?gp_Vec(1,0,0):gp_Vec(0,1,0);
                    x-=normal.Multiplied(x.Dot(normal));x.Normalize();
                    const gp_Vec y=normal.Crossed(x);
                    gp_Pnt origin=faceProps.CentreOfMass();
                    origin.Translate(x.Multiplied(v("u"))+y.Multiplied(v("v")));
                    const bool pocket=op.type=="Sketch polygon face pocket";
                    const double overlap=std::min(0.01,v("depth")*0.05);
                    origin.Translate(normal.Multiplied(pocket?overlap:-overlap));
                    BRepBuilderAPI_MakePolygon section;
                    for(const auto& vertex:polygonPoints(op.parameters,static_cast<int>(v("profile_count")),"p")){
                        gp_Pnt p=origin;
                        p.Translate(x.Multiplied(vertex.x)+y.Multiplied(vertex.y));
                        section.Add(p);
                    }
                    section.Close();
                    BRepPrimAPI_MakePrism extrusion(BRepBuilderAPI_MakeFace(section.Wire()).Face(),
                        normal.Multiplied(pocket?-(v("depth")+overlap):v("depth")+overlap),true,true);
                    extrusion.Build();
                    if(!extrusion.IsDone()||!geometryOkay(extrusion.Shape(),false,error))return false;
                    if(pocket){
                        auto cut=std::make_shared<BRepAlgoAPI_Cut>(input,extrusion.Shape());cut->Build();
                        if(!cut->IsDone()||cut->HasErrors()){
                            fail(error,"Face-attached polygon pocket failed.");return false;
                        }
                        next=cut->Shape();producedHistory=cut;
                    }else{
                        auto fused=std::make_shared<BRepAlgoAPI_Fuse>(input,extrusion.Shape());fused->Build();
                        if(!fused->IsDone()||fused->HasErrors()){
                            fail(error,"Face-attached polygon boss failed.");return false;
                        }
                        next=fused->Shape();producedHistory=fused;
                    }
                    BRepGProp::VolumeProperties(next,after);
                    if(!pocket){
                        // A face-attached boss must not become a floating,
                        // disconnected solid when the local sketch is moved
                        // off the selected face. Support legitimate fusions
                        // that *reduce* the count of initially separate solids.
                        int sourceSolids=0,resultSolids=0;
                        for(TopExp_Explorer e(input,TopAbs_SOLID);e.More();e.Next())++sourceSolids;
                        for(TopExp_Explorer e(next,TopAbs_SOLID);e.More();e.Next())++resultSolids;
                        if(resultSolids>sourceSolids){
                            fail(error,"Face-attached boss is disconnected from its target solid.");return false;
                        }
                    }
                    const double tolerance=std::max(1e-7,before.Mass()*1e-9);
                    if(pocket?after.Mass()>=before.Mass()-tolerance:
                              after.Mass()<=before.Mass()+tolerance){
                        fail(error,"Face-attached polygon did not change solid volume.");return false;
                    }
                }else if(polygonFeature(op.type)){
                    if(surface){fail(error,"Sketch profile solid features require a solid body.");return false;}
                    const int n=static_cast<int>(v("profile_count"));
                    const auto lower=polygonPoints(op.parameters,n,"p");
                    const bool loft=op.type=="Sketch polygons loft Z";
                    const bool revolve=op.type=="Sketch polygon revolve Z";
                    const bool pocket=op.type=="Sketch polygon pocket Z";
                    const double z=op.parameters.value("base_z",0.0);
                    const double height=op.parameters.value("height",0.0);
                    const auto closedWire=[](const std::vector<polygon::Point>& points,double elevation,bool radial){
                        BRepBuilderAPI_MakePolygon wire;
                        for(const auto& p:points)
                            wire.Add(radial?gp_Pnt(p.x,0,p.y):gp_Pnt(p.x,p.y,elevation));
                        wire.Close();return wire.Wire();
                    };
                    TopoDS_Shape tool;
                    if(revolve){
                        BRepPrimAPI_MakeRevol maker(BRepBuilderAPI_MakeFace(closedWire(lower,0,true)).Face(),
                            gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),2*std::acos(-1.0),true);
                        maker.Build();
                        if(!maker.IsDone()){fail(error,"Sketch polygon revolve failed.");return false;}
                        tool=maker.Shape();
                    }else if(loft){
                        BRepOffsetAPI_ThruSections maker(true,true);
                        maker.AddWire(closedWire(lower,z,false));
                        maker.AddWire(closedWire(polygonPoints(op.parameters,n,"q"),z+height,false));
                        maker.Build();
                        if(!maker.IsDone()){fail(error,"Sketch polygon loft failed.");return false;}
                        tool=maker.Shape();
                    }else{
                        BRepPrimAPI_MakePrism maker(BRepBuilderAPI_MakeFace(closedWire(lower,z,false)).Face(),
                            gp_Vec(0,0,height),true,true);
                        maker.Build();
                        if(!maker.IsDone()){fail(error,"Sketch polygon extrusion failed.");return false;}
                        tool=maker.Shape();
                    }
                    if(!geometryOkay(tool,false,error))return false;
                    if(pocket){
                        BRepAlgoAPI_Cut operation(input,tool);operation.Build();
                        if(!operation.IsDone()||operation.HasErrors()){
                            fail(error,"Sketch polygon pocket Boolean failed.");return false;
                        }
                        next=operation.Shape();
                        GProp_GProps before,after;
                        BRepGProp::VolumeProperties(input,before);
                        BRepGProp::VolumeProperties(next,after);
                        if(after.Mass()>=before.Mass()-std::max(1e-7,before.Mass()*1e-9)){
                            fail(error,"Sketch polygon pocket does not remove material.");return false;
                        }
                    }else{
                        BRepAlgoAPI_Fuse operation(input,tool);operation.Build();
                        if(!operation.IsDone()||operation.HasErrors()){
                            fail(error,"Sketch polygon additive fusion failed.");return false;
                        }
                        next=operation.Shape();
                    }
                }else if(op.type=="Revolve annulus Z"||op.type=="Sweep circle path"||
                         op.type=="Loft rectangles"){
                    if(surface){fail(error,"Additive form features require a solid body.");return false;}
                    TopoDS_Shape addition;
                    if(op.type=="Revolve annulus Z"){
                        if(v("outer_radius")<=v("inner_radius")){fail(error,"Outer revolve radius must exceed inner radius.");return false;}
                        BRepBuilderAPI_MakePolygon polygon;
                        polygon.Add(gp_Pnt(v("inner_radius"),0,0));
                        polygon.Add(gp_Pnt(v("outer_radius"),0,0));
                        polygon.Add(gp_Pnt(v("outer_radius"),0,v("height")));
                        polygon.Add(gp_Pnt(v("inner_radius"),0,v("height")));polygon.Close();
                        BRepPrimAPI_MakeRevol maker(BRepBuilderAPI_MakeFace(polygon.Wire()).Face(),
                            gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),2*std::acos(-1.0),true);
                        maker.Build();if(!maker.IsDone()){fail(error,"Revolve profile failed.");return false;}
                        addition=maker.Shape();
                    }else if(op.type=="Loft rectangles"){
                        const auto section=[](double x,double y,double w,double d,double z){
                            BRepBuilderAPI_MakePolygon p;p.Add(gp_Pnt(x,y,z));p.Add(gp_Pnt(x+w,y,z));
                            p.Add(gp_Pnt(x+w,y+d,z));p.Add(gp_Pnt(x,y+d,z));p.Close();return p.Wire();
                        };
                        BRepOffsetAPI_ThruSections loft(true,true);
                        loft.AddWire(section(0,0,v("width0"),v("depth0"),0));
                        loft.AddWire(section(v("offset_x"),v("offset_y"),v("width1"),v("depth1"),v("height")));
                        loft.Build();if(!loft.IsDone()){fail(error,"Rectangular section loft failed.");return false;}
                        addition=loft.Shape();
                    }else{
                        const gp_Pnt start(0,0,0),mid(v("path_length")*0.5,v("lateral_offset")*0.35,
                            v("vertical_rise")*0.5),end(v("path_length"),v("lateral_offset"),v("vertical_rise"));
                        Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,3);
                        points->SetValue(1,start);points->SetValue(2,mid);points->SetValue(3,end);
                        GeomAPI_Interpolate interp(points,false,1e-6);interp.Perform();
                        if(!interp.IsDone()){fail(error,"Could not interpolate sweep spine.");return false;}
                        const Handle(Geom_BSplineCurve) centerline=interp.Curve();
                        gp_Pnt onPath;gp_Vec tangent;
                        centerline->D1(centerline->FirstParameter(),onPath,tangent);
                        if(tangent.SquareMagnitude()<1e-14){fail(error,"Sweep spine has no initial tangent.");return false;}
                        tangent.Normalize();gp_Vec reference(0,0,1);
                        reference-=tangent*tangent.Dot(reference);
                        if(reference.SquareMagnitude()<1e-8){reference=gp_Vec(0,1,0);reference-=tangent*tangent.Dot(reference);}
                        const gp_Ax2 frame(start,gp_Dir(tangent),gp_Dir(reference));
                        const TopoDS_Wire profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,v("radius"))).Edge()).Wire();
                        const TopoDS_Wire spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(centerline).Edge()).Wire();
                        BRepOffsetAPI_MakePipeShell pipe(spine);pipe.Add(profile);pipe.Build();
                        if(!pipe.IsDone()||!pipe.MakeSolid()){fail(error,"Sweep pipe failed to form a solid.");return false;}
                        addition=pipe.Shape();
                    }
                    if(!geometryOkay(addition,false,error))return false;
                    auto fuse=std::make_shared<BRepAlgoAPI_Fuse>(input,addition);fuse->Build();
                    if(!fuse->IsDone()||fuse->HasErrors()){fail(error,"Additive feature fusion failed.");return false;}
                    next=fuse->Shape();
                    if(op.type=="Loft rectangles"||op.type=="Sweep circle path")
                        producedHistory=fuse;
                }else if(op.type=="Cylinder cut Z"||op.type=="Cylinder cut X"||op.type=="Cylinder cut Y"||
                         op.type=="Cylinder cut arbitrary axis"||
                         op.type=="Cylinder through all arbitrary axis"){
                    if(surface){fail(error,"Cylinder cuts require a solid.");return false;}
                    const gp_Dir direction=op.type=="Cylinder cut X"?gp_Dir(1,0,0):
                        op.type=="Cylinder cut Y"?gp_Dir(0,1,0):
                        (op.type=="Cylinder cut arbitrary axis"||
                         op.type=="Cylinder through all arbitrary axis")?gp_Dir(v("axis_x"),v("axis_y"),v("axis_z")):
                        gp_Dir(0,0,1);
                    // A through-all feature must be regenerated against the *current*
                    // input solid after upstream dimensional edits, including
                    // rotations, so no fixed depth is stored in its node.
                    gp_Pnt cutterOrigin(v("x"),v("y"),v("z"));
                    double cutterDepth=0.0;
                    if(op.type=="Cylinder through all arbitrary axis"){
                        Bnd_Box bounds;BRepBndLib::Add(input,bounds);
                        if(bounds.IsVoid()){
                            fail(error,"Through-all cut has no measurable solid bounds.");return false;
                        }
                        cad::directional_holes::Bounds box;
                        bounds.Get(box.xmin,box.ymin,box.zmin,box.xmax,box.ymax,box.zmax);
                        cad::directional_holes::Span span;
                        if(!cad::directional_holes::throughAllSpan(box,
                             {cutterOrigin.X(),cutterOrigin.Y(),cutterOrigin.Z()},
                             {direction.X(),direction.Y(),direction.Z()},&span)){
                            fail(error,"Through-all cutter extent is invalid for the current solid.");return false;
                        }
                        cutterOrigin.Translate(gp_Vec(direction).Multiplied(span.start));
                        cutterDepth=span.length;
                    }else cutterDepth=v("depth");
                    const TopoDS_Shape tool=BRepPrimAPI_MakeCylinder(
                        gp_Ax2(cutterOrigin,direction),v("radius"),cutterDepth).Shape();
                    auto cut=std::make_shared<BRepAlgoAPI_Cut>(input,tool);cut->Build();
                    if(!cut->IsDone()||cut->HasErrors()){fail(error,"Cylinder cut failed.");return false;}
                    next=cut->Shape();producedHistory=cut;
                    GProp_GProps before,after;BRepGProp::VolumeProperties(input,before);BRepGProp::VolumeProperties(next,after);
                    if(after.Mass()>=before.Mass()-std::max(1e-7,before.Mass()*1e-9)){
                        fail(error,"Cylinder cut does not remove material; check position and depth.");return false;
                    }
                }else if(op.type=="Face through-all hole"||
                         op.type=="Face counterbore through all"||
                         op.type=="Face countersink through all"||
                         op.type.startsWith("Face slot ")||
                         op.type=="Face hole grid through all"||
                         op.type=="Face bolt circle through all"||
                         op.type=="Face normal pocket"||op.type=="Face normal boss"||
                         op.type=="Face rectangle pocket"||op.type=="Face rectangle boss"){
                    if(surface){fail(error,"Face-based solid operations require a solid.");return false;}
                    TopoDS_Face chosen;
                    if(!findFace(input,op,&chosen,error))return false;
                    const BRepAdaptor_Surface adaptor(chosen);
                    if(adaptor.GetType()!=GeomAbs_Plane){fail(error,"Referenced face is no longer planar.");return false;}
                    GProp_GProps faceProps,mass;
                    BRepGProp::SurfaceProperties(chosen,faceProps);
                    BRepGProp::VolumeProperties(input,mass);
                    gp_Vec outward(adaptor.Plane().Axis().Direction());
                    const gp_Vec away(mass.CentreOfMass(),faceProps.CentreOfMass());
                    if(outward.Dot(away)<0)outward.Reverse();
                    gp_Vec x=std::abs(outward.X())<0.9?gp_Vec(1,0,0):gp_Vec(0,1,0);
                    x-=outward.Multiplied(x.Dot(outward));x.Normalize();
                    const gp_Vec y=outward.Crossed(x);
                    gp_Pnt center=witnessedMachiningOrigin(input,chosen,op,
                        faceProps.CentreOfMass());
                    center.Translate(x.Multiplied(v("u"))+y.Multiplied(v("v")));
                    // A stable support face may split or merge. Do not silently
                    // move a witnessed feature to a different trimmed region.
                    // Both raw and translated face witnesses are checked by
                    // findFace before this machining location is considered.
                    if(op.parameters.contains("face_anchor_witness_x")&&
                       !witnessOnFace(chosen,center,1e-6)){
                        fail(error,"Machining center left the witnessed trimmed face; reselect a valid face or position.");
                        return false;
                    }
                    const bool holePattern=op.type=="Face hole grid through all"||
                        op.type=="Face bolt circle through all";
                    const bool slot=op.type.startsWith("Face slot ");
                    if(holePattern){
                        std::vector<cad::hole_patterns::Point2> stations;
                        std::string issue;
                        const bool generated=op.type=="Face hole grid through all"?
                            cad::hole_patterns::grid(v("u"),v("v"),v("radius"),
                                static_cast<int>(v("columns")),static_cast<int>(v("rows")),
                                v("pitch_u"),v("pitch_v"),v("rotation_degrees"),&stations,&issue):
                            cad::hole_patterns::boltCircle(v("u"),v("v"),v("radius"),
                                static_cast<int>(v("count")),v("pcd"),
                                v("start_angle_degrees"),&stations,&issue);
                        if(!generated||stations.empty()){
                            fail(error,QString::fromStdString(issue));return false;
                        }
                        Bnd_Box bounds;BRepBndLib::Add(input,bounds);
                        if(bounds.IsVoid()){
                            fail(error,"Hole pattern needs finite solid bounds.");return false;
                        }
                        cad::directional_holes::Bounds box;
                        bounds.Get(box.xmin,box.ymin,box.zmin,box.xmax,box.ymax,box.zmax);
                        const gp_Dir inward(outward.Reversed());
                        BRep_Builder builder;
                        TopoDS_Compound cutters;
                        builder.MakeCompound(cutters);
                        for(const auto& local:stations){
                            gp_Pnt entry=witnessedMachiningOrigin(input,chosen,op,
                                faceProps.CentreOfMass());
                            entry.Translate(x.Multiplied(local.u)+y.Multiplied(local.v));
                            // An entire bore must fit on the selected trimmed
                            // face. Do not silently accept clipped/missing
                            // stations after a plate sketch is edited.
                            if(!witnessOnFace(chosen,entry,1e-6)){
                                fail(error,"A hole-pattern center has left the selected face.");return false;
                            }
                            for(int sample=0;sample<16;++sample){
                                const double a=2*std::numbers::pi*sample/16;
                                gp_Pnt edge=entry;
                                edge.Translate(x.Multiplied(v("radius")*std::cos(a))+
                                               y.Multiplied(v("radius")*std::sin(a)));
                                if(!witnessOnFace(chosen,edge,1e-6)){
                                    fail(error,"A patterned hole crosses the selected face boundary.");return false;
                                }
                            }
                            cad::directional_holes::Span span;
                            if(!cad::directional_holes::throughAllSpan(box,
                                   {entry.X(),entry.Y(),entry.Z()},
                                   {inward.X(),inward.Y(),inward.Z()},&span)){
                                fail(error,"Pattern cutter cannot span the current solid.");return false;
                            }
                            entry.Translate(gp_Vec(inward).Multiplied(span.start));
                            const TopoDS_Shape cutter=BRepPrimAPI_MakeCylinder(
                                gp_Ax2(entry,inward),v("radius"),span.length).Shape();
                            builder.Add(cutters,cutter);
                        }
                        auto cut=std::make_shared<BRepAlgoAPI_Cut>(input,cutters);
                        cut->Build();
                        if(!cut->IsDone()||cut->HasErrors()){
                            fail(error,"Hole-pattern Boolean operation failed.");return false;
                        }
                        next=cut->Shape();producedHistory=cut;
                        GProp_GProps after;BRepGProp::VolumeProperties(next,after);
                        if(after.Mass()>=mass.Mass()-std::max(1e-7,mass.Mass()*1e-9)){
                            fail(error,"Hole pattern removes no material.");return false;
                        }
                    }else if(slot){
                        const gp_Dir inward(outward.Reversed());
                        const double angle=v("rotation_degrees")*std::numbers::pi/180.0;
                        const gp_Vec along=x.Multiplied(std::cos(angle))+
                            y.Multiplied(std::sin(angle));
                        const double spacing=v("center_spacing");
                        const double radius=v("radius");
                        const bool through=op.type=="Face slot through all";
                        gp_Pnt start=center;
                        double cutDepth=0;
                        if(through){
                            Bnd_Box bounds;BRepBndLib::Add(input,bounds);
                            if(bounds.IsVoid()){
                                fail(error,"Slot has no finite body bounds.");return false;
                            }
                            cad::directional_holes::Bounds box;
                            bounds.Get(box.xmin,box.ymin,box.zmin,box.xmax,box.ymax,box.zmax);
                            cad::directional_holes::Span span;
                            if(!cad::directional_holes::throughAllSpan(box,
                                    {center.X(),center.Y(),center.Z()},
                                    {inward.X(),inward.Y(),inward.Z()},&span)){
                                fail(error,"Slot cannot span the current body.");return false;
                            }
                            start.Translate(gp_Vec(inward).Multiplied(span.start));
                            cutDepth=span.length;
                        }else{
                            const double overlap=std::min(0.01,v("depth")*0.05);
                            start.Translate(outward.Multiplied(overlap));
                            cutDepth=v("depth")+overlap;
                        }
                        gp_Pnt capA=start,capB=start;
                        capA.Translate(along.Multiplied(-spacing*0.5));
                        capB.Translate(along.Multiplied(spacing*0.5));
                        const gp_Vec across=gp_Vec(inward).Crossed(along);
                        gp_Pnt corner=capA;
                        corner.Translate(across.Multiplied(-radius));
                        const TopoDS_Shape first=BRepPrimAPI_MakeCylinder(
                            gp_Ax2(capA,inward),radius,cutDepth).Shape();
                        const TopoDS_Shape second=BRepPrimAPI_MakeCylinder(
                            gp_Ax2(capB,inward),radius,cutDepth).Shape();
                        const TopoDS_Shape bridge=BRepPrimAPI_MakeBox(
                            gp_Ax2(corner,inward,gp_Dir(along)),spacing,2*radius,cutDepth).Shape();
                        BRepAlgoAPI_Fuse initial(first,bridge);initial.Build();
                        if(!initial.IsDone()||initial.HasErrors()){
                            fail(error,"Slot cutter bridge fusion failed.");return false;
                        }
                        BRepAlgoAPI_Fuse combined(initial.Shape(),second);combined.Build();
                        if(!combined.IsDone()||combined.HasErrors()||
                           !BRepCheck_Analyzer(combined.Shape()).IsValid()){
                            fail(error,"Slot cutter end-cap fusion failed.");return false;
                        }
                        auto cut=std::make_shared<BRepAlgoAPI_Cut>(input,combined.Shape());
                        cut->Build();
                        if(!cut->IsDone()||cut->HasErrors()){
                            fail(error,"Face obround slot Boolean failed.");return false;
                        }
                        next=cut->Shape();producedHistory=cut;
                        GProp_GProps after;
                        BRepGProp::VolumeProperties(next,after);
                        if(after.Mass()>=mass.Mass()-std::max(1e-7,mass.Mass()*1e-9)){
                            fail(error,"Face slot does not remove any stock.");return false;
                        }
                    }else{
                    const bool fastenerHole=op.type=="Face through-all hole"||
                        op.type=="Face counterbore through all"||
                        op.type=="Face countersink through all";
                    if(fastenerHole){
                        // A selected planar face owns the drill-entry frame. Its
                        // trimmed witness is resolved again after upstream edits;
                        // the cutter extent is recomputed from the current B-rep.
                        const gp_Dir inward(outward.Reversed());
                        Bnd_Box bounds;BRepBndLib::Add(input,bounds);
                        if(bounds.IsVoid()){
                            fail(error,"Face drilling requires finite solid bounds.");return false;
                        }
                        cad::directional_holes::Bounds box;
                        bounds.Get(box.xmin,box.ymin,box.zmin,box.xmax,box.ymax,box.zmax);
                        cad::directional_holes::Span span;
                        if(!cad::directional_holes::throughAllSpan(box,
                              {center.X(),center.Y(),center.Z()},
                              {inward.X(),inward.Y(),inward.Z()},&span)){
                            fail(error,"Face drilling cutter cannot span the current solid.");return false;
                        }
                        gp_Pnt boreStart=center;
                        boreStart.Translate(gp_Vec(inward).Multiplied(span.start));
                        TopoDS_Shape tool=BRepPrimAPI_MakeCylinder(
                            gp_Ax2(boreStart,inward),v("radius"),span.length).Shape();
                        if(op.type!="Face through-all hole"){
                            const double overlap=0.01;
                            gp_Pnt start=center;
                            start.Translate(outward.Multiplied(overlap));
                            TopoDS_Shape recess;
                            if(op.type=="Face counterbore through all"){
                                recess=BRepPrimAPI_MakeCylinder(
                                    gp_Ax2(start,inward),v("head_radius"),
                                    v("recess_depth")+overlap).Shape();
                            }else{
                                const double halfRadians=v("included_angle")*std::numbers::pi/360.0;
                                const double slope=std::tan(halfRadians);
                                const double sinkDepth=(v("head_radius")-v("radius"))/slope;
                                // Keep the *design* head radius on the stock
                                // surface, not on the outward overlap plane.
                                recess=BRepPrimAPI_MakeCone(gp_Ax2(start,inward),
                                    v("head_radius")+overlap*slope,v("radius"),
                                    sinkDepth+overlap).Shape();
                            }
                            // Demand a physically distinct head recess. No
                            // silent reduction to a plain through-hole.
                            BRepAlgoAPI_Cut boreOnly(input,tool);boreOnly.Build();
                            if(!boreOnly.IsDone()||boreOnly.HasErrors()){
                                fail(error,"Fastener pilot bore failed.");return false;
                            }
                            BRepAlgoAPI_Cut additional(boreOnly.Shape(),recess);additional.Build();
                            if(!additional.IsDone()||additional.HasErrors()){
                                fail(error,"Fastener head recess failed.");return false;
                            }
                            GProp_GProps pilotProps,recessProps;
                            BRepGProp::VolumeProperties(boreOnly.Shape(),pilotProps);
                            BRepGProp::VolumeProperties(additional.Shape(),recessProps);
                            if(recessProps.Mass()>=pilotProps.Mass()-
                               std::max(1e-7,pilotProps.Mass()*1e-9)){
                                fail(error,"Head recess removes no additional stock; check drill entry and direction.");
                                return false;
                            }
                            // Build one final Boolean with the unioned cutter,
                            // retaining kernel history from original input to
                            // committed output for dependent topology mapping.
                            BRepAlgoAPI_Fuse combined(tool,recess);combined.Build();
                            if(!combined.IsDone()||combined.HasErrors()){
                                fail(error,"Failed to combine pilot bore and head recess.");return false;
                            }
                            tool=combined.Shape();
                        }
                        auto cut=std::make_shared<BRepAlgoAPI_Cut>(input,tool);cut->Build();
                        if(!cut->IsDone()||cut->HasErrors()){
                            fail(error,"Face fastener drilling failed.");return false;
                        }
                        next=cut->Shape();producedHistory=cut;
                        GProp_GProps after;BRepGProp::VolumeProperties(next,after);
                        if(after.Mass()>=mass.Mass()-std::max(1e-7,mass.Mass()*1e-9)){
                            fail(error,"Face through-hole does not intersect the solid.");return false;
                        }
                    }else{
                    const bool rectangle=op.type.startsWith("Face rectangle ");
                    const bool pocket=op.type.endsWith("pocket");
                    const double overlap=std::min(0.01,v("depth")*0.05);
                    center.Translate(outward.Multiplied(pocket?overlap:-overlap));
                    const gp_Vec direction=pocket?outward.Reversed():outward;
                    TopoDS_Shape tool;
                    if(rectangle){
                        // Right-handed local frame: the rectangle is centered on the
                        // selected face while the pocket extends into the material.
                        gp_Vec frameX=x;
                        gp_Vec frameY=direction.Crossed(frameX);
                        gp_Pnt corner=center;
                        corner.Translate(frameX.Multiplied(-v("width")*0.5)+
                            frameY.Multiplied(-v("height")*0.5));
                        tool=BRepPrimAPI_MakeBox(gp_Ax2(corner,gp_Dir(direction),gp_Dir(frameX)),
                            v("width"),v("height"),v("depth")+overlap).Shape();
                    }else{
                        tool=BRepPrimAPI_MakeCylinder(gp_Ax2(center,gp_Dir(direction)),
                            v("radius"),v("depth")+overlap).Shape();
                    }
                    if(pocket){
                        auto cut=std::make_shared<BRepAlgoAPI_Cut>(input,tool);cut->Build();
                        if(!cut->IsDone()||cut->HasErrors()){
                            fail(error,"Face-normal pocket cut failed.");return false;
                        }
                        next=cut->Shape();producedHistory=cut;
                    }else{
                        auto fuse=std::make_shared<BRepAlgoAPI_Fuse>(input,tool);fuse->Build();
                        if(!fuse->IsDone()||fuse->HasErrors()){
                            fail(error,"Face-normal boss fusion failed.");return false;
                        }
                        next=fuse->Shape();producedHistory=fuse;
                    }
                    GProp_GProps after;BRepGProp::VolumeProperties(next,after);
                    if(pocket?after.Mass()>=mass.Mass()-std::max(1e-7,mass.Mass()*1e-9):
                              after.Mass()<=mass.Mass()+std::max(1e-7,mass.Mass()*1e-9)){
                        fail(error,"Face-based feature has no material effect; verify face and offsets.");return false;
                    }
                    } // legacy blind face-pocket/boss branch
                    } // slot / legacy face machining
                }else if(op.type=="Open-face shell"){
                    if(surface){fail(error,"Open-face shell requires a solid body.");return false;}
                    TopoDS_Face opening;
                    if(!findFace(input,op,&opening,error))return false;
                    NCollection_List<TopoDS_Shape> openingFaces;openingFaces.Append(opening);
                    BRepOffsetAPI_MakeThickSolid shell;
                    shell.MakeThickSolidByJoin(input,openingFaces,-v("thickness"),1e-3,BRepOffset_Skin);
                    if(!shell.IsDone()){fail(error,"Shell failed; reduce thickness or choose another opening face.");return false;}
                    next=shell.Shape();
                    GProp_GProps a,b;BRepGProp::VolumeProperties(input,a);BRepGProp::VolumeProperties(next,b);
                    if(b.Mass()>=a.Mass()-1e-8){fail(error,"Shell operation did not remove material.");return false;}
                }else if(op.type=="Draft face Z"){
                    if(surface){fail(error,"Face draft requires a solid.");return false;}
                    TopoDS_Face face;
                    if(!findFace(input,op,&face,error))return false;
                    BRepOffsetAPI_DraftAngle draft(input);
                    draft.Add(face,gp_Dir(0,0,1),v("angle_degrees")*std::acos(-1.0)/180.0,
                              gp_Pln(gp_Pnt(0,0,v("neutral_z")),gp_Dir(0,0,1)),true);
                    if(!draft.AddDone()){fail(error,"Draft face cannot be drafted about this Z plane.");return false;}
                    draft.Build();
                    if(!draft.IsDone()){fail(error,"CAD kernel could not build the face draft.");return false;}
                    next=draft.Shape();
                }else if(op.type=="Fillet all edges"||op.type=="Fillet selected edge"||
                         op.type=="Fillet selected edges"){
                    if(surface){fail(error,"Edge fillets require a solid.");return false;}
                    auto fillet=std::make_shared<BRepFilletAPI_MakeFillet>(input);
                    TopTools_IndexedMapOfShape edges;TopExp::MapShapes(input,TopAbs_EDGE,edges);
                    if(!edges.Extent()){fail(error,"No filletable edges.");return false;}
                    if(op.type=="Fillet selected edge"){
                        int selected=static_cast<int>(v("edge_index"));
                        if(op.parameters.contains("edge_x")){
                            QMap<QString,double> anchor;
                            for(const QString& key:kEdgeAnchorFields)anchor.insert(key,op.parameters.value(key));
                            if(op.parameters.contains("edge_body_x"))
                                for(const QString& key:kEdgeBodyFields)anchor.insert(key,op.parameters.value(key));
                            if(op.parameters.contains("edge_anchor_version"))
                                for(const QString& key:kEdgeWitnessFields)anchor.insert(key,op.parameters.value(key));
                            if(!resolveStableEdgeAnchor(input,anchor,&selected,error))return false;
                        }
                        if(selected<1||selected>edges.Extent()){
                            fail(error,"Selected fillet edge is missing; reselect it.");return false;
                        }
                        fillet->Add(v("radius"),TopoDS::Edge(edges.FindKey(selected)));
                    }else if(op.type=="Fillet selected edges"){
                        const int count=static_cast<int>(v("edge_count"));
                        std::set<int> selectedIndices;
                        for(int j=0;j<count;++j){
                            const QString prefix=QStringLiteral("member_%1_").arg(j);
                            QMap<QString,double> anchor;
                            for(auto it=op.parameters.cbegin();it!=op.parameters.cend();++it)
                                if(it.key().startsWith(prefix))
                                    anchor.insert(it.key().mid(prefix.size()),it.value());
                            // Per-member machining size belongs to the blend operation,
                            // NOT to the persisted geometry witness. Passing it to the
                            // strict anchor resolver rejected every sized edge group.
                            const double memberSize=anchor.value("size",v("radius"));
                            int selected=static_cast<int>(anchor.value("edge_index"));
                            anchor.remove("edge_index");
                            anchor.remove("size");
                            if(!resolveStableEdgeAnchor(input,anchor,&selected,error))return false;
                            if(selected<1||selected>edges.Extent()||!selectedIndices.insert(selected).second){
                                fail(error,"Edge group members no longer refer to distinct edges. Reselect the group.");return false;
                            }
                            fillet->Add(memberSize,
                                        TopoDS::Edge(edges.FindKey(selected)));
                        }
                    }else for(int i=1;i<=edges.Extent();++i)
                        fillet->Add(v("radius"),TopoDS::Edge(edges.FindKey(i)));
                    fillet->Build();if(!fillet->IsDone()){fail(error,"Fillet failed at this radius.");return false;}
                    next=fillet->Shape();producedHistory=fillet;
                }else if(op.type=="Chamfer all edges"||op.type=="Chamfer selected edge"||
                         op.type=="Chamfer selected edges"){
                    if(surface){fail(error,"Edge chamfers require a solid.");return false;}
                    auto chamfer=std::make_shared<BRepFilletAPI_MakeChamfer>(input);
                    TopTools_IndexedMapOfShape edges;TopExp::MapShapes(input,TopAbs_EDGE,edges);
                    if(!edges.Extent()){fail(error,"No chamferable edges.");return false;}
                    if(op.type=="Chamfer selected edge"){
                        int selected=static_cast<int>(v("edge_index"));
                        if(op.parameters.contains("edge_x")){
                            QMap<QString,double> anchor;
                            for(const QString& key:kEdgeAnchorFields)anchor.insert(key,op.parameters.value(key));
                            if(op.parameters.contains("edge_body_x"))
                                for(const QString& key:kEdgeBodyFields)anchor.insert(key,op.parameters.value(key));
                            if(op.parameters.contains("edge_anchor_version"))
                                for(const QString& key:kEdgeWitnessFields)anchor.insert(key,op.parameters.value(key));
                            if(!resolveStableEdgeAnchor(input,anchor,&selected,error))return false;
                        }
                        if(selected<1||selected>edges.Extent()){
                            fail(error,"Selected chamfer edge is missing; reselect it.");return false;
                        }
                        chamfer->Add(v("distance"),TopoDS::Edge(edges.FindKey(selected)));
                    }else if(op.type=="Chamfer selected edges"){
                        const int count=static_cast<int>(v("edge_count"));
                        std::set<int> selectedIndices;
                        for(int j=0;j<count;++j){
                            const QString prefix=QStringLiteral("member_%1_").arg(j);
                            QMap<QString,double> anchor;
                            for(auto it=op.parameters.cbegin();it!=op.parameters.cend();++it)
                                if(it.key().startsWith(prefix))
                                    anchor.insert(it.key().mid(prefix.size()),it.value());
                            // Per-member machining size belongs to the blend operation,
                            // NOT to the persisted geometry witness. Passing it to the
                            // strict anchor resolver rejected every sized edge group.
                            const double memberSize=anchor.value("size",v("distance"));
                            int selected=static_cast<int>(anchor.value("edge_index"));
                            anchor.remove("edge_index");
                            anchor.remove("size");
                            if(!resolveStableEdgeAnchor(input,anchor,&selected,error))return false;
                            if(selected<1||selected>edges.Extent()||!selectedIndices.insert(selected).second){
                                fail(error,"Chamfer group members no longer refer to distinct edges. Reselect the group.");return false;
                            }
                            chamfer->Add(memberSize,
                                         TopoDS::Edge(edges.FindKey(selected)));
                        }
                    }else for(int i=1;i<=edges.Extent();++i)
                        chamfer->Add(v("distance"),TopoDS::Edge(edges.FindKey(i)));
                    chamfer->Build();if(!chamfer->IsDone()){
                        fail(error,"Chamfer failed at this distance.");return false;
                    }
                    next=chamfer->Shape();producedHistory=chamfer;
                }
            }
            if(!geometryOkay(next,surface,error)){
                if(error)*error=QStringLiteral("Feature %1 (%2): %3").arg(i+1).arg(op.type,*error);
                return false;
            }
            outputs.insert(op.id,next);intermediate.append(next);previous=next;
        }
        *output=previous;
        // Publish only after the whole rebuild succeeds. These OCCT builders
        // are valid for this geometry transaction only, never persisted.
        if(kernelHistory)*kernelHistory=std::move(producedHistory);
        if(perNodeOutputs)*perNodeOutputs=std::move(intermediate);
        return true;
    }catch(const Standard_Failure& e){fail(error,QStringLiteral("CAD kernel feature rebuild failed: %1").arg(e.what()));}
     catch(const std::exception& e){fail(error,QStringLiteral("Feature rebuild failed: %1").arg(e.what()));}
    return false;
}
}

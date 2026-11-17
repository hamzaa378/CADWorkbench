#include "BooleanReferenceLineage.h"
#include "ShapeFeatureTree.h"
#include <TopAbs_ShapeEnum.hxx>
#include <TopAbs_State.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <QStringList>
#include <QSet>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <QString>
#include <cmath>
#include <utility>

namespace cad {
namespace {
// Defined alongside the other planar-reference geometry helpers below.
// Forward declaration is required by the Boolean rebase path above them.
bool insideTrimmedPlanarFace(const TopoDS_Face& face,const gp_Pnt& point);
void fail(QString* error,const QString& detail){if(error)*error=detail;}

// Match the Boolean kernel's topological successor *inside the current result*.
// History can report shapes that were later removed by the same operation;
// restricting them to the final output prevents a false attachment.
QVector<int> successors(const TopoDS_Shape& result,TopAbs_ShapeEnum kind,
                        const TopoDS_Shape& original,
                        BRepBuilderAPI_MakeShape& boolean){
    TopTools_IndexedMapOfShape available;
    TopExp::MapShapes(result,kind,available);
    QVector<int> found;
    const auto collect=[&](const TopoDS_Shape& candidate){
        for(int i=1;i<=available.Extent();++i){
            if(!available.FindKey(i).IsSame(candidate)||found.contains(i))continue;
            found.append(i);
        }
    };
    // OCCT fillet/chamfer builders can report IsDeleted(original) even when
    // the exact unmodified TShape remains in their final result. The final
    // output is authoritative: collect() only accepts an IsSame match that
    // is actually present there. A truly consumed edge cannot match.
    collect(original);
    for(const TopoDS_Shape& descendant:boolean.Modified(original))
        collect(descendant);
    for(const TopoDS_Shape& descendant:boolean.Generated(original))
        collect(descendant);
    return found;
}

QMap<QString,double> numericAnchor(const QMap<QString,double>& parameters,
                                   const QString& prefix){
    QMap<QString,double> result;
    for(auto it=parameters.cbegin();it!=parameters.cend();++it){
        if(prefix==QStringLiteral("edge")){
            if((it.key().startsWith("edge_")&&it.key()!=QStringLiteral("edge_index"))||it.key()==QStringLiteral("edge_x")||
               it.key()==QStringLiteral("edge_y")||it.key()==QStringLiteral("edge_z"))
                result.insert(it.key(),it.value());
        }else if(prefix==QStringLiteral("curved")){
            if(it.key().startsWith("curved_"))result.insert(it.key(),it.value());
        }else if(it.key().startsWith("face_anchor_"))result.insert(it.key(),it.value());
    }
    return result;
}

bool selectedEdge(const QString& type){
    return type==QStringLiteral("Fillet selected edge")||
           type==QStringLiteral("Chamfer selected edge");
}
bool selectedFace(const QString& type){
    return type.startsWith(QStringLiteral("Face normal "))||
           type.startsWith(QStringLiteral("Face rectangle "))||
           type.startsWith(QStringLiteral("Face slot "))||
           type==QStringLiteral("Face hole grid through all")||
           type==QStringLiteral("Face bolt circle through all")||
           type==QStringLiteral("Face through-all hole")||
           type==QStringLiteral("Face counterbore through all")||
           type==QStringLiteral("Face countersink through all")||
           type==QStringLiteral("Open-face shell")||type==QStringLiteral("Draft face Z")||
           type==QStringLiteral("Sketch polygon face boss")||
           type==QStringLiteral("Sketch polygon face pocket");
}
}

bool rebaseFeaturesThroughBoolean(const TopoDS_Shape& source,
                                 const TopoDS_Shape& result,
                                 BRepBuilderAPI_MakeShape& boolean,
                                 const QVector<ShapeFeatureNode>& features,
                                 QVector<ShapeFeatureNode>* rebound,
                                 QString* error){
    if(!rebound||features.isEmpty()||features.size()>40){
        fail(error,"Boolean batch reference recovery needs 1-40 features and a destination.");return false;
    }
    QSet<QString> ids;
    QVector<ShapeFeatureNode> staged;staged.reserve(features.size());
    for(const ShapeFeatureNode& feature:features){
        if(feature.id.trimmed().isEmpty()||ids.contains(feature.id)){
            fail(error,"Boolean reference recovery has a missing or duplicated feature ID.");return false;
        }
        ids.insert(feature.id);
        ShapeFeatureNode resolved;
        if(!rebaseFeatureThroughBoolean(source,result,boolean,feature,&resolved,error))return false;
        staged.append(std::move(resolved));
    }
    *rebound=std::move(staged);
    if(error)error->clear();
    return true;
}

bool rebaseFeatureThroughBoolean(const TopoDS_Shape& source,const TopoDS_Shape& result,
                                BRepBuilderAPI_MakeShape& boolean,
                                const ShapeFeatureNode& feature,ShapeFeatureNode* rebound,
                                QString* error){
    if(!rebound||source.IsNull()||result.IsNull()){
        fail(error,"Boolean reference rebase requires valid input and output solids.");return false;
    }
    // A builder belongs to one transaction. Refuse provenance from a
    // different result, even if geometric similarity might mask the mistake.
    if(boolean.Shape().IsNull()||!boolean.Shape().IsSame(result)){
        fail(error,"Recorded topology history does not belong to this rebuild result.");return false;
    }
    ShapeFeatureNode changed=feature;
    if(selectedEdge(feature.type)){
        if(!feature.parameters.contains("edge_anchor_version")){
            fail(error,"Boolean edge references require a witnessed v2 edge anchor on the source branch.");
            return false;
        }
        const auto oldAnchor=numericAnchor(feature.parameters,"edge");
        int before=0;
        if(!resolveStableEdgeAnchor(source,oldAnchor,&before,error))return false;
        TopTools_IndexedMapOfShape from;TopExp::MapShapes(source,TopAbs_EDGE,from);
        const auto candidates=successors(result,TopAbs_EDGE,from.FindKey(before),boolean);
        if(candidates.isEmpty()){
            fail(error,"Boolean operation removed the referenced edge; downstream feature was not rebuilt.");return false;
        }
        // A single kernel descendant is NOT sufficient: a Boolean may trim
        // the witnessed region away but leave another fragment of that edge.
        // Require the saved interior witness to survive in the final output
        // even if Modified() reports only one candidate.
        int chosen=0;
        if(!resolveStableEdgeAnchor(result,oldAnchor,&chosen,error)||!candidates.contains(chosen)){
            fail(error,"Boolean edge descendant no longer contains the saved design witness; reselect the edge.");
            return false;
        }
        QMap<QString,double> fresh;
        if(!captureStableEdgeAnchor(result,chosen,&fresh,error))return false;
        // The newly captured midpoint belongs to the *trimmed descendant*,
        // not necessarily the designer's original location. Preserve the
        // original physical edge point and support tangent/curvature across
        // repeated Boolean splits/merges. The witness was independently
        // resolved on the current output and confirmed in the builder's
        // Modified/Generated/unchanged provenance above. A removed or
        // boundary witness therefore still fails closed.
        for(const char* field:{"edge_anchor_version","edge_curve_kind",
            "edge_witness_x","edge_witness_y","edge_witness_z",
            "edge_tangent_x","edge_tangent_y","edge_tangent_z",
            "edge_curvature"})
            fresh.insert(QString::fromLatin1(field),oldAnchor.value(QString::fromLatin1(field)));
        for(auto it=oldAnchor.cbegin();it!=oldAnchor.cend();++it)changed.parameters.remove(it.key());
        changed.parameters.insert("edge_index",chosen);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)changed.parameters.insert(it.key(),it.value());
    }else if(feature.type==QStringLiteral("Open-face shell")&&
             feature.parameters.contains("curved_type")){
        const auto oldAnchor=numericAnchor(feature.parameters,"curved");
        TopoDS_Face original;
        if(!resolveCurvedFaceAnchor(source,oldAnchor,&original,error))return false;
        const auto candidates=successors(result,TopAbs_FACE,original,boolean);
        if(candidates.isEmpty()){
            fail(error,"Boolean removed the shell opening's curved face.");return false;
        }
        TopoDS_Face witnessed;
        if(!resolveCurvedFaceAnchor(result,oldAnchor,&witnessed,error))return false;
        TopTools_IndexedMapOfShape faces;TopExp::MapShapes(result,TopAbs_FACE,faces);
        int chosen=0;
        for(const int candidate:candidates)
            if(faces.FindKey(candidate).IsSame(witnessed)){chosen=candidate;break;}
        if(!chosen){
            fail(error,"Curved shell witness no longer belongs to its kernel-confirmed Boolean descendants.");
            return false;
        }
        int explorerIndex=0,index=0;
        for(TopExp_Explorer walk(result,TopAbs_FACE);walk.More();walk.Next()){
            ++index;if(walk.Current().IsSame(faces.FindKey(chosen))){explorerIndex=index;break;}
        }
        if(!explorerIndex){fail(error,"Curved shell successor is missing.");return false;}
        QMap<QString,double> fresh;
        if(!captureCurvedFaceAnchor(result,explorerIndex,&fresh,error))return false;
        // A Boolean crop can change the sampled interior point of the
        // remaining curved face. Preserve the physical design witness that
        // already selected a UNIQUE kernel-confirmed descendant, including
        // the optional second witness on freeform surfaces. Re-resolving
        // this saved point at the next Boolean prevents cumulative drift.
        for(const QString& prefix:{QStringLiteral("curved_witness_"),
                                   QStringLiteral("curved_witness2_")}){
            for(const QString& axis:{QStringLiteral("x"),QStringLiteral("y"),QStringLiteral("z")}){
                const QString field=prefix+axis;
                if(oldAnchor.contains(field))fresh.insert(field,oldAnchor.value(field));
            }
        }
        for(auto it=oldAnchor.cbegin();it!=oldAnchor.cend();++it)changed.parameters.remove(it.key());
        changed.parameters.insert("face_index",explorerIndex);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)changed.parameters.insert(it.key(),it.value());
    }else if(selectedFace(feature.type)){
        if(!feature.parameters.contains("face_anchor_witness_x")){
            fail(error,"Boolean face references require an interior planar witness on the source branch.");
            return false;
        }
        const auto oldAnchor=numericAnchor(feature.parameters,"face");
        TopoDS_Face oldFace;
        if(!resolvePlanarFaceAnchor(source,oldAnchor,&oldFace,error))return false;
        const auto candidates=successors(result,TopAbs_FACE,oldFace,boolean);
        if(candidates.isEmpty()){
            fail(error,"Boolean operation removed the referenced face; downstream feature was not rebuilt.");return false;
        }
        // A face may have a unique modified descendant even though its saved
        // witness lies inside a NEW Boolean hole. Never move a hole/boss to a
        // different part of the face in that situation.
        TopoDS_Face witnessedFace;
        if(!resolvePlanarFaceAnchor(result,oldAnchor,&witnessedFace,error)){
            fail(error,"Boolean removed the referenced face region; reselect the design location.");
            return false;
        }
        // The capture/resolve API uses face explorer indices, not unique map
        // indices. Translate the chosen topological identity explicitly.
        auto findExplorerIndex=[&](const TopoDS_Shape& selected){
            int index=0;
            for(TopExp_Explorer walk(result,TopAbs_FACE);walk.More();walk.Next())
                if(++index>0&&walk.Current().IsSame(selected))return index;
            return 0;
        };
        TopTools_IndexedMapOfShape faces;TopExp::MapShapes(result,TopAbs_FACE,faces);
        int chosen=candidates.first();
        if(candidates.size()>1){
            chosen=0;
            for(const int candidate:candidates)if(faces.FindKey(candidate).IsSame(witnessedFace)){
                chosen=candidate;break;
            }
            if(!chosen){fail(error,"Face witness no longer belongs to its Boolean lineage.");return false;}
        }
        if(!faces.FindKey(chosen).IsSame(witnessedFace)){
            fail(error,"Boolean descendant no longer contains the referenced face witness.");return false;
        }
        const int explorerIndex=findExplorerIndex(faces.FindKey(chosen));
        if(!explorerIndex){fail(error,"Boolean face descendant is missing.");return false;}
        QMap<QString,double> fresh;
        if(!capturePlanarFaceAnchor(result,explorerIndex,&fresh,error)||
           !fresh.contains("face_anchor_witness_x")){
            fail(error,"Boolean successor face cannot provide an interior witness.");return false;
        }
        // Keep the ORIGINAL physical design witness across successive
        // Boolean splits and co-planar merges. Re-capturing a successor's
        // centroid/interior point at every stage would quietly walk the
        // reference across the model (even if the first rebase was correct).
        // The kernel provenance above confirms the descendant; the trimmed
        // interior classifier below confirms the actual design location.
        const gp_Pnt persistentWitness(oldAnchor.value("face_anchor_witness_x"),
                                       oldAnchor.value("face_anchor_witness_y"),
                                       oldAnchor.value("face_anchor_witness_z"));
        const TopoDS_Face selectedSuccessor=TopoDS::Face(faces.FindKey(chosen));
        if(!insideTrimmedPlanarFace(selectedSuccessor,persistentWitness)){
            fail(error,"Boolean successor no longer owns the original interior design witness.");
            return false;
        }
        fresh.insert("face_anchor_witness_x",persistentWitness.X());
        fresh.insert("face_anchor_witness_y",persistentWitness.Y());
        fresh.insert("face_anchor_witness_z",persistentWitness.Z());
        // A secondary witness is optional. Preserve it ONLY if the same
        // physical point survived on this descendant. A split may remove it
        // while retaining the primary witness; it must never migrate to a
        // newly sampled position in that case.
        for(const auto* axis:{"x","y","z"})
            fresh.remove(QStringLiteral("face_anchor_witness2_")+QString::fromLatin1(axis));
        if(oldAnchor.contains("face_anchor_witness2_x")){
            const gp_Pnt second(oldAnchor.value("face_anchor_witness2_x"),
                                oldAnchor.value("face_anchor_witness2_y"),
                                oldAnchor.value("face_anchor_witness2_z"));
            if(insideTrimmedPlanarFace(selectedSuccessor,second)){
                fresh.insert("face_anchor_witness2_x",second.X());
                fresh.insert("face_anchor_witness2_y",second.Y());
                fresh.insert("face_anchor_witness2_z",second.Z());
            }
        }
        // Offset-driven face features are positioned relative to a face's
        // centroid. Boolean splits and merges move that centroid even when
        // the physical design location is unchanged. Re-express the original
        // in-plane location in the new face frame before rebuilding.
        if(feature.parameters.contains("u")&&feature.parameters.contains("v")){
            const gp_Vec oldNormal(oldAnchor.value("face_anchor_nx"),
                oldAnchor.value("face_anchor_ny"),oldAnchor.value("face_anchor_nz"));
            const gp_Vec newNormal(fresh.value("face_anchor_nx"),
                fresh.value("face_anchor_ny"),fresh.value("face_anchor_nz"));
            if(oldNormal.Dot(newNormal)<0.999){
                fail(error,"Boolean successor face changed its local frame; reselect the reference.");
                return false;
            }
            const auto axes=[](const gp_Vec& normal){
                gp_Vec x=std::abs(normal.X())<0.9?gp_Vec(1,0,0):gp_Vec(0,1,0);
                x-=normal.Multiplied(x.Dot(normal));x.Normalize();
                return std::pair<gp_Vec,gp_Vec>(x,normal.Crossed(x));
            };
            const auto [oldX,oldY]=axes(oldNormal);
            const auto [newX,newY]=axes(newNormal);
            gp_Pnt designPoint(oldAnchor.value("face_anchor_x"),oldAnchor.value("face_anchor_y"),
                               oldAnchor.value("face_anchor_z"));
            designPoint.Translate(oldX.Multiplied(feature.parameters.value("u"))+
                                  oldY.Multiplied(feature.parameters.value("v")));
            const gp_Pnt center(fresh.value("face_anchor_x"),fresh.value("face_anchor_y"),
                                fresh.value("face_anchor_z"));
            const gp_Vec offset(center,designPoint);
            if(std::abs(offset.Dot(newNormal))>1e-4){
                fail(error,"Referenced face moved off its original design plane; reselect it.");
                return false;
            }
            // A surviving face witness does not guarantee that the *machining
            // center* survived. For example, a Boolean hole can consume the
            // pocket center while retaining the face elsewhere. Never relocate
            // the machining feature to a remaining patch by accident.
            if(!insideTrimmedPlanarFace(TopoDS::Face(faces.FindKey(chosen)),designPoint)){
                fail(error,"Boolean removed or clipped the saved machining center; reselect the face location.");
                return false;
            }
            changed.parameters.insert("u",offset.Dot(newX));
            changed.parameters.insert("v",offset.Dot(newY));
        }
        for(auto it=oldAnchor.cbegin();it!=oldAnchor.cend();++it)changed.parameters.remove(it.key());
        changed.parameters.insert("face_index",explorerIndex);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)changed.parameters.insert(it.key(),it.value());
    }else{
        fail(error,"Only witnessed face features and selected-edge fillets/chamfers support Boolean ancestry.");
        return false;
    }
    if(!validateShapeFeatureNodes({changed},error))return false;
    *rebound=std::move(changed);
    return true;
}
}

namespace cad {
namespace {
bool referenceSimilarityTransform(const ShapeFeatureNode& operation,gp_Trsf* transform,QString* error){
    if(!transform||!supportsSimilarityReferenceLineage(operation)){
        fail(error,"Only rigid moves, reflections and positive uniform scaling support similarity lineage.");return false;
    }
    const auto value=[&](const char* name){return operation.parameters.value(QString::fromLatin1(name));};
    for(auto it=operation.parameters.cbegin();it!=operation.parameters.cend();++it){
        if(!std::isfinite(it.value())||std::abs(it.value())>1e6){
            fail(error,"Reference transform contains an invalid parameter.");return false;
        }
    }
    if(operation.type==QStringLiteral("Uniform scale")){
        if(operation.parameters.size()!=1||!operation.parameters.contains("factor")||
           value("factor")<1e-4||value("factor")>1e4){
            fail(error,"Similarity lineage needs a finite positive uniform scale.");return false;
        }
        transform->SetScale(gp_Pnt(0,0,0),value("factor"));
    }else if(operation.type.startsWith(QStringLiteral("Mirror "))){
        const bool arbitrary=operation.type==QStringLiteral("Mirror arbitrary plane");
        if(operation.parameters.size()!=(arbitrary?6:0)){
            fail(error,"Mirror reference lineage has unexpected parameters.");return false;
        }
        gp_Dir normal(0,0,1);gp_Pnt origin(0,0,0);
        if(operation.type==QStringLiteral("Mirror XZ"))normal=gp_Dir(0,1,0);
        else if(operation.type==QStringLiteral("Mirror YZ"))normal=gp_Dir(1,0,0);
        else if(arbitrary){
            for(const char* key:{"normal_x","normal_y","normal_z",
                                    "origin_x","origin_y","origin_z"})
                if(!operation.parameters.contains(QLatin1String(key))){
                    fail(error,"Mirror plane has missing origin or normal coordinates.");return false;
                }
            const gp_Vec raw(value("normal_x"),value("normal_y"),value("normal_z"));
            if(raw.SquareMagnitude()<1e-12){
                fail(error,"Mirror plane normal cannot be zero.");return false;
            }
            normal=gp_Dir(raw);
            origin=gp_Pnt(value("origin_x"),value("origin_y"),value("origin_z"));
        }
        transform->SetMirror(gp_Ax2(origin,normal));
    }else if(operation.type==QStringLiteral("Translate")){
        if(operation.parameters.size()!=3||!operation.parameters.contains("x")||
           !operation.parameters.contains("y")||!operation.parameters.contains("z"))return false;
        transform->SetTranslation(gp_Vec(value("x"),value("y"),value("z")));
    }else{
        const bool arbitrary=operation.type==QStringLiteral("Rotate arbitrary axis");
        if(operation.parameters.size()!=(arbitrary?7:1)||!operation.parameters.contains("degrees"))return false;
        gp_Dir axis(0,0,1);gp_Pnt pivot(0,0,0);
        if(arbitrary){
            const QStringList required={"axis_x","axis_y","axis_z","pivot_x","pivot_y","pivot_z"};
            for(const QString& key:required)if(!operation.parameters.contains(key))return false;
            const gp_Vec direction(value("axis_x"),value("axis_y"),value("axis_z"));
            if(direction.SquareMagnitude()<1e-12){fail(error,"Invalid rotation axis.");return false;}
            axis=gp_Dir(direction);
            pivot=gp_Pnt(value("pivot_x"),value("pivot_y"),value("pivot_z"));
        }else if(operation.type==QStringLiteral("Rotate X"))axis=gp_Dir(1,0,0);
        else if(operation.type==QStringLiteral("Rotate Y"))axis=gp_Dir(0,1,0);
        transform->SetRotation(gp_Ax1(pivot,axis),value("degrees")*std::acos(-1.0)/180.0);
    }
    return true;
}
void pointFields(QMap<QString,double>* fields,const QString& prefix,const gp_Trsf& tr){
    gp_Pnt point(fields->value(prefix+"x"),fields->value(prefix+"y"),fields->value(prefix+"z"));
    point.Transform(tr);
    fields->insert(prefix+"x",point.X());
    fields->insert(prefix+"y",point.Y());
    fields->insert(prefix+"z",point.Z());
}
void directionFields(QMap<QString,double>* fields,const QString& prefix,const gp_Trsf& tr){
    gp_Vec direction(fields->value(prefix+"x"),fields->value(prefix+"y"),fields->value(prefix+"z"));
    direction.Transform(tr);
    // A scale changes vector magnitude, but a persisted normal/tangent/axis
    // is a UNIT direction in the resolver's schema.
    if(direction.SquareMagnitude()>1e-18)direction.Normalize();
    fields->insert(prefix+"x",direction.X());
    fields->insert(prefix+"y",direction.Y());
    fields->insert(prefix+"z",direction.Z());
}
std::pair<gp_Vec,gp_Vec> planarAxes(const gp_Vec& normal){
    gp_Vec x=std::abs(normal.X())<0.9?gp_Vec(1,0,0):gp_Vec(0,1,0);
    x-=normal.Multiplied(x.Dot(normal));x.Normalize();
    return {x,normal.Crossed(x)};
}
// The OCCT face classifier projects a 3D point onto a surface. Its IN
// result alone does not prove that the point lies on that face's plane.
// Verify the plane distance before checking trimmed-wire containment.
bool insideTrimmedPlanarFace(const TopoDS_Face& face,const gp_Pnt& point){
    if(face.IsNull())return false;
    const BRepAdaptor_Surface surface(face);
    if(surface.GetType()!=GeomAbs_Plane||surface.Plane().Distance(point)>1e-5)return false;
    const BRepClass_FaceClassifier classifier(face,point,1e-5);
    return classifier.State()==TopAbs_IN;
}
// Narrow, fail-closed recovery for a face *generated by the direct parent*
// whose entire support plane moved along its normal (for example a changed
// loft height). This is NOT used for arbitrary Boolean lineage: the saved
// feature creator ID must already name this exact evaluated parent stage.
// Two similar candidate faces remain ambiguous, even if one is closer.
bool findDirectStageTranslatedPlanarFace(const TopoDS_Shape& current,
        const QMap<QString,double>& old,TopoDS_Face* selected,
        QMap<QString,double>* descriptor,double* planeShift,QString* error){
    if(!selected||!descriptor||!planeShift||
       !old.contains("face_anchor_witness_x")||
       !old.contains("face_anchor_area")){
        fail(error,"Translated generated-face recovery requires a complete witnessed planar anchor.");return false;
    }
    const gp_Vec n(old.value("face_anchor_nx"),old.value("face_anchor_ny"),old.value("face_anchor_nz"));
    const double area=old.value("face_anchor_area");
    if(std::abs(n.SquareMagnitude()-1.0)>1e-4||area<=1e-8){
        fail(error,"Invalid direct-stage planar reference orientation or area.");return false;
    }
    const gp_Pnt oldCenter(old.value("face_anchor_x"),old.value("face_anchor_y"),old.value("face_anchor_z"));
    const gp_Pnt oldWitness(old.value("face_anchor_witness_x"),
        old.value("face_anchor_witness_y"),old.value("face_anchor_witness_z"));
    int matches=0;TopoDS_Face found;QMap<QString,double> fresh;double shift=0.0;
    int index=0;
    for(TopExp_Explorer it(current,TopAbs_FACE);it.More();it.Next()){
        ++index;
        const TopoDS_Face candidate=TopoDS::Face(it.Current());
        QMap<QString,double> proposed;
        if(!capturePlanarFaceAnchor(current,index,&proposed,nullptr))continue;
        const gp_Vec normal(proposed.value("face_anchor_nx"),
            proposed.value("face_anchor_ny"),proposed.value("face_anchor_nz"));
        if(normal.Dot(n)<0.9999)continue;
        const double newArea=proposed.value("face_anchor_area");
        // A translated cap keeps its in-plane footprint/area. Deliberately
        // refuse remodeling that substantially changes the face profile.
        if(newArea<=0||std::abs(std::log(newArea/area))>0.02)continue;
        const gp_Pnt center(proposed.value("face_anchor_x"),
            proposed.value("face_anchor_y"),proposed.value("face_anchor_z"));
        const gp_Vec displacement(oldCenter,center);
        const double normalTravel=displacement.Dot(n);
        const gp_Vec lateral=displacement-n.Multiplied(normalTravel);
        if(lateral.Magnitude()>std::max(1e-4,0.005*std::sqrt(area)))continue;
        gp_Pnt translatedWitness=oldWitness;
        translatedWitness.Translate(n.Multiplied(normalTravel));
        if(!insideTrimmedPlanarFace(candidate,translatedWitness))continue;
        ++matches;found=candidate;fresh=std::move(proposed);shift=normalTravel;
    }
    if(matches!=1){
        fail(error,matches?"Multiple translated generated faces match the saved reference; reselect the face.":
             "The generated face changed shape or location beyond safe normal-translation lineage.");
        return false;
    }
    *selected=found;*descriptor=std::move(fresh);*planeShift=shift;return true;
}
// A direct-stage curved face has an explicit creator-stage identity. On a
// fresh rebuild a coaxial cylindrical face may have a changed radius (for
// example a regenerated rounded feature). Recover ONLY if the old radial
// design location projects into exactly one trimmed new cylinder, with the
// same axis and a bounded radius change. No nearest-center/radius fallback.
bool findDirectStageCoaxialCylinder(const TopoDS_Shape& current,
        const QMap<QString,double>& saved,TopoDS_Face* selected,QString* error){
    if(!selected||saved.size()!=14||saved.value("curved_type")!=1.0){
        fail(error,"Only complete, cylindrical direct-stage references support radius updates.");return false;
    }
    const QStringList fields={"curved_body_x","curved_body_y","curved_body_z",
        "curved_center_x","curved_center_y","curved_center_z",
        "curved_axis_x","curved_axis_y","curved_axis_z","curved_radius",
        "curved_witness_x","curved_witness_y","curved_witness_z"};
    for(const QString& field:fields)
        if(!saved.contains(field)||!std::isfinite(saved.value(field))){
            fail(error,"Cylindrical face witness is incomplete.");return false;
        }
    const gp_Vec axis(saved.value("curved_axis_x"),saved.value("curved_axis_y"),
                      saved.value("curved_axis_z"));
    const double radius=saved.value("curved_radius");
    if(std::abs(axis.SquareMagnitude()-1.0)>1e-4||radius<1e-8){
        fail(error,"Cylindrical face axis or radius is invalid.");return false;
    }
    const gp_Pnt center(saved.value("curved_center_x"),saved.value("curved_center_y"),
                        saved.value("curved_center_z"));
    const gp_Pnt witness(saved.value("curved_witness_x"),saved.value("curved_witness_y"),
                         saved.value("curved_witness_z"));
    const gp_Vec centerToWitness(center,witness);
    const gp_Vec radial=centerToWitness-axis.Multiplied(centerToWitness.Dot(axis));
    if(std::abs(radial.Magnitude()-radius)>std::max(1e-4,radius*1e-5)){
        fail(error,"Saved cylindrical witness is not on its support radius.");return false;
    }
    int matches=0;TopoDS_Face found;
    for(TopExp_Explorer it(current,TopAbs_FACE);it.More();it.Next()){
        const TopoDS_Face candidate=TopoDS::Face(it.Current());
        const BRepAdaptor_Surface surface(candidate);
        if(surface.GetType()!=GeomAbs_Cylinder)continue;
        const auto cylinder=surface.Cylinder();
        const double nextRadius=cylinder.Radius();
        if(nextRadius<1e-8||std::abs(nextRadius-radius)>0.20*radius)continue;
        const gp_Vec currentAxis(cylinder.Axis().Direction());
        if(std::abs(currentAxis.Dot(axis))<0.99999)continue;
        const gp_Vec axisOffset(center,cylinder.Axis().Location());
        if((axisOffset-axis.Multiplied(axisOffset.Dot(axis))).Magnitude()>1e-4)continue;
        gp_Pnt predicted=witness;
        predicted.Translate(radial.Multiplied(nextRadius/radius-1.0));
        const BRepClass_FaceClassifier classification(candidate,predicted,1e-5);
        if(classification.State()!=TopAbs_IN)continue;
        found=candidate;++matches;
    }
    if(matches!=1){
        fail(error,matches?"Multiple coaxial generated faces contain the design witness; reselect.":
             "No coaxial generated face preserves the radial design location.");return false;
    }
    *selected=found;return true;
}
// A circular fillet often creates a toroidal face. When its fillet radius
// changes, the support is no longer identical, even though its major circle
// and axis remain fixed. A narrowly bounded *direct-stage* recovery is safe
// only when the saved witness projects into exactly one trimmed successor.
// No inference is made about changed Boolean provenance or blended B-splines.
bool findDirectStageFilletTorus(const TopoDS_Shape& current,
        const QMap<QString,double>& saved,TopoDS_Face* selected,QString* error){
    if(!selected||saved.size()!=15||saved.value("curved_type")!=4.0){
        fail(error,"Direct fillet recovery requires a complete toroidal witness.");return false;
    }
    const double major=saved.value("curved_radius"),minor=saved.value("curved_aux");
    const gp_Pnt center(saved.value("curved_center_x"),saved.value("curved_center_y"),saved.value("curved_center_z"));
    const gp_Pnt witness(saved.value("curved_witness_x"),saved.value("curved_witness_y"),saved.value("curved_witness_z"));
    const gp_Vec axis(saved.value("curved_axis_x"),saved.value("curved_axis_y"),saved.value("curved_axis_z"));
    if(!std::isfinite(major)||!std::isfinite(minor)||minor<1e-6||major<=minor||
       std::abs(axis.SquareMagnitude()-1)>1e-4){
        fail(error,"Invalid original toroidal fillet support.");return false;
    }
    int count=0;TopoDS_Face matched;
    for(TopExp_Explorer it(current,TopAbs_FACE);it.More();it.Next()){
        const TopoDS_Face candidate=TopoDS::Face(it.Current());
        const BRepAdaptor_Surface adaptor(candidate);
        if(adaptor.GetType()!=GeomAbs_Torus)continue;
        const auto torus=adaptor.Torus();
        if(torus.Location().Distance(center)>1e-4||
           std::abs(gp_Vec(torus.Axis().Direction()).Dot(axis))<0.99999||
           std::abs(torus.MajorRadius()-major)>std::max(1e-4,major*1e-5)||
           std::abs(torus.MinorRadius()-minor)>minor*0.20)continue;
        GeomAPI_ProjectPointOnSurf projection(witness,BRep_Tool::Surface(candidate));
        if(projection.NbPoints()<1||projection.LowerDistance()>0.21*minor)continue;
        Standard_Real u=0,v=0;projection.LowerDistanceParameters(u,v);
        const gp_Pnt projected=adaptor.Value(u,v);
        const BRepClass_FaceClassifier inside(candidate,projected,1e-5);
        if(inside.State()!=TopAbs_IN)continue;
        matched=candidate;++count;
    }
    if(count!=1){
        fail(error,count?"Multiple possible toroidal fillet successors; reselect the face.":
             "The fillet support or trimmed witness changed beyond safe recovery.");
        return false;
    }
    *selected=matched;return true;
}
int faceExplorerIndex(const TopoDS_Shape& result,const TopoDS_Face& selected){
    int i=0;
    for(TopExp_Explorer e(result,TopAbs_FACE);e.More();e.Next()){
        ++i;if(e.Current().IsSame(selected))return i;
    }
    return 0;
}
}
bool supportsSimilarityReferenceLineage(const ShapeFeatureNode& operation){
    return supportsRigidReferenceLineage(operation)||
           operation.type==QStringLiteral("Uniform scale")||
           operation.type==QStringLiteral("Mirror XY")||
           operation.type==QStringLiteral("Mirror XZ")||
           operation.type==QStringLiteral("Mirror YZ")||
           operation.type==QStringLiteral("Mirror arbitrary plane");
}
bool supportsRigidReferenceLineage(const ShapeFeatureNode& operation){
    return operation.type==QStringLiteral("Translate")||
           operation.type==QStringLiteral("Rotate X")||
           operation.type==QStringLiteral("Rotate Y")||
           operation.type==QStringLiteral("Rotate Z")||
           operation.type==QStringLiteral("Rotate arbitrary axis");
}
bool transportDesignPointThroughRigid(const ShapeFeatureNode& operation,
                                     gp_Pnt* designPoint,QString* error){
    if(!supportsRigidReferenceLineage(operation)){
        fail(error,"Rigid design point transport requires translation or rotation.");return false;
    }
    return transportDesignPointThroughSimilarity(operation,designPoint,error);
}
bool transportDesignPointThroughSimilarity(const ShapeFeatureNode& operation,
                                          gp_Pnt* designPoint,QString* error){
    if(!designPoint){fail(error,"Similarity design point is missing.");return false;}
    gp_Trsf tr;
    if(!referenceSimilarityTransform(operation,&tr,error))return false;
    designPoint->Transform(tr);
    return std::isfinite(designPoint->X())&&std::isfinite(designPoint->Y())&&
           std::isfinite(designPoint->Z());
}
bool rebaseFeatureThroughRigid(const TopoDS_Shape& source,const TopoDS_Shape& result,
                              const ShapeFeatureNode& operation,const ShapeFeatureNode& feature,
                              ShapeFeatureNode* rebound,QString* error){
    if(!supportsRigidReferenceLineage(operation)){
        fail(error,"Rigid feature rebasing requires translation or rotation.");return false;
    }
    return rebaseFeatureThroughSimilarity(source,result,operation,feature,rebound,error);
}
bool rebaseFeatureThroughSimilarity(const TopoDS_Shape& source,const TopoDS_Shape& result,
                                    const ShapeFeatureNode& operation,const ShapeFeatureNode& feature,
                                    ShapeFeatureNode* rebound,QString* error){
    if(source.IsNull()||result.IsNull()||!rebound){
        fail(error,"Rigid lineage requires a valid source and result.");return false;
    }
    gp_Trsf tr;
    if(!referenceSimilarityTransform(operation,&tr,error))return false;
    const bool scaling=operation.type==QStringLiteral("Uniform scale");
    const double scale=scaling?operation.parameters.value("factor"):1.0;
    ShapeFeatureNode changed=feature;
    if(selectedEdge(feature.type)){
        if(!feature.parameters.contains("edge_anchor_version")){
            fail(error,"Rigid edge lineage requires a witnessed edge.");return false;
        }
        const auto old=numericAnchor(feature.parameters,"edge");
        int original=0;
        if(!resolveStableEdgeAnchor(source,old,&original,error))return false;
        QMap<QString,double> moved=old;
        pointFields(&moved,"edge_",tr);
        pointFields(&moved,"edge_body_",tr);
        pointFields(&moved,"edge_witness_",tr);
        directionFields(&moved,"edge_tangent_",tr);
        if(scaling){
            moved.insert("edge_length",old.value("edge_length")*scale);
            moved.insert("edge_curvature",old.value("edge_curvature")/scale);
        }
        int after=0;
        if(!resolveStableEdgeAnchor(result,moved,&after,error))return false;
        QMap<QString,double> fresh;
        if(!captureStableEdgeAnchor(result,after,&fresh,error))return false;
        if(!fresh.contains("edge_anchor_version")){
            fail(error,"Rigid successor edge could not retain a witness.");return false;
        }
        // A rigidly rotated/translated trimmed successor has a new midpoint,
        // which is not the physical design point. Retain the independently
        // transported original witness and tangent rather than introducing
        // cumulative drift on a second topology-changing operation.
        for(const char* field:{"edge_anchor_version","edge_curve_kind",
                "edge_witness_x","edge_witness_y","edge_witness_z",
                "edge_tangent_x","edge_tangent_y","edge_tangent_z",
                "edge_curvature"})
            fresh.insert(QString::fromLatin1(field),moved.value(QString::fromLatin1(field)));
        for(auto it=old.cbegin();it!=old.cend();++it)changed.parameters.remove(it.key());
        changed.parameters.insert("edge_index",after);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)changed.parameters.insert(it.key(),it.value());
    }else if(feature.type==QStringLiteral("Open-face shell")&&
             feature.parameters.contains("curved_type")){
        const auto old=numericAnchor(feature.parameters,"curved");
        TopoDS_Face sourceFace;
        if(!resolveCurvedFaceAnchor(source,old,&sourceFace,error))return false;
        QMap<QString,double> moved=old;
        pointFields(&moved,"curved_body_",tr);
        pointFields(&moved,"curved_center_",tr);
        pointFields(&moved,"curved_witness_",tr);
        if(moved.contains("curved_witness2_x"))pointFields(&moved,"curved_witness2_",tr);
        if(old.value("curved_type")!=2.0)
            directionFields(&moved,"curved_axis_",tr);
        if(scaling){
            const double kind=old.value("curved_type");
            // Non-analytic B-spline/Bezier faces cannot be certified by a
            // similarity of their old local size descriptor alone.
            if(kind==5.0||kind==6.0){
                fail(error,"Uniform-scale rebasing of a freeform face requires explicit reselection.");return false;
            }
            moved.insert("curved_radius",old.value("curved_radius")*scale);
            if(kind==4.0)moved.insert("curved_aux",old.value("curved_aux")*scale);
            // Cone semi-angle (kind 3) is dimensionless and must not scale.
        }
        TopoDS_Face successor;
        if(!resolveCurvedFaceAnchor(result,moved,&successor,error))return false;
        const int index=faceExplorerIndex(result,successor);
        if(!index){fail(error,"Rigid curved-shell successor is missing.");return false;}
        QMap<QString,double> fresh;
        if(!captureCurvedFaceAnchor(result,index,&fresh,error))return false;
        // The resolver has already confirmed this transformed witness belongs
        // to ONE output face. Do not replace it with the newly sampled center:
        // that causes curved shell openings to drift after several rotations.
        // Exact rigid transforms preserve both interior design witnesses.
        for(const QString& prefix:{QStringLiteral("curved_witness_"),
                                   QStringLiteral("curved_witness2_")}){
            for(const QString& axis:{QStringLiteral("x"),QStringLiteral("y"),QStringLiteral("z")}){
                const QString key=prefix+axis;
                if(moved.contains(key))fresh.insert(key,moved.value(key));
            }
        }
        for(auto it=old.cbegin();it!=old.cend();++it)changed.parameters.remove(it.key());
        changed.parameters.insert("face_index",index);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)changed.parameters.insert(it.key(),it.value());
    }else if(selectedFace(feature.type)){
        const auto old=numericAnchor(feature.parameters,"face");
        if(!old.contains("face_anchor_witness_x")){
            fail(error,"Rigid planar face lineage needs a trimmed interior witness.");return false;
        }
        TopoDS_Face sourceFace;
        if(!resolvePlanarFaceAnchor(source,old,&sourceFace,error))return false;
        gp_Pnt designPoint(old.value("face_anchor_x"),old.value("face_anchor_y"),old.value("face_anchor_z"));
        if(feature.parameters.contains("u")&&feature.parameters.contains("v")){
            const gp_Vec oldNormal(old.value("face_anchor_nx"),old.value("face_anchor_ny"),old.value("face_anchor_nz"));
            const auto [x,y]=planarAxes(oldNormal);
            designPoint.Translate(x.Multiplied(feature.parameters.value("u"))+
                                  y.Multiplied(feature.parameters.value("v")));
        }
        designPoint.Transform(tr);
        QMap<QString,double> moved=old;
        pointFields(&moved,"face_anchor_",tr);
        pointFields(&moved,"face_anchor_body_",tr);
        pointFields(&moved,"face_anchor_witness_",tr);
        if(moved.contains("face_anchor_witness2_x"))
            pointFields(&moved,"face_anchor_witness2_",tr);
        directionFields(&moved,"face_anchor_n",tr);
        if(scaling)moved.insert("face_anchor_area",old.value("face_anchor_area")*scale*scale);
        TopoDS_Face successor;
        if(!resolvePlanarFaceAnchor(result,moved,&successor,error))return false;
        const int index=faceExplorerIndex(result,successor);
        if(!index){fail(error,"Rigid successor face was not found in the rebuilt body.");return false;}
        QMap<QString,double> fresh;
        if(!capturePlanarFaceAnchor(result,index,&fresh,error)||
           !fresh.contains("face_anchor_witness_x"))return false;
        const gp_Pnt transportedWitness(moved.value("face_anchor_witness_x"),
            moved.value("face_anchor_witness_y"),moved.value("face_anchor_witness_z"));
        if(!insideTrimmedPlanarFace(successor,transportedWitness)){
            fail(error,"Rigid planar reference lost its transported interior design witness.");return false;
        }
        // Keep the original interior location even if a transformed or trimmed
        // face has a different sampled centroid. The second witness is only
        // retained if it belongs to the SAME unique successor after the move.
        for(const char* axis:{"x","y","z"}){
            const QString field=QStringLiteral("face_anchor_witness_")+QString::fromLatin1(axis);
            fresh.insert(field,moved.value(field));
        }
        for(const char* axis:{"x","y","z"})
            fresh.remove(QStringLiteral("face_anchor_witness2_")+QString::fromLatin1(axis));
        if(moved.contains("face_anchor_witness2_x")){
            const gp_Pnt other(moved.value("face_anchor_witness2_x"),
                moved.value("face_anchor_witness2_y"),moved.value("face_anchor_witness2_z"));
            if(insideTrimmedPlanarFace(successor,other))
                for(const char* axis:{"x","y","z"}){
                    const QString field=QStringLiteral("face_anchor_witness2_")+QString::fromLatin1(axis);
                    fresh.insert(field,moved.value(field));
                }
        }
        if(feature.parameters.contains("u")&&feature.parameters.contains("v")){
            const gp_Vec newNormal(fresh.value("face_anchor_nx"),fresh.value("face_anchor_ny"),fresh.value("face_anchor_nz"));
            const gp_Pnt newCenter(fresh.value("face_anchor_x"),fresh.value("face_anchor_y"),fresh.value("face_anchor_z"));
            const gp_Vec relative(newCenter,designPoint);
            if(std::abs(relative.Dot(newNormal))>1e-4||
               !insideTrimmedPlanarFace(successor,designPoint)){
                fail(error,"Rigidly transformed machining location was trimmed away or left its selected face.");return false;
            }
            const auto [x,y]=planarAxes(newNormal);
            changed.parameters.insert("u",relative.Dot(x));
            changed.parameters.insert("v",relative.Dot(y));
        }
        for(auto it=old.cbegin();it!=old.cend();++it)changed.parameters.remove(it.key());
        changed.parameters.insert("face_index",index);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)changed.parameters.insert(it.key(),it.value());
    }else{
        fail(error,"Rigid lineage supports witnessed selected edges and planar face features.");return false;
    }
    if(!validateShapeFeatureNodes({changed},error))return false;
    *rebound=std::move(changed);
    return true;
}
}

namespace cad {
bool supportsPatternReferenceLineage(const ShapeFeatureNode& operation){
    return operation.type==QStringLiteral("Linear pattern X")||
           operation.type==QStringLiteral("Linear pattern Y")||
           operation.type==QStringLiteral("Linear pattern Z")||
           operation.type==QStringLiteral("Linear pattern vector")||
           operation.type==QStringLiteral("Circular pattern Z")||
           operation.type==QStringLiteral("Circular pattern arbitrary axis");
}
namespace {
bool selectedPatternRigidOperation(const ShapeFeatureNode& operation,int occurrence,
                                   ShapeFeatureNode* rigid,QString* error){
    if(!rigid||!supportsPatternReferenceLineage(operation)||
       !validateShapeFeatureNodes({operation},error))return false;
    const int count=int(operation.parameters.value("count"));
    if(occurrence<0||occurrence>=count){
        fail(error,"Selected pattern occurrence no longer exists after the count changed.");
        return false;
    }
    ShapeFeatureNode constructed;constructed.id=operation.id;
    if(operation.type.startsWith(QStringLiteral("Linear pattern"))){
        double dx=0,dy=0,dz=0;
        if(operation.type==QStringLiteral("Linear pattern vector")){
            dx=operation.parameters.value("dx");dy=operation.parameters.value("dy");
            dz=operation.parameters.value("dz");
        }else{
            const double spacing=operation.parameters.value("spacing");
            if(operation.type==QStringLiteral("Linear pattern X"))dx=spacing;
            if(operation.type==QStringLiteral("Linear pattern Y"))dy=spacing;
            if(operation.type==QStringLiteral("Linear pattern Z"))dz=spacing;
        }
        constructed.type=QStringLiteral("Translate");
        constructed.parameters={{"x",occurrence*dx},{"y",occurrence*dy},{"z",occurrence*dz}};
    }else{
        const bool custom=operation.type==QStringLiteral("Circular pattern arbitrary axis");
        constructed.type=custom?QStringLiteral("Rotate arbitrary axis"):QStringLiteral("Rotate Z");
        const double sweep=operation.parameters.value("sweep_degrees");
        const double increment=sweep>=360.0-1e-8?sweep/count:sweep/(count-1);
        constructed.parameters={{"degrees",occurrence*increment}};
        // Keep in lockstep with the circular pattern geometry constructor.
        if(custom){
            for(const char* key:{"axis_x","axis_y","axis_z","pivot_x","pivot_y","pivot_z"})
                constructed.parameters.insert(QString::fromLatin1(key),operation.parameters.value(QString::fromLatin1(key)));
        }
    }
    *rigid=std::move(constructed);
    return true;
}
} // namespace
bool transportDesignPointThroughPattern(const ShapeFeatureNode& operation,int occurrence,
                                        gp_Pnt* designPoint,QString* error){
    ShapeFeatureNode rigid;
    if(!selectedPatternRigidOperation(operation,occurrence,&rigid,error))return false;
    return transportDesignPointThroughRigid(rigid,designPoint,error);
}
bool rebaseFeatureThroughPattern(const TopoDS_Shape& source,const TopoDS_Shape& result,
                                const ShapeFeatureNode& operation,int occurrence,
                                const ShapeFeatureNode& feature,ShapeFeatureNode* rebound,
                                QString* error){
    if(!rebound||source.IsNull()||result.IsNull()){
        fail(error,"A tracked pattern needs valid source and result geometry.");return false;
    }
    ShapeFeatureNode rigid;
    if(!selectedPatternRigidOperation(operation,occurrence,&rigid,error))return false;
    return rebaseFeatureThroughRigid(source,result,rigid,feature,rebound,error);
}}

namespace cad {
bool supportsSurvivingModifierLineage(const ShapeFeatureNode& operation){
    return operation.type==QStringLiteral("Fillet selected edge")||
           operation.type==QStringLiteral("Chamfer selected edge")||
           operation.type==QStringLiteral("Fillet selected edges")||
           operation.type==QStringLiteral("Chamfer selected edges")||
           operation.type==QStringLiteral("Fillet all edges")||
           operation.type==QStringLiteral("Chamfer all edges");
}

bool rebaseFeatureThroughSurvivingModifier(const TopoDS_Shape& source,
                                          const TopoDS_Shape& result,
                                          const ShapeFeatureNode& feature,
                                          ShapeFeatureNode* rebound,QString* error){
    if(!rebound||source.IsNull()||result.IsNull()){
        fail(error,"Surviving-subshape lineage requires two valid geometry stages.");return false;
    }
    ShapeFeatureNode changed=feature;
    if(selectedEdge(feature.type)){
        if(!feature.parameters.contains("edge_anchor_version")){
            fail(error,"Surviving edge needs a stable source witness.");return false;
        }
        const auto beforeAnchor=numericAnchor(feature.parameters,"edge");
        int original=0;
        if(!resolveStableEdgeAnchor(source,beforeAnchor,&original,error))return false;
        TopTools_IndexedMapOfShape oldEdges,newEdges;
        TopExp::MapShapes(source,TopAbs_EDGE,oldEdges);
        TopExp::MapShapes(result,TopAbs_EDGE,newEdges);
        int surviving=0;
        for(int i=1;i<=newEdges.Extent();++i){
            if(newEdges.FindKey(i).IsSame(oldEdges.FindKey(original))){
                if(surviving){fail(error,"Unchanged edge appears ambiguously in modifier output.");return false;}
                surviving=i;
            }
        }
        if(!surviving){
            fail(error,"Selected edge was consumed or replaced by fillet/chamfer; reselect it.");return false;
        }
        QMap<QString,double> fresh;
        if(!captureStableEdgeAnchor(result,surviving,&fresh,error))return false;
        for(auto it=beforeAnchor.cbegin();it!=beforeAnchor.cend();++it)
            changed.parameters.remove(it.key());
        changed.parameters.insert("edge_index",surviving);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)
            changed.parameters.insert(it.key(),it.value());
    }else if(feature.type==QStringLiteral("Open-face shell")&&
             feature.parameters.contains("curved_type")){
        const auto beforeAnchor=numericAnchor(feature.parameters,"curved");
        TopoDS_Face original;
        if(!resolveCurvedFaceAnchor(source,beforeAnchor,&original,error))return false;
        const int index=faceExplorerIndex(result,original);
        if(index<=0){
            fail(error,"Curved face was consumed by fillet/chamfer; reselect the opening.");return false;
        }
        QMap<QString,double> fresh;
        if(!captureCurvedFaceAnchor(result,index,&fresh,error))return false;
        for(auto it=beforeAnchor.cbegin();it!=beforeAnchor.cend();++it)
            changed.parameters.remove(it.key());
        changed.parameters.insert("face_index",index);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)
            changed.parameters.insert(it.key(),it.value());
    }else if(selectedFace(feature.type)){
        const auto beforeAnchor=numericAnchor(feature.parameters,"face");
        if(!beforeAnchor.contains("face_anchor_witness_x")){
            fail(error,"Surviving planar face needs an interior source witness.");return false;
        }
        TopoDS_Face original;
        if(!resolvePlanarFaceAnchor(source,beforeAnchor,&original,error))return false;
        const int index=faceExplorerIndex(result,original);
        if(index<=0){
            fail(error,"Referenced face was modified by fillet/chamfer; reselect the face.");return false;
        }
        QMap<QString,double> fresh;
        if(!capturePlanarFaceAnchor(result,index,&fresh,error)||
           !fresh.contains("face_anchor_witness_x"))return false;
        // Exact unchanged identity implies an unchanged surface placement; the
        // feature's existing face-local u/v coordinates remain valid.
        for(auto it=beforeAnchor.cbegin();it!=beforeAnchor.cend();++it)
            changed.parameters.remove(it.key());
        changed.parameters.insert("face_index",index);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)
            changed.parameters.insert(it.key(),it.value());
    }else{
        fail(error,"Unchanged-subshape lineage supports witnessed edges and supported faces only.");return false;
    }
    if(!validateShapeFeatureNodes({changed},error))return false;
    *rebound=std::move(changed);
    return true;
}
} // namespace cad

namespace cad {
bool supportsRecordedModifierLineage(const ShapeFeatureNode& operation){
    if(operation.suppressed)return false;
    return supportsSurvivingModifierLineage(operation)||
           operation.type.startsWith(QStringLiteral("Face normal "))||
           operation.type.startsWith(QStringLiteral("Face rectangle "))||
           operation.type.startsWith(QStringLiteral("Face slot "))||
           operation.type==QStringLiteral("Face through-all hole")||
           operation.type==QStringLiteral("Face counterbore through all")||
           operation.type==QStringLiteral("Face countersink through all")||
           operation.type==QStringLiteral("Sketch polygon face boss")||
           operation.type==QStringLiteral("Sketch polygon face pocket")||
           operation.type==QStringLiteral("Loft rectangles")||
           operation.type==QStringLiteral("Sweep circle path")||
           operation.type==QStringLiteral("Cylinder cut X")||
           operation.type==QStringLiteral("Cylinder cut Y")||
           operation.type==QStringLiteral("Cylinder cut Z");
}

bool rebaseFeatureOnCurrentStage(const TopoDS_Shape& current,
                                 const ShapeFeatureNode& feature,
                                 ShapeFeatureNode* rebound,QString* error){
    if(!rebound||current.IsNull()){
        fail(error,"Direct-stage witness needs valid current geometry.");return false;
    }
    ShapeFeatureNode changed=feature;
    if(selectedEdge(feature.type)){
        if(!feature.parameters.contains("edge_anchor_version")){
            fail(error,"An intermediate edge requires a witnessed anchor.");return false;
        }
        const auto anchor=numericAnchor(feature.parameters,"edge");
        int selected=0;QMap<QString,double> fresh;
        if(!resolveStableEdgeAnchor(current,anchor,&selected,error)||
           !captureStableEdgeAnchor(current,selected,&fresh,error))return false;
        for(auto it=anchor.cbegin();it!=anchor.cend();++it)changed.parameters.remove(it.key());
        changed.parameters.insert("edge_index",selected);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)
            changed.parameters.insert(it.key(),it.value());
    }else if(feature.type==QStringLiteral("Open-face shell")&&
             feature.parameters.contains("curved_type")){
        const auto anchor=numericAnchor(feature.parameters,"curved");
        TopoDS_Face selected;QMap<QString,double> fresh;
        QString exactError;
        if(!resolveCurvedFaceAnchor(current,anchor,&selected,&exactError)){
            if(anchor.value("curved_type")==1.0){
                if(!findDirectStageCoaxialCylinder(current,anchor,&selected,error))return false;
            }else if(anchor.value("curved_type")==4.0){
                if(!findDirectStageFilletTorus(current,anchor,&selected,error))return false;
            }else {fail(error,exactError);return false;}
        }
        const int index=faceExplorerIndex(current,selected);
        if(index<1||!captureCurvedFaceAnchor(current,index,&fresh,error))return false;
        for(auto it=anchor.cbegin();it!=anchor.cend();++it)changed.parameters.remove(it.key());
        changed.parameters.insert("face_index",index);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)
            changed.parameters.insert(it.key(),it.value());
    }else if(selectedFace(feature.type)){
        const auto anchor=numericAnchor(feature.parameters,"face");
        if(!anchor.contains("face_anchor_witness_x")){
            fail(error,"A directly attached planar feature requires an interior witness.");return false;
        }
        TopoDS_Face selected;QMap<QString,double> fresh;
        double normalShift=0.0;
        QString exactError;
        if(resolvePlanarFaceAnchor(current,anchor,&selected,&exactError)){
            const int index=faceExplorerIndex(current,selected);
            if(index<1||!capturePlanarFaceAnchor(current,index,&fresh,error))return false;
        }else if(!findDirectStageTranslatedPlanarFace(current,anchor,&selected,
                                                       &fresh,&normalShift,error)){
            // A missing witness is not a license to guess across a split or
            // to jump to a different generated cap.
            return false;
        }
        const int index=faceExplorerIndex(current,selected);
        if(index<1){fail(error,"Direct-stage planar face was not found in the output.");return false;}
        // Retain physical design placement when the current face's centroid
        // moves after a loft, sweep or other upstream parameter change.
        if(feature.parameters.contains("u")&&feature.parameters.contains("v")){
            const gp_Vec oldNormal(anchor.value("face_anchor_nx"),
                anchor.value("face_anchor_ny"),anchor.value("face_anchor_nz"));
            const gp_Vec newNormal(fresh.value("face_anchor_nx"),
                fresh.value("face_anchor_ny"),fresh.value("face_anchor_nz"));
            if(oldNormal.SquareMagnitude()<1e-12||newNormal.SquareMagnitude()<1e-12){
                fail(error,"Direct-stage face normal is degenerate.");return false;
            }
            const auto [oldX,oldY]=planarAxes(oldNormal);
            gp_Pnt design(anchor.value("face_anchor_x"),anchor.value("face_anchor_y"),
                          anchor.value("face_anchor_z"));
            design.Translate(oldX.Multiplied(feature.parameters.value("u"))+
                             oldY.Multiplied(feature.parameters.value("v")));
            // A cap may move along its normal when loft height or sweep
            // offset changes. Carry the machining center by the *verified*
            // plane displacement, never by a nearest-face heuristic.
            design.Translate(oldNormal.Multiplied(normalShift));
            const gp_Pnt center(fresh.value("face_anchor_x"),
                                fresh.value("face_anchor_y"),fresh.value("face_anchor_z"));
            const gp_Vec offset(center,design);
            if(std::abs(offset.Dot(newNormal))>1e-4){
                fail(error,"The saved machining location left the updated face plane.");return false;
            }
            if(!insideTrimmedPlanarFace(selected,design)){
                fail(error,"The saved machining center was trimmed away from its intermediate face.");
                return false;
            }
            const auto [newX,newY]=planarAxes(newNormal);
            changed.parameters.insert("u",offset.Dot(newX));
            changed.parameters.insert("v",offset.Dot(newY));
        }
        for(auto it=anchor.cbegin();it!=anchor.cend();++it)changed.parameters.remove(it.key());
        changed.parameters.insert("face_index",index);
        for(auto it=fresh.cbegin();it!=fresh.cend();++it)
            changed.parameters.insert(it.key(),it.value());
    }else{
        fail(error,"Direct intermediate reference is not a witnessed face or selected edge.");return false;
    }
    if(!validateShapeFeatureNodes({changed},error))return false;
    *rebound=std::move(changed);
    return true;
}
} // namespace cad

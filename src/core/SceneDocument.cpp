#include "SceneDocument.h"
#include "LinkedFlatDevelopmentAudit.h"
#include "FoldedContourTopology.h" // direct owner: cad::detail::ContourPoint
#include "RefoldGeometryAudit.h"   // direct owner: cad::refold::Bend/Audit/inspect
#include "DirectionalHoleSpan.h"
#include "AdaptiveMotionScan.h"
#include "JointMotionInterpolation.h"
#include "JointKeyframeRateAudit.h"
#include "DocumentAudit.h"
#include "StockContourMetrics.h"
#include "StockMaterialCompatibility.h"
#include "SimultaneousProximity.h"
#include "MotionSweptClearance.h"
#include "AssemblyFitAudit.h"
#include "LinkedBodyGraph.h"
#include "MeshRepair.h"
#include "ShapeFeatureTree.h"
#include "DrawingSheet.h" // explicit geometry-checked drawing annotation review
#include "BooleanReferenceLineage.h"
#include "BooleanLineagePath.h"
#include "BranchReferenceAgreement.h"
#include "ReferenceRouteConsensus.h"
#include "AtomicRebuildGuard.h"
#include "DrawingSourceFingerprint.h"
#include "DrawingEdgeAnchors.h"
#include "ParametricStudy.h"
#include <map>
#include <memory>
#include <cstdio>
#include <vector>

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepLib.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Builder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BSplineCurve.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_Tool.hxx>
#include <TCollection_AsciiString.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDocStd_Document.hxx>
#include <TopLoc_Location.hxx>
#include <Standard_Failure.hxx>
#include <StlAPI_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopAbs_State.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopoDS_Wire.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <NCollection_List.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <QCryptographicHash>
#include <QDir>
#include <QTemporaryDir>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_HArray1.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Mat.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>

#include <QColor>
#include <QDate>
#include <QFile>
#include <QSet>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QUuid>

#include <cmath>
#include <functional>
#include <algorithm>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cad {
namespace {
constexpr qsizetype kMaxFileBytes = 96 * 1024 * 1024;
constexpr qsizetype kMaxStepImportBytes = 512 * 1024 * 1024;
constexpr qsizetype kMaxIgesImportBytes = 512 * 1024 * 1024;
constexpr int kMaxObjects = 2000;
constexpr int kHistoryDepth = 40;
constexpr int kMaxInPlaceSketchStages = 8;
const QString kSchema1 = QStringLiteral("cadworkbench.scene.v1");
const QString kSchema2 = QStringLiteral("cadworkbench.scene.v2");
const QString kSchema3 = QStringLiteral("cadworkbench.scene.v3");
const QString kSchema4 = QStringLiteral("cadworkbench.scene.v4");
const QString kSchema5 = QStringLiteral("cadworkbench.scene.v5");
const QString kSchema6 = QStringLiteral("cadworkbench.scene.v6");
const QString kSchema7 = QStringLiteral("cadworkbench.scene.v7");
const QString kSchema8 = QStringLiteral("cadworkbench.scene.v8");
const QString kSchema9 = QStringLiteral("cadworkbench.scene.v9");
const QString kSchema10 = QStringLiteral("cadworkbench.scene.v10");
const QString kSchema11 = QStringLiteral("cadworkbench.scene.v11");
const QString kSchema12 = QStringLiteral("cadworkbench.scene.v12");
const QString kSchema13 = QStringLiteral("cadworkbench.scene.v13");
const QString kSchema14 = QStringLiteral("cadworkbench.scene.v14");
const QString kSchema15 = QStringLiteral("cadworkbench.scene.v15");
const QString kSchema16 = QStringLiteral("cadworkbench.scene.v16");
const QString kSchema17 = QStringLiteral("cadworkbench.scene.v17");
const QString kSchema18 = QStringLiteral("cadworkbench.scene.v18");
const QString kSchema19 = QStringLiteral("cadworkbench.scene.v19");
const QString kSchema20 = QStringLiteral("cadworkbench.scene.v20");
const QString kSchema21 = QStringLiteral("cadworkbench.scene.v21");
const QString kSchema22 = QStringLiteral("cadworkbench.scene.v22");
const QString kSchema23 = QStringLiteral("cadworkbench.scene.v23");
const QString kSchema24 = QStringLiteral("cadworkbench.scene.v24");
constexpr qsizetype kMaxStlImportBytes = 512 * 1024 * 1024;

// Single source of truth for the dependency model used by regeneration,
// impact previews and transactional batch-edit preflight. New derived-body
// families must be added here, not to independently copied graphs.
std::vector<linkedgraph::Node> linkedBodyGraphNodes(const QVector<Part>& parts){
    std::vector<linkedgraph::Node> nodes;
    nodes.reserve(static_cast<std::size_t>(parts.size()));
    for(const Part& part:parts){
        linkedgraph::Node node;
        node.id=part.id.toStdString();
        node.derived=part.hasLinkedBoolean||part.hasLinkedSketchExtrusion||
                     part.hasInPlaceSketchFeature||part.hasInterleavedHistory||
                     !part.linkedFoldedFlatSourceId.isEmpty();
        node.unary=part.hasLinkedSketchExtrusion||!part.linkedFoldedFlatSourceId.isEmpty()||
                   (part.hasInPlaceSketchFeature&&!part.hasInPlaceSketchFollowUp);
        if(!part.linkedFoldedFlatSourceId.isEmpty()){
            node.primary=part.linkedFoldedFlatSourceId.toStdString();
        }else if(part.hasLinkedBoolean){
            node.primary=part.linkedPrimaryId.toStdString();
            node.tool=part.linkedToolId.toStdString();
        }else if(part.hasLinkedSketchExtrusion){
            node.primary=part.linkedSketchSourceId.toStdString();
        }else if(part.hasInPlaceSketchFeature){
            node.primary=part.inPlaceSketchSourceId.toStdString();
            if(part.hasInPlaceSketchFollowUp)
                node.tool=part.inPlaceFollowSourceId.toStdString();
            for(const auto& stage:part.extraInPlaceSketchStages)
                node.additionalSources.push_back(stage.sourceId.toStdString());
        }else if(part.hasInterleavedHistory){
            QStringList sources;
            for(const auto& step:part.interleavedSteps)
                if(step.kind!=QStringLiteral("Shape")&&!sources.contains(step.sketchSourceId))
                    sources.append(step.sketchSourceId);
            if(!sources.isEmpty()){
                node.primary=sources.first().toStdString();
                node.unary=sources.size()==1;
                if(sources.size()>1)node.tool=sources[1].toStdString();
                for(int i=2;i<sources.size();++i)node.additionalSources.push_back(sources[i].toStdString());
            }else node.derived=false;
        }
        nodes.push_back(std::move(node));
    }
    return nodes;
}

bool planLinkedBodyOrder(const QVector<Part>& parts,QVector<int>* ordered,QString* error){
    const auto nodes=linkedBodyGraphNodes(parts);
    std::vector<std::size_t> planned;
    std::string reason;
    if(!linkedgraph::plan(nodes,&planned,&reason)){
        if(error)*error=QStringLiteral("Linked-body dependency: %1").arg(QString::fromStdString(reason));
        return false;
    }
    if(ordered){
        QVector<int> next;
        next.reserve(static_cast<qsizetype>(planned.size()));
        for(std::size_t index:planned)next.append(static_cast<int>(index));
        *ordered=std::move(next);
    }
    return true;
}

bool linkedBooleanResult(const TopoDS_Shape& primary,const TopoDS_Shape& tool,
                         const QString& op,TopoDS_Shape* output,QString* error,
                         std::shared_ptr<BRepAlgoAPI_BooleanOperation>* recordedHistory=nullptr){
    try{
        std::shared_ptr<BRepAlgoAPI_BooleanOperation> builder;
        if(op=="Fuse")builder=std::make_shared<BRepAlgoAPI_Fuse>(primary,tool);
        else if(op=="Cut")builder=std::make_shared<BRepAlgoAPI_Cut>(primary,tool);
        else if(op=="Common")builder=std::make_shared<BRepAlgoAPI_Common>(primary,tool);
        else{if(error)*error="Unknown linked Boolean operation.";return false;}
        builder->Build();
        if(!builder->IsDone()||builder->HasErrors()){
            if(error)*error="Linked Boolean kernel operation failed.";return false;
        }
        *output=builder->Shape();
        if(output->IsNull()||!BRepCheck_Analyzer(*output).IsValid()){
            if(error)*error="Linked Boolean generated invalid geometry.";return false;
        }
        GProp_GProps properties;BRepGProp::VolumeProperties(*output,properties);
        if(!std::isfinite(properties.Mass())||properties.Mass()<=1e-9){
            if(error)*error="Linked Boolean has no remaining solid volume.";return false;
        }
        if(recordedHistory)*recordedHistory=std::move(builder);
        return true;
    }catch(const Standard_Failure& e){if(error)*error=QStringLiteral("Linked Boolean kernel error: %1").arg(e.what());}
     catch(const std::exception& e){if(error)*error=e.what();}
    return false;
}
// A linked sketch-derived cutter must remove measurable material, both when
// first created and on *every* source edit. Without this check an upstream
// sketch move could silently turn a live pocket into an ineffective Cut.
bool linkedSketchCutEffective(const TopoDS_Shape& original,const TopoDS_Shape& cut,
                              QString* error){
    GProp_GProps before,after;
    try{
        BRepGProp::VolumeProperties(original,before);
        BRepGProp::VolumeProperties(cut,after);
    }catch(const Standard_Failure& failure){
        if(error)*error=QStringLiteral("Cannot measure sketch-driven Cut: %1").arg(failure.what());
        return false;
    }catch(const std::exception& failure){
        if(error)*error=QString::fromUtf8(failure.what());return false;
    }
    const double removed=before.Mass()-after.Mass();
    if(!std::isfinite(before.Mass())||before.Mass()<=0||!std::isfinite(removed)||
       removed<=std::max(1e-7,before.Mass()*1e-9)){
        if(error)*error="Sketch-driven pocket misses the target or removes no measurable solid volume.";
        return false;
    }
    return true;
}

// A sketch-driven boss must add measurable material and form one connected
// solid. Otherwise OpenCascade may return a valid *compound* holding a
// detached cutter, which is not a successful additive CAD feature.
bool linkedSketchFuseEffective(const TopoDS_Shape& original,const TopoDS_Shape& fused,
                               QString* error){
    try{
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(original,before);
        BRepGProp::VolumeProperties(fused,after);
        const double gained=after.Mass()-before.Mass();
        if(!std::isfinite(before.Mass())||before.Mass()<=0||
           !std::isfinite(gained)||gained<=std::max(1e-7,before.Mass()*1e-9)){
            if(error)*error="Sketch-driven boss is entirely inside the target or adds no measurable solid volume.";
            return false;
        }
        int solids=0;
        for(TopExp_Explorer explorer(fused,TopAbs_SOLID);explorer.More();explorer.Next()){
            if(++solids>1)break;
        }
        if(solids!=1){
            if(error)*error="Sketch-driven boss is detached or produces multiple solid components.";
            return false;
        }
        return true;
    }catch(const Standard_Failure& failure){
        if(error)*error=QStringLiteral("Cannot measure sketch-driven boss: %1").arg(failure.what());
    }catch(const std::exception& failure){
        if(error)*error=QString::fromUtf8(failure.what());
    }
    return false;
}

bool finitePositive(double v) { return std::isfinite(v) && v > 1e-5 && v <= 1e6; }
bool finiteOffset(double v) { return std::isfinite(v) && std::abs(v) <= 1e6; }
// Bounded directed dependency graph for positive dimension parameters only.
// Each target has one source. Recursion rejects cycles before any model change.
void linkError(QString* error,const QString& message){if(error)*error=message;}
bool linkableDimension(const QString& key) {
    return key==QStringLiteral("width")||key==QStringLiteral("height")||
           key==QStringLiteral("depth")||key==QStringLiteral("hole_diameter")||key==QStringLiteral("hole_depth")||
           key==QStringLiteral("hole_pattern_spacing");
}
bool resolveParameterLinks(const QMap<QString,ParameterLink>& links,QMap<QString,double>& values,QString* error) {
    if(links.size()>4){linkError(error,"Parameter dependency graph exceeds four links.");return false;}
    QSet<QString> visiting,finished;
    std::function<bool(const QString&)> visit=[&](const QString& target)->bool{
        if(finished.contains(target))return true;
        if(visiting.contains(target)){linkError(error,"Cyclic named parameter dependency.");return false;}
        const auto it=links.constFind(target);
        if(it==links.cend())return true;
        if(!linkableDimension(target)||!linkableDimension(it->source)||!values.contains(target)||
           !values.contains(it->source)||target==it->source||!std::isfinite(it->scale)||
           it->scale<=0.0||it->scale>1e6||!finiteOffset(it->offset)){
            linkError(error,"Unsupported named parameter dependency, scale, or offset.");return false;
        }
        visiting.insert(target);
        if(!visit(it->source))return false;
        const double result=values.value(it->source)*it->scale+it->offset;
        if(!finitePositive(result)){linkError(error,"Dependent named parameter exceeds its supported range.");return false;}
        values[target]=result;
        visiting.remove(target);finished.insert(target);return true;
    };
    for(auto it=links.cbegin();it!=links.cend();++it)if(!visit(it.key()))return false;
    return true;
}


// World placement = Rz(yaw) * Ry(pitch) * Rx(roll) about the local origin,
// followed by XYZ translation. Joint-driven poses remain stored separately.
gp_Trsf freeOccurrencePlacement(const Part& part) {
    gp_Trsf placement;
    placement.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),
                          part.placementYawDegrees*std::numbers::pi/180.0);
    gp_Trsf pitch;pitch.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,1,0)),
                                     part.placementPitchDegrees*std::numbers::pi/180.0);
    gp_Trsf roll;roll.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,0,0)),
                                   part.placementRollDegrees*std::numbers::pi/180.0);
    placement.Multiply(pitch);placement.Multiply(roll);
    placement.SetTranslationPart(gp_Vec(part.placementX,part.placementY,part.placementZ));
    return placement;
}

// A revolute drive is stored in the B-rep, not in the occurrence's XYZ
// placement. Copying or synchronizing it as a definition would bake a driven
// pose into the unposed instance. Follow rigid/prismatic ancestors as well.
bool hasDrivenRevoluteAncestor(const Part& occurrence,const QVector<Part>& parts) {
    const Part* cursor=&occurrence;
    QSet<QString> visited;
    while(cursor&&!visited.contains(cursor->id)) {
        visited.insert(cursor->id);
        if(cursor->assemblyJointType==QStringLiteral("Revolute")&&cursor->jointAngleDegrees!=0.0)return true;
        if(cursor->assemblyJointParentBodyId.isEmpty())break;
        const QString parentId=cursor->assemblyJointParentBodyId;
        const auto parent=std::find_if(parts.cbegin(),parts.cend(),[&](const Part& p){return p.id==parentId;});
        cursor=parent==parts.cend()?nullptr:&*parent;
    }
    return false;
}
bool validDrawingReadinessPolicy(const DrawingReadinessPolicy& policy,QString* error) {
    const QSet<QString> fields={QStringLiteral("description"),QStringLiteral("part_number"),QStringLiteral("revision"),
        QStringLiteral("drawing_number"),QStringLiteral("drawn_by"),QStringLiteral("checked_by"),
        QStringLiteral("issue_date"),QStringLiteral("material")};
    const QSet<QString> conventions={QStringLiteral("envelope_dimensions"),QStringLiteral("projection_symbol"),
        QStringLiteral("auxiliary_view"),QStringLiteral("general_notes"),QStringLiteral("section_view"),
        QStringLiteral("detail_view"),QStringLiteral("linked_parts_list"),QStringLiteral("bom_balloons")};
    QSet<QString> seen;
    for(const QString& field:policy.requiredTitleBlockFields){
        if(!fields.contains(field)||seen.contains(field)){if(error)*error="Drawing project policy contains an unsupported or duplicate title-block field.";return false;}
        seen.insert(field);
    }
    seen.clear();
    for(const QString& convention:policy.requiredAnnotationConventions){
        if(!conventions.contains(convention)||seen.contains(convention)){if(error)*error="Drawing project policy contains an unsupported or duplicate annotation convention.";return false;}
        seen.insert(convention);
    }
    return true;
}
// SVG is XML 1.0: validate Unicode scalar values, not individual UTF-16 units.
// A valid surrogate pair represents one allowed supplementary code point;
// an unpaired surrogate cannot be serialized as well-formed XML.
bool drawingXmlSafe(const QString& value) {
    for (qsizetype i=0;i<value.size();++i) {
        const ushort c=value.at(i).unicode();
        if (c>=0xD800 && c<=0xDBFF) {
            if (++i>=value.size()) return false;
            const ushort low=value.at(i).unicode();
            if (low<0xDC00 || low>0xDFFF) return false;
            continue;
        }
        if ((c<0x20 && c!=0x09 && c!=0x0A && c!=0x0D) ||
            (c>=0xDC00 && c<=0xDFFF) || c==0xFFFE || c==0xFFFF)
            return false;
    }
    return true;
}

// Keep this fingerprint byte-for-byte compatible with DrawingSheet.cpp's
// buildDrawingAssemblyPart(): previously saved multi-body sheets use this token.
QString drawingSourceFingerprint(QVector<const Part*> sources) {
    std::sort(sources.begin(),sources.end(),[](const Part* a,const Part* b){return a->id<b->id;});
    QByteArray identity;
    for(const Part* source:sources){
        identity.append(source->id.toUtf8());
        identity.append('|');
        identity.append(source->geometryToken.toUtf8());
        identity.append('\n');
    }
    return QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
}

// Linked parts lists also depend on source metadata, which can change without
// changing the B-rep geometry token. Hash the complete source BOM/material
// identity separately so annotation edits can preserve a stale revision.
QString drawingSourceDataFingerprint(QVector<const Part*> sources) {
    return drawingSourceMetadataFingerprint(std::move(sources));
}

bool validDrawingSettings(const DrawingSheetSettings& s,QString* error) {
    QStringList xmlStrings={s.sheetId,s.sourceGeometryToken,s.sourceDataToken,s.drawingNumber,s.drawnBy,
                            s.checkedBy,s.issueDate,s.revision,s.partsListAnnotation.id};
    xmlStrings.append(s.sourceBodyIds);
    xmlStrings.append(s.generalNotes);
    for(auto it=s.viewIds.cbegin();it!=s.viewIds.cend();++it)xmlStrings.append(it.value());
    for(const DrawingNoteAnnotation& note:s.noteAnnotations){xmlStrings.append(note.id);xmlStrings.append(note.text);}
    for(const DrawingDimensionAnnotation& dimension:s.dimensionAnnotations)xmlStrings.append(dimension.id);
    for(const DrawingEdgeDimensionAnnotation& dimension:s.edgeDimensionAnnotations){xmlStrings.append(dimension.id);xmlStrings.append(dimension.sourceBodyId);xmlStrings.append(dimension.sourceGeometryToken);}
    for(const DrawingGdtFrameAnnotation& f:s.gdtFrames){xmlStrings.append(f.id);xmlStrings.append(f.sourceBodyId);xmlStrings.append(f.sourceGeometryToken);xmlStrings.append(f.characteristic);xmlStrings.append(f.datumA);xmlStrings.append(f.datumB);xmlStrings.append(f.datumC);}
    for(const DrawingBalloonAnnotation& balloon:s.balloonAnnotations){xmlStrings.append(balloon.id);xmlStrings.append(balloon.sourceBodyId);}
    for(const DrawingDetailAnnotation& detail:s.detailAnnotations)xmlStrings.append(detail.id);
    for(const DrawingSectionAnnotation& section:s.sectionAnnotations)xmlStrings.append(section.id);
    for(const QString& value:xmlStrings)if(!drawingXmlSafe(value)){
        if(error)*error="Drawing text and persistent identities must contain XML-safe characters.";
        return false;
    }
    // XML 1.0 permits tab/newline in attributes, but an XML reader normalizes
    // them to spaces. Persisted IDs must survive an SVG metadata round trip.
    QStringList identityStrings={s.sheetId,s.sourceGeometryToken,s.sourceDataToken,s.partsListAnnotation.id};
    identityStrings.append(s.sourceBodyIds);
    for(auto it=s.viewIds.cbegin();it!=s.viewIds.cend();++it)identityStrings.append(it.value());
    for(const DrawingNoteAnnotation& note:s.noteAnnotations)identityStrings.append(note.id);
    for(const DrawingDimensionAnnotation& dimension:s.dimensionAnnotations)identityStrings.append(dimension.id);
    for(const DrawingEdgeDimensionAnnotation& dimension:s.edgeDimensionAnnotations){identityStrings.append(dimension.id);identityStrings.append(dimension.sourceBodyId);identityStrings.append(dimension.sourceGeometryToken);}
    for(const DrawingGdtFrameAnnotation& f:s.gdtFrames){identityStrings.append(f.id);identityStrings.append(f.sourceBodyId);identityStrings.append(f.sourceGeometryToken);}
    for(const DrawingBalloonAnnotation& balloon:s.balloonAnnotations){identityStrings.append(balloon.id);identityStrings.append(balloon.sourceBodyId);}
    for(const DrawingDetailAnnotation& detail:s.detailAnnotations)identityStrings.append(detail.id);
    for(const DrawingSectionAnnotation& section:s.sectionAnnotations)identityStrings.append(section.id);
    for(const QString& value:identityStrings)if(value.contains(QLatin1Char('\t'))||
            value.contains(QLatin1Char('\n'))||value.contains(QLatin1Char('\r'))){
        if(error)*error="Persistent drawing identities cannot contain XML-normalized whitespace.";
        return false;
    }
    // The SVG stores the source set as comma-separated data, so allowing a
    // literal comma in a source ID would make its membership ambiguous.
    for(const QString& sourceId:s.sourceBodyIds)if(sourceId.contains(QLatin1Char(','))){
        if(error)*error="Drawing source body IDs cannot contain commas.";
        return false;
    }

    const bool projection=s.projection=="First-angle"||s.projection=="Third-angle";
    const bool sheet=s.sheetSize=="A4"||s.sheetSize=="A3";
    const bool auxiliary=s.auxiliaryView=="Isometric"||s.auxiliaryView=="Section A-A";
    if(!projection||!sheet||!auxiliary||!std::isfinite(s.sectionDepthFraction)||s.sectionDepthFraction<0.01||s.sectionDepthFraction>0.99){if(error)*error="Invalid drawing projection, sheet, auxiliary view, or section depth.";return false;}
    if(s.generalNotes.size()>5){if(error)*error="Drawing settings allow up to five general notes.";return false;}
    for(const QString& note:s.generalNotes)if(note.trimmed().isEmpty()||note.size()>60||note.contains('\n')||note.contains('\r')){if(error)*error="Each drawing note must contain 1 to 60 single-line characters.";return false;}
    if(s.drawingNumber.size()>32||s.drawnBy.size()>40||s.checkedBy.size()>40||s.revision.size()>16||s.issueDate.size()>20){if(error)*error="Drawing title-block text exceeds its allowed length.";return false;}
    for(const QString* value:{&s.drawingNumber,&s.drawnBy,&s.checkedBy,&s.revision,&s.issueDate})for(QChar c:*value)if(c.category()==QChar::Other_Control){if(error)*error="Drawing title-block fields cannot contain control characters.";return false;}
    if(!s.issueDate.isEmpty()){const QDate date=QDate::fromString(s.issueDate,Qt::ISODate);if(!date.isValid()||date.toString(Qt::ISODate)!=s.issueDate){if(error)*error="Drawing issue date must use YYYY-MM-DD format.";return false;}}
    const QSet<QString> viewNames={QStringLiteral("TOP"),QStringLiteral("FRONT"),QStringLiteral("RIGHT"),QStringLiteral("AUXILIARY")};
    QSet<QString> uniqueIds;
    for(auto it=s.viewIds.cbegin();it!=s.viewIds.cend();++it){
        if(!viewNames.contains(it.key())||it.value().trimmed().isEmpty()||it.value().size()>64||uniqueIds.contains(it.value())){
            if(error)*error="Drawing view identities must be unique and use a supported view name.";return false;
        }
        uniqueIds.insert(it.value());
    }
    const double maxX=s.sheetSize=="A3"?420.0:297.0,maxY=s.sheetSize=="A3"?297.0:210.0;
    for(auto it=s.viewCenters.cbegin();it!=s.viewCenters.cend();++it){
        const QPointF point=it.value();
        if(!viewNames.contains(it.key())||!std::isfinite(point.x())||!std::isfinite(point.y())||
           point.x()<10.0||point.x()>maxX-10.0||point.y()<10.0||point.y()>maxY-10.0){
            if(error)*error="Drawing view centers must use supported views and remain inside the sheet border.";return false;
        }
    }
    if(s.sheetId.trimmed().isEmpty()||s.sheetId.size()>64||s.sourceGeometryToken.trimmed().isEmpty()||s.sourceGeometryToken.size()>64||
       (!s.sourceDataToken.isEmpty()&&s.sourceDataToken.size()>64)||
       std::any_of(s.sheetId.cbegin(),s.sheetId.cend(),[](QChar c){return c.category()==QChar::Other_Control;})||
       std::any_of(s.sourceGeometryToken.cbegin(),s.sourceGeometryToken.cend(),[](QChar c){return c.category()==QChar::Other_Control;})){
        if(error)*error="Invalid persistent drawing sheet identity or source revision.";return false;
    }
    if(s.sourceBodyIds.size()>50){if(error)*error="A drawing sheet can reference at most 50 source bodies.";return false;}
    QSet<QString> sourceIds;
    for(const QString& id:s.sourceBodyIds){
        if(id.trimmed().isEmpty()||id.size()>64||id.contains('\n')||id.contains('\r')||sourceIds.contains(id)){
            if(error)*error="Drawing source body identities must be unique and valid.";return false;
        }
        sourceIds.insert(id);
    }
    if(!s.sheetId.isEmpty()&&(s.viewIds.size()!=4||s.viewIds.size()!=uniqueIds.size())){
        if(error)*error="A persistent drawing sheet must retain stable identities for all four views.";return false;
    }
    if(s.noteAnnotations.size()>5||s.noteAnnotations.size()!=s.generalNotes.size()){
        if(error)*error="A drawing sheet can contain at most five persisted notes and each note must have a matching text entry.";return false;
    }
    QSet<QString> annotationIds;
    for(int i=0;i<s.noteAnnotations.size();++i){
        const DrawingNoteAnnotation& note=s.noteAnnotations[i];
        if(note.id.trimmed().isEmpty()||note.id.size()>64||uniqueIds.contains(note.id)||annotationIds.contains(note.id)||
           note.text.trimmed().isEmpty()||note.text.size()>60||
           std::any_of(note.text.cbegin(),note.text.cend(),[](QChar c){return c.category()==QChar::Other_Control;})||
           !std::isfinite(note.xMm)||!std::isfinite(note.yMm)||note.xMm<10||note.xMm>maxX-10||note.yMm<10||note.yMm>maxY-10||
           note.text!=s.generalNotes[i]){
            if(error)*error="Drawing notes require unique IDs, matching single-line text, and paper positions inside the sheet.";return false;
        }
        annotationIds.insert(note.id);
    }
    if(!s.dimensionAnnotations.isEmpty()&&s.dimensionAnnotations.size()!=3){if(error)*error="A drawing sheet must contain three persistent overall dimensions (X, Y, Z).";return false;}
    QSet<QString> dimensionAxes;
    for(const DrawingDimensionAnnotation& dimension:s.dimensionAnnotations){
        if((dimension.axis!="X"&&dimension.axis!="Y"&&dimension.axis!="Z")||dimension.id.trimmed().isEmpty()||dimension.id.size()>64||
           uniqueIds.contains(dimension.id)||annotationIds.contains(dimension.id)||dimensionAxes.contains(dimension.axis)||
           !std::isfinite(dimension.offsetMm)||dimension.offsetMm<5.0||dimension.offsetMm>60.0){
            if(error)*error="Persistent drawing dimensions require unique IDs, X/Y/Z axes, and offsets from 5 to 60 mm.";return false;
        }
        dimensionAxes.insert(dimension.axis);annotationIds.insert(dimension.id);
    }
    if(!s.dimensionAnnotations.isEmpty()&&dimensionAxes.size()!=3){if(error)*error="Persistent drawing dimensions must include X, Y, and Z.";return false;}
    if(s.edgeDimensionAnnotations.size()>20){if(error)*error="A drawing sheet can contain at most 20 selected-edge dimensions.";return false;}
    for(const DrawingEdgeDimensionAnnotation& dimension:s.edgeDimensionAnnotations){
        if(dimension.id.trimmed().isEmpty()||dimension.id.size()>64||uniqueIds.contains(dimension.id)||annotationIds.contains(dimension.id)||
           !sourceIds.contains(dimension.sourceBodyId)||dimension.sourceGeometryToken.trimmed().isEmpty()||dimension.sourceGeometryToken.size()>64||
           dimension.edgeIndex<1||!QSet<QString>{"TOP","FRONT","RIGHT"}.contains(dimension.view)||
           (!dimension.edgeAnchor.isEmpty()&&!validDrawingEdgeWitness(dimension.edgeAnchor))||
           !QSet<QString>{"LENGTH","DIAMETER","CENTER_DISTANCE","CENTER_X","CENTER_Y","CENTER_Z"}.contains(dimension.kind)||
           (dimension.kind!="CENTER_DISTANCE"&&(dimension.secondEdgeIndex!=0||!dimension.secondEdgeAnchor.isEmpty()))||
           ((dimension.kind=="CENTER_Z"&&dimension.view=="TOP")||
            (dimension.kind=="CENTER_Y"&&dimension.view=="FRONT")||
            (dimension.kind=="CENTER_X"&&dimension.view=="RIGHT"))||
           (dimension.kind=="CENTER_DISTANCE"&&(dimension.secondEdgeIndex<1||
                !validDrawingEdgeWitness(dimension.edgeAnchor)||!validDrawingEdgeWitness(dimension.secondEdgeAnchor)))||
           (dimension.kind!="LENGTH"&&dimension.kind!="CENTER_DISTANCE"&&!validDrawingEdgeWitness(dimension.edgeAnchor))||
           !std::isfinite(dimension.upperToleranceMm)||!std::isfinite(dimension.lowerToleranceMm)||
           dimension.upperToleranceMm<0||dimension.lowerToleranceMm<0||
           dimension.upperToleranceMm>1000||dimension.lowerToleranceMm>1000||
           !std::isfinite(dimension.offsetMm)||dimension.offsetMm<5.0||dimension.offsetMm>60.0){
            if(error)*error="Selected-edge dimensions need a unique ID, drawing source body, edge index, orthographic view, and 5-60 mm offset.";return false;
        }
        annotationIds.insert(dimension.id);
    }
    if(s.gdtFrames.size()>8){if(error)*error="At most eight saved geometric tolerance frames are supported.";return false;}
    const QSet<QString> gdtKinds={"POSITION","FLATNESS","PARALLELISM","PERPENDICULARITY","PROFILE","CIRCULARITY"};
    for(const DrawingGdtFrameAnnotation& f:s.gdtFrames){
        const auto goodDatum=[](const QString& d){return d.isEmpty()||(d.size()==1&&d.front()>=QLatin1Char('A')&&d.front()<=QLatin1Char('Z'));};
        if(f.id.trimmed().isEmpty()||f.id.size()>64||uniqueIds.contains(f.id)||annotationIds.contains(f.id)||
           !sourceIds.contains(f.sourceBodyId)||f.sourceGeometryToken.trimmed().isEmpty()||f.sourceGeometryToken.size()>64||
           !gdtKinds.contains(f.characteristic)||!std::isfinite(f.toleranceMm)||f.toleranceMm<=0||f.toleranceMm>1000||
           !goodDatum(f.datumA)||!goodDatum(f.datumB)||!goodDatum(f.datumC)||
           (!f.datumB.isEmpty()&&f.datumA.isEmpty())||(!f.datumC.isEmpty()&&f.datumB.isEmpty())||
           (!f.datumA.isEmpty()&&(f.datumA==f.datumB||f.datumA==f.datumC))||
           (!f.datumB.isEmpty()&&f.datumB==f.datumC)||
           !std::isfinite(f.xMm)||!std::isfinite(f.yMm)||
           f.xMm<10||f.xMm>maxX-75||f.yMm<20||f.yMm>maxY-12){
            if(error)*error="Invalid source-referenced manual geometric tolerance frame.";return false;
        }
        annotationIds.insert(f.id);
    }
    const DrawingPartsListAnnotation& partsList=s.partsListAnnotation;
    if(partsList.id.trimmed().isEmpty()||partsList.id.size()>64||uniqueIds.contains(partsList.id)||annotationIds.contains(partsList.id)||
       !std::isfinite(partsList.xMm)||!std::isfinite(partsList.yMm)||partsList.xMm<10.0||partsList.xMm>maxX-155.0||
       partsList.yMm<10.0||partsList.yMm>maxY-108.0){
        if(error)*error="A linked parts-list identity and placement must remain inside the drawing sheet.";return false;
    }
    if(s.balloonAnnotations.size()>50){if(error)*error="A drawing sheet can contain at most 50 BOM balloons.";return false;}
    annotationIds.insert(partsList.id);
    for(const DrawingBalloonAnnotation& balloon:s.balloonAnnotations){
        if(balloon.id.trimmed().isEmpty()||balloon.id.size()>64||uniqueIds.contains(balloon.id)||annotationIds.contains(balloon.id)||
           balloon.sourceBodyId.trimmed().isEmpty()||!sourceIds.contains(balloon.sourceBodyId)||
           !std::isfinite(balloon.xMm)||!std::isfinite(balloon.yMm)||balloon.xMm<8.0||balloon.xMm>maxX-8.0||
           balloon.yMm<8.0||balloon.yMm>maxY-8.0){
            if(error)*error="Drawing BOM balloons require unique IDs, source bodies on the sheet, and positions inside the border.";return false;
        }
        annotationIds.insert(balloon.id);
    }
    if(s.detailAnnotations.size()>4){if(error)*error="A drawing sheet can contain at most four detail views.";return false;}
    const QSet<QString> sourceViews={QStringLiteral("TOP"),QStringLiteral("FRONT"),QStringLiteral("RIGHT")};
    for(const DrawingDetailAnnotation& detail:s.detailAnnotations){
        if(detail.id.trimmed().isEmpty()||detail.id.size()>64||uniqueIds.contains(detail.id)||annotationIds.contains(detail.id)||
           !sourceViews.contains(detail.sourceView)||!std::isfinite(detail.sourceXmm)||!std::isfinite(detail.sourceYmm)||
           std::abs(detail.sourceXmm)>1e6||std::abs(detail.sourceYmm)>1e6||!std::isfinite(detail.captureRadiusMm)||
           detail.captureRadiusMm<0.1||detail.captureRadiusMm>1e6||!std::isfinite(detail.xMm)||!std::isfinite(detail.yMm)||
           !std::isfinite(detail.radiusMm)||detail.radiusMm<8.0||detail.radiusMm>40.0||
           detail.xMm-detail.radiusMm<10.0||detail.xMm+detail.radiusMm>maxX-10.0||
           detail.yMm-detail.radiusMm<10.0||detail.yMm+detail.radiusMm>maxY-10.0){
            if(error)*error="Drawing detail views require unique IDs, a projected source view, valid capture data, and an on-sheet circular placement.";return false;
        }
        annotationIds.insert(detail.id);
    }
    if(s.sectionAnnotations.size()>4){if(error)*error="A drawing sheet can contain at most four separately placed section views.";return false;}
    for(const DrawingSectionAnnotation& section:s.sectionAnnotations){
        if(section.id.trimmed().isEmpty()||section.id.size()>64||uniqueIds.contains(section.id)||annotationIds.contains(section.id)||
           (section.planeAxis!="X"&&section.planeAxis!="Y"&&section.planeAxis!="Z")||
           !std::isfinite(section.depthFraction)||section.depthFraction<0.01||section.depthFraction>0.99||
           !std::isfinite(section.xMm)||!std::isfinite(section.yMm)||section.xMm<20.0||section.xMm>maxX-20.0||
           section.yMm<30.0||section.yMm>maxY-30.0){
            if(error)*error="Section views require unique IDs, a cut depth from 1 to 99 percent, and an on-sheet center.";return false;
        }
        annotationIds.insert(section.id);
    }
    return true;
}
void inheritMaterial(Part& destination, const Part& source) {
    destination.materialName = source.materialName;
    destination.materialDensityKgPerM3 = source.materialDensityKgPerM3;
    destination.materialEngineering = source.materialEngineering;
    destination.partNumber = source.partNumber;
    destination.description = source.description;
    destination.revision = source.revision;
    destination.assemblyPath = source.assemblyPath;
    destination.procurementType = source.procurementType;
    destination.supplier = source.supplier;
    destination.supplierPartNumber = source.supplierPartNumber;
}
QString generatedId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
const char* primitiveName(Primitive kind) {
    switch(kind) {
        case Primitive::Box: return "Box";
        case Primitive::Cylinder: return "Cylinder";
        case Primitive::Sphere: return "Sphere";
        case Primitive::Cone: return "Cone";
        case Primitive::Torus: return "Torus";
        case Primitive::RegularPrism: return "Regular Prism";
        case Primitive::Capsule: return "Capsule";
        case Primitive::Ellipsoid: return "Ellipsoid";
        case Primitive::Tube: return "Tube";
        case Primitive::Wedge: return "Wedge";
        case Primitive::PyramidFrustum: return "Pyramid Frustum";
        case Primitive::HexNut: return "Hex Nut";
    }
    return "Solid";
}
QString brepToText(const TopoDS_Shape& shape) {
    std::ostringstream out;
    BRepTools::Write(shape, out);
    if (!out.good()) throw std::runtime_error("B-rep serialization failed");
    return QString::fromLatin1(QByteArray::fromStdString(out.str()).toBase64());
}
TopoDS_Shape brepFromText(const QString& encoded) {
    const QByteArray bytes = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.isEmpty()) throw std::runtime_error("Invalid or empty B-rep payload");
    std::istringstream in(bytes.toStdString());
    TopoDS_Shape shape;
    BRep_Builder builder;
    BRepTools::Read(shape, in, builder);
    if (shape.IsNull() || in.bad()) throw std::runtime_error("Failed to parse B-rep payload");
    return shape;
}
bool isHexColor(const QString& text) {
    if (text.size() != 7 || text[0] != QLatin1Char('#')) return false;
    for (int i = 1; i < 7; ++i) {
        const ushort c = text[i].unicode();
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}
bool hasSolid(const TopoDS_Shape& shape) {
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) return true;
    return false;
}
bool validSurfaceBodyShape(const TopoDS_Shape& shape,QString* error) {
    if(shape.IsNull()||(shape.ShapeType()!=TopAbs_FACE&&shape.ShapeType()!=TopAbs_SHELL)){
        if(error)*error="A surface body must be one valid face or shell.";
        return false;
    }
    if(!BRepCheck_Analyzer(shape).IsValid()){
        if(error)*error="Surface body failed OCCT topology validation.";
        return false;
    }
    GProp_GProps area;
    BRepGProp::SurfaceProperties(shape,area);
    if(!std::isfinite(area.Mass())||area.Mass()<=1e-9){
        if(error)*error="Surface body must have finite, non-zero area.";
        return false;
    }
    return true;
}
bool rejectSurfaceOnlyInput(const Part& part,const QString& operation,QString* error) {
    if(!part.isSurfaceBody)return false;
    if(error)*error=QStringLiteral("%1 requires a solid body. Surface bodies can be transformed or renamed, but this operation does not edit surfaces.").arg(operation);
    return true;
}
TopoDS_Shape makeFoldedSheetSolid(const QVector<double>& flanges,double width,double thickness,
                                  const QVector<FoldedSheetBend>& bends,QString* error) {
    QVector<QPointF> outline;
    if(!buildFoldedSheetOutline(flanges,width,thickness,bends,&outline,error))return {};
    BRepBuilderAPI_MakePolygon boundary;
    for(const QPointF& point:outline)boundary.Add(gp_Pnt(point.x(),point.y(),0.0));
    boundary.Close();
    if(!boundary.IsDone()){if(error)*error="Could not close the folded sheet cross-section.";return {};}
    BRepBuilderAPI_MakeFace face(boundary.Wire());
    if(!face.IsDone()){if(error)*error="The folded sheet profile is self-intersecting or has invalid thickness.";return {};}
    // Do not request Shape() from an unchecked OCCT extrusion builder. An
    // unsuccessful prism can otherwise propagate an invalid topology handle
    // into volume inspection, deep-copy checkpoints, and scene persistence.
    BRepPrimAPI_MakePrism extrusion(face.Face(),gp_Vec(0.0,0.0,width));
    if(!extrusion.IsDone()){
        if(error)*error="OCCT could not extrude the folded sheet cross-section into a solid.";
        return {};
    }
    const TopoDS_Shape solid=extrusion.Shape();
    if(solid.IsNull()){
        if(error)*error="OCCT returned a null folded sheet solid.";
        return {};
    }
    return solid;
}
} // namespace

void SceneDocument::setError(QString* error, const QString& text) { if (error) *error = text; }

namespace {
MeshImportSummary summarizeMeshRepair(const mesh::RepairStats& stats) {
    MeshImportSummary summary;
    summary.inputVertices=static_cast<quint64>(stats.inputVertices);
    summary.outputVertices=static_cast<quint64>(stats.outputVertices);
    summary.inputTriangles=static_cast<quint64>(stats.inputTriangles);
    summary.outputTriangles=static_cast<quint64>(stats.outputTriangles);
    summary.weldedVertices=static_cast<quint64>(stats.weldedVertices);
    summary.removedDegenerateTriangles=static_cast<quint64>(stats.removedDegenerateTriangles);
    summary.removedDuplicateTriangles=static_cast<quint64>(stats.removedDuplicateTriangles);
    summary.reorientedTriangles=static_cast<quint64>(stats.flippedTriangles);
    summary.filledHoles=static_cast<quint64>(stats.filledHoles);
    summary.filledHoleTriangles=static_cast<quint64>(stats.filledHoleTriangles);
    summary.boundaryEdges=static_cast<quint64>(stats.boundaryEdges);
    summary.nonManifoldEdges=static_cast<quint64>(stats.nonManifoldEdges);
    return summary;
}
}

bool SceneDocument::validShape(const TopoDS_Shape& shape, QString* error) {
    if (shape.IsNull()) { setError(error, "The kernel returned a null shape."); return false; }
    if (!hasSolid(shape)) { setError(error, "A modeling operation must produce at least one solid."); return false; }
    if (!BRepCheck_Analyzer(shape).IsValid()) {
        setError(error, "Geometry failed OCCT topology validation."); return false;
    }
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    if (!std::isfinite(props.Mass()) || props.Mass() <= 1e-12) {
        setError(error, "Solid has zero or invalid volume."); return false;
    }
    return true;
}

namespace {
bool rebuildParametricSolid(const Part& part,const QMap<QString,double>& parameters,
                            const QStringList& suppressed,TopoDS_Shape* result,QString* error) {
    if(!result){if(error)*error="Provide an output shape for parametric rebuild.";return false;}
    const double originX=parameters.value(QStringLiteral("origin_x"));
    const double originY=parameters.value(QStringLiteral("origin_y"));
    const double width=parameters.value(QStringLiteral("width"));
    const double height=parameters.value(QStringLiteral("height"));
    const double depth=parameters.value(QStringLiteral("depth"));
    if(!finiteOffset(originX+width)||!finiteOffset(originY+height)){
        if(error)*error="Sketch rectangle coordinates exceed the supported range.";return false;
    }
    const bool hasBlindHole=part.featureHistory.contains(QStringLiteral("Cut: Parametric blind hole"));
    const bool hasThroughHole=part.featureHistory.contains(QStringLiteral("Cut: Parametric through-hole"));
    const bool hasHole=hasBlindHole||hasThroughHole;
    const bool hasPattern=part.featureHistory.contains(QStringLiteral("Pattern: Linear through-hole array along X"))||
                          part.featureHistory.contains(QStringLiteral("Pattern: Linear through-hole array along Y"))||
                          part.featureHistory.contains(QStringLiteral("Pattern: Linear blind-hole array along X"))||
                          part.featureHistory.contains(QStringLiteral("Pattern: Linear blind-hole array along Y"));
    const bool hasChamfer=part.featureHistory.contains(QStringLiteral("Chamfer: All edges"));
    const bool hasFillet=part.featureHistory.contains(QStringLiteral("Fillet: All edges"));
    const QString holeSuppressionId=hasBlindHole?QStringLiteral("blind_hole"):QStringLiteral("through_hole");
    const bool activeHole=hasHole&&!suppressed.contains(holeSuppressionId);
    const bool activePattern=activeHole&&hasPattern&&!suppressed.contains(QStringLiteral("linear_hole_pattern"));
    const bool activeChamfer=hasChamfer&&!suppressed.contains(QStringLiteral("edge_chamfer"));
    const bool activeFillet=hasFillet&&!suppressed.contains(QStringLiteral("edge_fillet"));
    TopoDS_Shape rebuilt=BRepPrimAPI_MakeBox(gp_Pnt(originX,originY,0),
        gp_Pnt(originX+width,originY+height,depth)).Shape();
    // A suppressed optional cut must not invalidate the remaining base solid
    // merely because its archived hole center no longer fits a resized body.
    // Re-enabling the cut revalidates the preserved original design parameters.
    if(activeHole){
        const double diameter=parameters.value(QStringLiteral("hole_diameter"));
        const double holeDepth=hasBlindHole?parameters.value(QStringLiteral("hole_depth")):depth;
        const double radius=diameter/2.0;
        const double x=parameters.value(QStringLiteral("hole_x"));
        const double y=parameters.value(QStringLiteral("hole_y"));
        const int fullPatternCount=hasPattern?static_cast<int>(parameters.value(QStringLiteral("hole_pattern_count"))):1;
        const double spacing=parameters.value(QStringLiteral("hole_pattern_spacing"));
        const bool alongX=part.holePatternAxis==QStringLiteral("X");
        const double lastX=x+(hasPattern&&alongX?spacing*(fullPatternCount-1):0.0);
        const double lastY=y+(hasPattern&&!alongX?spacing*(fullPatternCount-1):0.0);
        const double minimumFloor=std::max(0.01,depth*1e-5);
        if(!finitePositive(diameter)||(hasBlindHole&&(!finitePositive(holeDepth)||holeDepth>=depth-minimumFloor))||
           x-radius<=originX||y-radius<=originY||
           lastX+radius>=originX+width||lastY+radius>=originY+height||
           (hasPattern&&spacing<diameter)){
            if(error)*error="The parametric through-hole feature would leave the rectangular profile or overlap its pattern.";
            return false;
        }
        if(activeHole){
        const int count=activePattern?fullPatternCount:1;
        BRep_Builder builder;TopoDS_Compound cutters;builder.MakeCompound(cutters);
        for(int i=0;i<count;++i){
            const double holeX=x+(activePattern&&alongX?spacing*i:0.0);
            const double holeY=y+(activePattern&&!alongX?spacing*i:0.0);
            const double startZ=hasBlindHole?depth-holeDepth:-0.01;
            const double cutterDepth=hasBlindHole?holeDepth+0.01:depth+0.02;
            builder.Add(cutters,BRepPrimAPI_MakeCylinder(
                gp_Ax2(gp_Pnt(holeX,holeY,startZ),gp_Dir(0,0,1)),radius,cutterDepth).Shape());
        }
        BRepAlgoAPI_Cut cut(rebuilt,cutters);cut.Build();
        if(!cut.IsDone()||cut.HasErrors()){
            if(error)*error="The parametric through-hole rebuild failed.";return false;
        }
        rebuilt=cut.Shape();
        }
    }
    if(activeChamfer){
        const double distance=parameters.value(QStringLiteral("chamfer_distance"));
        if(!finitePositive(distance)||distance>=0.5*std::min({width,height,depth})){
            if(error)*error="The parametric all-edge chamfer must be positive and smaller than half the smallest base dimension.";
            return false;
        }
        BRepFilletAPI_MakeChamfer chamfer(rebuilt);
        int edgeCount=0;
        for(TopExp_Explorer edges(rebuilt,TopAbs_EDGE);edges.More();edges.Next()){
            chamfer.Add(distance,TopoDS::Edge(edges.Current()));++edgeCount;
        }
        if(edgeCount==0){if(error)*error="The parametric chamfer found no solid edges.";return false;}
        chamfer.Build();
        if(!chamfer.IsDone()){
            if(error)*error="The parametric distance cannot be applied to all current solid edges.";
            return false;
        }
        rebuilt=chamfer.Shape();
    }
    if(activeFillet){
        const double radius=parameters.value(QStringLiteral("fillet_radius"));
        if(!finitePositive(radius)||radius>=0.5*std::min({width,height,depth})){
            if(error)*error="The parametric all-edge fillet must be positive and smaller than half the smallest base dimension.";
            return false;
        }
        BRepFilletAPI_MakeFillet fillet(rebuilt);
        int edgeCount=0;
        for(TopExp_Explorer edges(rebuilt,TopAbs_EDGE);edges.More();edges.Next()){
            fillet.Add(radius,TopoDS::Edge(edges.Current()));++edgeCount;
        }
        if(edgeCount==0){if(error)*error="The parametric fillet found no solid edges.";return false;}
        fillet.Build();
        if(!fillet.IsDone()){
            if(error)*error="The parametric radius cannot be applied to all current solid edges.";
            return false;
        }
        rebuilt=fillet.Shape();
    }
    if(rebuilt.IsNull()||!hasSolid(rebuilt)||!BRepCheck_Analyzer(rebuilt).IsValid()){
        if(error)*error="Parametric feature rebuild produced invalid solid topology.";return false;
    }
    GProp_GProps volume;BRepGProp::VolumeProperties(rebuilt,volume);
    if(!std::isfinite(volume.Mass())||volume.Mass()<=1e-12){
        if(error)*error="Parametric feature rebuild produced zero or invalid volume.";return false;
    }
    *result=rebuilt;return true;
}
}

const Part* SceneDocument::find(const QString& id) const {
    for (const Part& part : parts_) if (part.id == id) return &part;
    return nullptr;
}

QString SceneDocument::uniqueName(const QString& base) const {
    QString candidate = base;
    int suffix = 2;
    bool occupied = true;
    while (occupied) {
        occupied = false;
        for (const auto& part : parts_) if (part.name.compare(candidate, Qt::CaseInsensitive) == 0) {
            occupied = true;
            candidate = base + QStringLiteral(" %1").arg(suffix++);
            break;
        }
    }
    return candidate;
}

void SceneDocument::checkpoint() {
    const bool traceFolded=qEnvironmentVariableIsSet("CAD_V170_FOLDED_TRACE")&&
        std::any_of(parts_.cbegin(),parts_.cend(),[](const Part& part){return part.hasFoldedSheetMetalDefinition;});
    if(traceFolded){std::fprintf(stderr,"[folded checkpoint] scene copy: before\n");std::fflush(stderr);}
    QVector<Part> snapshotParts = parts_;
    if(traceFolded){std::fprintf(stderr,"[folded checkpoint] scene copy: after; parts=%lld\n",
                  static_cast<long long>(snapshotParts.size()));std::fflush(stderr);}
    for (auto& part : snapshotParts){
        if(traceFolded){std::fprintf(stderr,"[folded checkpoint] B-rep deep copy: before\n");std::fflush(stderr);}
        part.shape = BRepBuilderAPI_Copy(part.shape).Shape();
        if(traceFolded){std::fprintf(stderr,"[folded checkpoint] B-rep deep copy: after\n");std::fflush(stderr);}
    }
    undo_.append({std::move(snapshotParts), drawingReadinessPolicy_, revision_,
                  liveRigidMateFixed_, liveRigidMates_,
                  liveArticulatedFixed_,liveArticulatedJoints_,liveArticulatedReferencePoses_});
    if (undo_.size() > kHistoryDepth) undo_.removeFirst();
    redo_.clear();
    revision_ = nextRevision_++;
}

bool SceneDocument::setDrawingReadinessPolicy(const DrawingReadinessPolicy& policy,QString* error) {
    if(!validDrawingReadinessPolicy(policy,error))return false;
    if(policy.requiredTitleBlockFields==drawingReadinessPolicy_.requiredTitleBlockFields&&
       policy.requiredAnnotationConventions==drawingReadinessPolicy_.requiredAnnotationConventions)return true;
    checkpoint();drawingReadinessPolicy_=policy;return true;
}

bool SceneDocument::addPrimitive(Primitive kind, const QVector<double>& dimensions, QString* error) {
    int required = 0;
    switch(kind) {
        case Primitive::Box: required = 3; break;
        case Primitive::Cylinder: required = 2; break;
        case Primitive::Sphere: required = 1; break;
        case Primitive::Cone: required = 3; break;
        case Primitive::Torus: required = 2; break;
        case Primitive::RegularPrism: required = 3; break; // circumradius, height, side count
        case Primitive::Capsule: required = 2; break; // radius, distance between cap centers
        case Primitive::Ellipsoid: required = 3; break; // X/Y/Z radii
        case Primitive::Tube: required = 3; break; // outer radius, inner radius, height
        case Primitive::Wedge: required = 4; break; // width, depth, height, top width
        case Primitive::PyramidFrustum: required = 5; break; // base X/Y, top X/Y, height
        case Primitive::HexNut: required = 3; break; // outer radius, bore radius, height
    }
    if (dimensions.size() != required) { setError(error, "Wrong number of dimensions."); return false; }
    for (double v : dimensions) if (!finitePositive(v)) {
        setError(error, "Dimensions must be finite and between 0.00001 and 1,000,000 mm."); return false;
    }
    if (kind == Primitive::Torus && dimensions[0] <= dimensions[1]) {
        setError(error, "The torus major radius must exceed its tube radius."); return false;
    }
    if (kind == Primitive::RegularPrism &&
        (dimensions[2] < 3 || dimensions[2] > 64 || std::floor(dimensions[2]) != dimensions[2])) {
        setError(error, "A regular prism needs an integer side count from 3 through 64."); return false;
    }
    if (kind == Primitive::Tube && dimensions[1] >= dimensions[0]) {
        setError(error, "The tube inner radius must be smaller than its outer radius."); return false;
    }
    if (kind == Primitive::Wedge && dimensions[3] > dimensions[0]) {
        setError(error, "The wedge top width must not exceed its base width."); return false;
    }
    if (kind == Primitive::PyramidFrustum &&
        (dimensions[2] > dimensions[0] || dimensions[3] > dimensions[1])) {
        setError(error, "Pyramid-frustum top dimensions must not exceed its base dimensions."); return false;
    }
    if (kind == Primitive::HexNut && dimensions[1] >= dimensions[0] * std::cos(std::numbers::pi / 6.0)) {
        setError(error, "The hex-nut bore must be smaller than the hexagon's inradius."); return false;
    }
    if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
    try {
        TopoDS_Shape shape;
        switch (kind) {
            case Primitive::Box: shape = BRepPrimAPI_MakeBox(dimensions[0], dimensions[1], dimensions[2]).Shape(); break;
            case Primitive::Cylinder: shape = BRepPrimAPI_MakeCylinder(dimensions[0], dimensions[1]).Shape(); break;
            case Primitive::Sphere: shape = BRepPrimAPI_MakeSphere(dimensions[0]).Shape(); break;
            case Primitive::Cone: shape = BRepPrimAPI_MakeCone(dimensions[0], dimensions[1], dimensions[2]).Shape(); break;
            case Primitive::Torus: shape = BRepPrimAPI_MakeTorus(dimensions[0], dimensions[1]).Shape(); break;
            case Primitive::RegularPrism: {
                BRepBuilderAPI_MakePolygon polygon;
                const int sides = static_cast<int>(dimensions[2]);
                for (int i = 0; i < sides; ++i) {
                    const double angle = 2.0 * std::numbers::pi * i / sides;
                    polygon.Add(gp_Pnt(dimensions[0] * std::cos(angle),
                                       dimensions[0] * std::sin(angle), 0.0));
                }
                polygon.Close();
                const TopoDS_Face base = BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
                shape = BRepPrimAPI_MakePrism(base, gp_Vec(0, 0, dimensions[1])).Shape();
                break;
            }
            case Primitive::Capsule: {
                const double radius = dimensions[0], centerSpan = dimensions[1];
                const TopoDS_Shape shaft = BRepPrimAPI_MakeCylinder(radius, centerSpan).Shape();
                const TopoDS_Shape lower = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), radius).Shape();
                const TopoDS_Shape upper = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, centerSpan), radius).Shape();
                BRepAlgoAPI_Fuse first(shaft, lower);
                first.Build();
                if (!first.IsDone()) { setError(error, "Capsule lower cap could not be fused."); return false; }
                BRepAlgoAPI_Fuse second(first.Shape(), upper);
                second.Build();
                if (!second.IsDone()) { setError(error, "Capsule upper cap could not be fused."); return false; }
                shape = second.Shape();
                break;
            }
            case Primitive::Ellipsoid: {
                const gp_GTrsf transform(
                    gp_Mat(dimensions[0], 0, 0, 0, dimensions[1], 0, 0, 0, dimensions[2]),
                    gp_XYZ(0, 0, 0));
                shape = BRepBuilderAPI_GTransform(BRepPrimAPI_MakeSphere(1.0).Shape(), transform, false).Shape();
                break;
            }
            case Primitive::Tube: {
                const TopoDS_Shape outer = BRepPrimAPI_MakeCylinder(dimensions[0], dimensions[2]).Shape();
                const TopoDS_Shape inner = BRepPrimAPI_MakeCylinder(dimensions[1], dimensions[2]).Shape();
                BRepAlgoAPI_Cut cut(outer, inner);
                cut.Build();
                if (!cut.IsDone()) { setError(error, "Tube bore could not be cut."); return false; }
                shape = cut.Shape();
                break;
            }
            case Primitive::Wedge: {
                BRepBuilderAPI_MakePolygon polygon;
                polygon.Add(gp_Pnt(0, 0, 0));
                polygon.Add(gp_Pnt(dimensions[0], 0, 0));
                polygon.Add(gp_Pnt(dimensions[3], 0, dimensions[2]));
                polygon.Add(gp_Pnt(0, 0, dimensions[2]));
                polygon.Close();
                const TopoDS_Face profile = BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
                shape = BRepPrimAPI_MakePrism(profile, gp_Vec(0, dimensions[1], 0)).Shape();
                break;
            }
            case Primitive::PyramidFrustum: {
                BRepBuilderAPI_MakePolygon base, top;
                base.Add(gp_Pnt(0,0,0)); base.Add(gp_Pnt(dimensions[0],0,0));
                base.Add(gp_Pnt(dimensions[0],dimensions[1],0)); base.Add(gp_Pnt(0,dimensions[1],0)); base.Close();
                const double xOffset=(dimensions[0]-dimensions[2])/2.0;
                const double yOffset=(dimensions[1]-dimensions[3])/2.0;
                top.Add(gp_Pnt(xOffset,yOffset,dimensions[4]));
                top.Add(gp_Pnt(xOffset+dimensions[2],yOffset,dimensions[4]));
                top.Add(gp_Pnt(xOffset+dimensions[2],yOffset+dimensions[3],dimensions[4]));
                top.Add(gp_Pnt(xOffset,yOffset+dimensions[3],dimensions[4])); top.Close();
                BRepOffsetAPI_ThruSections loft(true,true);
                loft.AddWire(base.Wire()); loft.AddWire(top.Wire()); loft.Build();
                if (!loft.IsDone()) { setError(error,"Pyramid frustum loft failed."); return false; }
                shape=loft.Shape();
                break;
            }
            case Primitive::HexNut: {
                BRepBuilderAPI_MakePolygon boundary;
                for (int i=0;i<6;++i) {
                    const double angle=2.0*std::numbers::pi*i/6.0;
                    boundary.Add(gp_Pnt(dimensions[0]*std::cos(angle),dimensions[0]*std::sin(angle),0));
                }
                boundary.Close();
                const TopoDS_Face base=BRepBuilderAPI_MakeFace(boundary.Wire()).Face();
                const TopoDS_Shape blank=BRepPrimAPI_MakePrism(base,gp_Vec(0,0,dimensions[2])).Shape();
                const TopoDS_Shape bore=BRepPrimAPI_MakeCylinder(dimensions[1],dimensions[2]).Shape();
                BRepAlgoAPI_Cut cut(blank,bore); cut.Build();
                if (!cut.IsDone()) { setError(error,"Hex-nut bore could not be cut."); return false; }
                shape=cut.Shape();
                break;
            }
        }
        if (!validShape(shape, error)) return false;
        const QString name = uniqueName(QString::fromLatin1(primitiveName(kind)));
        checkpoint();
        parts_.append({generatedId(), name, shape});
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::addSurfaceBody(const QString& name,const TopoDS_Shape& shape,QString* error) {
    const QString cleanName=name.trimmed();
    if(cleanName.isEmpty()||cleanName.size()>120||cleanName.contains('\n')||cleanName.contains('\r')){
        setError(error,"Surface body names must contain 1 to 120 single-line characters.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    for(const Part& part:parts_)if(part.name.compare(cleanName,Qt::CaseInsensitive)==0){
        setError(error,"Another body already uses this name.");return false;
    }
    if(!validSurfaceBodyShape(shape,error))return false;
    Part part;part.id=generatedId();part.name=cleanName;part.shape=shape;part.isSurfaceBody=true;
    checkpoint();parts_.append(std::move(part));return true;
}

bool SceneDocument::thickenSurfaceBody(const QString& surfaceBodyId,double thicknessMm,QString* error) {
    const Part* source=find(surfaceBodyId);
    if(!source||!source->isSurfaceBody){setError(error,"Select a persisted surface body to thicken.");return false;}
    if(!std::isfinite(thicknessMm)||std::abs(thicknessMm)<1e-5||std::abs(thicknessMm)>1e6){
        setError(error,"Surface thickness must be finite, non-zero, and no greater than 1,000,000 mm in magnitude.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        TopoDS_Shape solid;
        TopExp_Explorer faceExplorer(source->shape,TopAbs_FACE);
        const bool singleFace=faceExplorer.More() && ([&](){faceExplorer.Next();return !faceExplorer.More();})();
        if(singleFace && BRepAdaptor_Surface(TopoDS::Face(TopExp_Explorer(source->shape,TopAbs_FACE).Current()),true).GetType()==GeomAbs_Plane){
            const TopoDS_Face face=TopoDS::Face(TopExp_Explorer(source->shape,TopAbs_FACE).Current());
            const gp_Dir normal=BRepAdaptor_Surface(face,true).Plane().Axis().Direction();
            BRepPrimAPI_MakePrism prism(face,gp_Vec(normal)*thicknessMm,true);
            if(!prism.IsDone()){setError(error,"OCCT could not extrude this planar surface into a solid.");return false;}
            solid=prism.Shape();
        }else{
            BRepOffsetAPI_MakeThickSolid thickener;
            thickener.MakeThickSolidBySimple(source->shape,thicknessMm);
            if(!thickener.IsDone()){setError(error,"OCCT could not thicken this surface without invalid or self-intersecting geometry.");return false;}
            solid=thickener.Shape();
        }
        int solidCount=0;for(TopExp_Explorer explorer(solid,TopAbs_SOLID);explorer.More();explorer.Next())++solidCount;
        if(solidCount!=1||!validShape(solid,error)){
            if(error&&error->isEmpty())setError(error,"Thickening must produce exactly one valid solid.");
            return false;
        }
        Part result{generatedId(),uniqueName(source->name+QStringLiteral(" Thickened")),solid,source->visible,source->color};
        result.materialName=source->materialName;result.materialDensityKgPerM3=source->materialDensityKgPerM3;
        result.materialEngineering=source->materialEngineering;
        checkpoint();parts_.append(std::move(result));return true;
    }catch(const Standard_Failure& failure){setError(error,QStringLiteral("Surface-thickening kernel error: %1").arg(failure.what()));}
     catch(const std::exception& failure){setError(error,QStringLiteral("Surface thickening failed: %1").arg(QString::fromUtf8(failure.what())));}
    return false;
}

bool SceneDocument::stitchSurfaceBodies(const QStringList& surfaceBodyIds,double toleranceMm,QString* error) {
    if(surfaceBodyIds.isEmpty()||surfaceBodyIds.size()>100){setError(error,"Select between one and 100 surface bodies to stitch.");return false;}
    if(!std::isfinite(toleranceMm)||toleranceMm<1e-7||toleranceMm>1.0){setError(error,"Stitch tolerance must be finite and between 0.0000001 and 1 mm.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    QSet<QString> unique;QVector<const Part*> sources;sources.reserve(surfaceBodyIds.size());
    for(const QString& id:surfaceBodyIds){
        if(unique.contains(id)){setError(error,"A surface body was selected more than once.");return false;}
        unique.insert(id);const Part* part=find(id);
        if(!part||!part->isSurfaceBody){setError(error,"Every selected body must be a persisted surface body.");return false;}
        sources.append(part);
    }
    try{
        BRepBuilderAPI_Sewing sewing(toleranceMm,true,true,true,false);
        for(const Part* source:sources)sewing.Add(source->shape);
        sewing.Perform();const TopoDS_Shape stitched=sewing.SewedShape();
        if(stitched.IsNull()){setError(error,"OCCT could not sew the selected surface bodies.");return false;}
        BRep_Builder builder;TopoDS_Compound solids;builder.MakeCompound(solids);int solidCount=0;
        for(TopExp_Explorer explorer(stitched,TopAbs_SOLID);explorer.More();explorer.Next()){
            const TopoDS_Shape solid=explorer.Current();if(!validShape(solid,error))return false;
            builder.Add(solids,solid);++solidCount;
        }
        if(solidCount==0){
            for(TopExp_Explorer explorer(stitched,TopAbs_SHELL);explorer.More();explorer.Next()){
                const TopoDS_Shell shell=TopoDS::Shell(explorer.Current());
                if(!BRep_Tool::IsClosed(shell)){setError(error,"The stitched surfaces still have open boundary edges; no solid was created.");return false;}
                BRepBuilderAPI_MakeSolid makeSolid(shell);
                if(!makeSolid.IsDone()){setError(error,"The closed stitched shell could not be converted to a solid.");return false;}
                TopoDS_Solid solid=makeSolid.Solid();
                if(!BRepLib::OrientClosedSolid(solid)||!validShape(solid,error))return false;
                builder.Add(solids,solid);++solidCount;
            }
        }
        if(solidCount==0||!validShape(solids,error)){
            if(error&&error->isEmpty())setError(error,"Stitching did not produce a closed valid solid.");
            return false;
        }
        Part result{generatedId(),uniqueName(QStringLiteral("Stitched Surface Solid")),solids,true,sources.first()->color};
        result.materialName=sources.first()->materialName;
        result.materialDensityKgPerM3=sources.first()->materialDensityKgPerM3;
        result.materialEngineering=sources.first()->materialEngineering;
        checkpoint();parts_.append(std::move(result));return true;
    }catch(const Standard_Failure& failure){setError(error,QStringLiteral("Surface stitching kernel error: %1").arg(failure.what()));}
     catch(const std::exception& failure){setError(error,QStringLiteral("Surface stitching failed: %1").arg(QString::fromUtf8(failure.what())));}
    return false;
}

bool SceneDocument::sewSurfaceBodies(const QStringList& surfaceBodyIds,double toleranceMm,QString* error) {
    if(surfaceBodyIds.size()<2||surfaceBodyIds.size()>100){
        setError(error,"Select between two and 100 surface bodies to merge into a shell.");return false;
    }
    if(!std::isfinite(toleranceMm)||toleranceMm<1e-7||toleranceMm>1.0){
        setError(error,"Sewing tolerance must be finite and between 0.0000001 and 1 mm.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    QSet<QString> unique;QVector<const Part*> sources;sources.reserve(surfaceBodyIds.size());
    for(const QString& id:surfaceBodyIds){
        if(unique.contains(id)){setError(error,"A surface body was selected more than once.");return false;}
        unique.insert(id);const Part* part=find(id);
        if(!part||!part->isSurfaceBody){setError(error,"Every selected body must be a persisted surface body.");return false;}
        sources.append(part);
    }
    try{
        BRepBuilderAPI_Sewing sewing(toleranceMm,true,true,true,false);
        NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> inputFaces;
        for(const Part* source:sources){
            sewing.Add(source->shape);
            TopExp::MapShapes(source->shape,TopAbs_FACE,inputFaces);
        }
        sewing.Perform();
        const TopoDS_Shape sewn=sewing.SewedShape();
        if(sewn.IsNull()){setError(error,"The CAD kernel could not sew the selected surface bodies.");return false;}
        QVector<TopoDS_Shape> shells;
        for(TopExp_Explorer explorer(sewn,TopAbs_SHELL);explorer.More();explorer.Next())
            shells.append(explorer.Current());
        if(shells.size()!=1){
            setError(error,"The selected surfaces did not sew into one connected shell. Check that their boundary edges meet within the chosen tolerance.");return false;
        }
        NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> sewnFaces;
        TopExp::MapShapes(shells.first(),TopAbs_FACE,sewnFaces);
        if(sewnFaces.Extent()!=inputFaces.Extent()){
            setError(error,"Sewing would omit or merge selected faces; no surface body was created.");return false;
        }
        if(!validSurfaceBodyShape(shells.first(),error))return false;
        Part result{generatedId(),uniqueName(QStringLiteral("Sewn Surface Shell")),shells.first(),true,sources.first()->color};
        result.isSurfaceBody=true;
        result.materialName=sources.first()->materialName;
        result.materialDensityKgPerM3=sources.first()->materialDensityKgPerM3;
        result.materialEngineering=sources.first()->materialEngineering;
        checkpoint();parts_.append(std::move(result));return true;
    }catch(const Standard_Failure& failure){setError(error,QStringLiteral("Surface-shell sewing kernel error: %1").arg(failure.what()));}
     catch(const std::exception& failure){setError(error,QStringLiteral("Surface-shell sewing failed: %1").arg(QString::fromUtf8(failure.what())));}
    return false;
}

bool SceneDocument::trimSurfaceBody(const QString& surfaceBodyId,const QString& cutterSolidId,
                                    bool keepInsideCutter,QString* error) {
    const Part* source=find(surfaceBodyId);
    const Part* cutter=find(cutterSolidId);
    if(!source||!source->isSurfaceBody){setError(error,"Select a persisted surface body to trim.");return false;}
    if(!cutter||cutter->isSurfaceBody||!hasSolid(cutter->shape)){
        setError(error,"Select a solid body as the trimming tool.");return false;
    }
    if(surfaceBodyId==cutterSolidId){setError(error,"The surface and trimming tool must be different bodies.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        TopoDS_Shape booleanResult;
        if(keepInsideCutter){
            BRepAlgoAPI_Common common(source->shape,cutter->shape);common.Build();
            if(!common.IsDone()){setError(error,"The CAD kernel could not intersect the surface with the trimming solid.");return false;}
            booleanResult=common.Shape();
        }else{
            BRepAlgoAPI_Cut cut(source->shape,cutter->shape);cut.Build();
            if(!cut.IsDone()){setError(error,"The CAD kernel could not subtract the trimming solid from the surface.");return false;}
            booleanResult=cut.Shape();
        }
        if(booleanResult.IsNull()){setError(error,"The trimming operation produced no surface area.");return false;}

        BRepBuilderAPI_Sewing sewing(1e-7,true,true,true,false);
        int faceCount=0;
        for(TopExp_Explorer faces(booleanResult,TopAbs_FACE);faces.More();faces.Next()){
            sewing.Add(faces.Current());++faceCount;
        }
        if(faceCount==0){setError(error,"The trimming operation did not leave any surface faces.");return false;}
        sewing.Perform();
        const TopoDS_Shape sewn=sewing.SewedShape();
        if(sewn.IsNull()){setError(error,"The trimmed faces could not be joined into valid surface bodies.");return false;}

        QVector<TopoDS_Shape> surfacePieces;
        for(TopExp_Explorer shells(sewn,TopAbs_SHELL);shells.More();shells.Next())
            surfacePieces.append(shells.Current());
        if(surfacePieces.isEmpty()){
            if(sewn.ShapeType()==TopAbs_FACE)surfacePieces.append(sewn);
            else for(TopExp_Explorer faces(sewn,TopAbs_FACE);faces.More();faces.Next())
                surfacePieces.append(faces.Current());
        }
        if(surfacePieces.isEmpty()){setError(error,"The trimmed result contains no valid surface body.");return false;}

        GProp_GProps originalArea;BRepGProp::SurfaceProperties(source->shape,originalArea);
        double resultArea=0.0;
        for(const TopoDS_Shape& piece:surfacePieces){
            if(!validSurfaceBodyShape(piece,error))return false;
            GProp_GProps pieceArea;BRepGProp::SurfaceProperties(piece,pieceArea);resultArea+=pieceArea.Mass();
        }
        if(!std::isfinite(resultArea)||resultArea<=1e-9){setError(error,"The trimmed result has no finite surface area.");return false;}
        if(std::abs(resultArea-originalArea.Mass())<=std::max(1e-8,originalArea.Mass()*1e-9)){
            setError(error,"The trimming solid did not change the selected surface.");return false;
        }
        if(parts_.size()+surfacePieces.size()>kMaxObjects){setError(error,"Trimming would exceed the scene body limit.");return false;}

        QSet<QString> reservedNames;
        for(const Part& part:parts_)reservedNames.insert(part.name.toCaseFolded());
        QVector<Part> results;results.reserve(surfacePieces.size());
        const QString baseName=source->name.left(100).trimmed()+QStringLiteral(" Trimmed");
        for(int i=0;i<surfacePieces.size();++i){
            QString name=surfacePieces.size()==1?baseName:baseName+QStringLiteral(" %1").arg(i+1);
            const QString stem=name;int suffix=2;
            while(reservedNames.contains(name.toCaseFolded()))name=stem+QStringLiteral(" %1").arg(suffix++);
            reservedNames.insert(name.toCaseFolded());
            Part result{generatedId(),name,surfacePieces[i],source->visible,source->color};
            result.isSurfaceBody=true;
            result.materialName=source->materialName;
            result.materialDensityKgPerM3=source->materialDensityKgPerM3;
            result.materialEngineering=source->materialEngineering;
            results.append(std::move(result));
        }
        checkpoint();
        for(Part& result:results)parts_.append(std::move(result));
        return true;
    }catch(const Standard_Failure& failure){setError(error,QStringLiteral("Surface-trimming kernel error: %1").arg(failure.what()));}
     catch(const std::exception& failure){setError(error,QStringLiteral("Surface trimming failed: %1").arg(QString::fromUtf8(failure.what())));}
    return false;
}

bool SceneDocument::splitSurfaceBodyBySolid(const QString& surfaceBodyId,const QString& cutterSolidId,QString* error) {
    const Part* source=find(surfaceBodyId);
    const Part* cutter=find(cutterSolidId);
    if(!source||!source->isSurfaceBody){setError(error,"Select a persisted surface body to split.");return false;}
    if(!validSurfaceBodyShape(source->shape,error))return false;
    if(!cutter||cutter->isSurfaceBody||!hasSolid(cutter->shape)){
        setError(error,"Select a solid body as the splitting tool.");return false;
    }
    if(surfaceBodyId==cutterSolidId){setError(error,"The surface and splitting tool must be different bodies.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        QVector<TopoDS_Shape> sourceFaces;
        for(TopExp_Explorer faces(source->shape,TopAbs_FACE);faces.More();faces.Next())
            sourceFaces.append(faces.Current());
        if(sourceFaces.isEmpty()){setError(error,"The surface body contains no faces to split.");return false;}
        if(sourceFaces.size()>5000){
            setError(error,"Solid-guided splitting is limited to surface bodies with at most 5,000 faces to keep the operation responsive.");return false;
        }
        QVector<TopoDS_Shape> faceFragments;
        for(const TopoDS_Shape& sourceFace:sourceFaces){
            NCollection_List<TopoDS_Shape> arguments,tools;
            arguments.Append(sourceFace);tools.Append(cutter->shape);
            BRepAlgoAPI_Splitter splitter;
            splitter.SetArguments(arguments);splitter.SetTools(tools);splitter.Build();
            if(!splitter.IsDone()){
                setError(error,"The CAD kernel could not split every face of the surface body.");return false;
            }
            const TopoDS_Shape splitResult=splitter.Shape();
            if(splitResult.IsNull()){setError(error,"The split operation produced no surface fragments.");return false;}
            if(splitResult.ShapeType()==TopAbs_FACE)faceFragments.append(splitResult);
            else for(TopExp_Explorer faces(splitResult,TopAbs_FACE);faces.More();faces.Next())
                faceFragments.append(faces.Current());
        }
        if(faceFragments.size()<=sourceFaces.size()){
            setError(error,"The solid did not divide the selected surface into multiple fragments.");return false;
        }
        // Persist every split face independently. This accepts whole sewn
        // shells and leaves each result face directly selectable for reuse.
        QVector<TopoDS_Shape> fragments=std::move(faceFragments);
        if(fragments.size()<2){
            setError(error,QStringLiteral("The solid did not divide the selected surface into multiple fragments (%1 source faces, %2 split faces, %3 grouped bodies).")
                .arg(sourceFaces.size()).arg(faceFragments.size()).arg(fragments.size()));return false;
        }
        GProp_GProps originalArea;BRepGProp::SurfaceProperties(source->shape,originalArea);
        double fragmentArea=0.0;
        for(const TopoDS_Shape& fragment:fragments){
            if(!validSurfaceBodyShape(fragment,error))return false;
            GProp_GProps area;BRepGProp::SurfaceProperties(fragment,area);fragmentArea+=area.Mass();
        }
        const double areaTolerance=std::max(1e-7,originalArea.Mass()*1e-7);
        if(!std::isfinite(fragmentArea)||std::abs(fragmentArea-originalArea.Mass())>areaTolerance){
            setError(error,QStringLiteral("The split fragments failed surface-area conservation (%1 mm² from %2 mm² across %3 fragments); the scene was not changed.")
                .arg(fragmentArea,0,'g',12).arg(originalArea.Mass(),0,'g',12).arg(fragments.size()));return false;
        }
        if(parts_.size()+fragments.size()>kMaxObjects){setError(error,"Splitting would exceed the scene body limit.");return false;}

        QSet<QString> reservedNames;
        for(const Part& part:parts_)reservedNames.insert(part.name.toCaseFolded());
        QVector<Part> results;results.reserve(fragments.size());
        const QString baseName=source->name.left(90).trimmed()+QStringLiteral(" Split");
        for(int i=0;i<fragments.size();++i){
            const QString stem=baseName+QStringLiteral(" %1").arg(i+1);
            QString name=stem;int suffix=2;
            while(reservedNames.contains(name.toCaseFolded()))name=stem+QStringLiteral(" %1").arg(suffix++);
            reservedNames.insert(name.toCaseFolded());
            Part result{generatedId(),name,fragments[i],source->visible,source->color};
            result.isSurfaceBody=true;
            result.materialName=source->materialName;
            result.materialDensityKgPerM3=source->materialDensityKgPerM3;
            result.materialEngineering=source->materialEngineering;
            results.append(std::move(result));
        }
        checkpoint();
        for(Part& result:results)parts_.append(std::move(result));
        return true;
    }catch(const Standard_Failure& failure){setError(error,QStringLiteral("Surface-splitting kernel error: %1").arg(failure.what()));}
     catch(const std::exception& failure){setError(error,QStringLiteral("Surface splitting failed: %1").arg(QString::fromUtf8(failure.what())));}
    return false;
}

// Build a linked flat solid from the same validated bend stations and end reliefs
// used in manufacturing DXF/SVG. This is still a neutral flat development, not
// bend compensation, springback simulation, or production-certified unfolding.
namespace {
bool createLinkedFlatBlank(const Part& folded,const QVector<double>& requestedK,
                           const SheetMetalRelief& relief,TopoDS_Shape* blank,QString* error){
    if(!blank||!folded.hasFoldedSheetMetalDefinition||
       folded.geometryToken!=folded.foldedSheetGeometryToken){
        if(error)*error="Linked flat blank requires a current folded-sheet source.";return false;
    }
    const QVector<double> k=requestedK.isEmpty()?
        QVector<double>(folded.foldedSheetBends.size(),0.33):requestedK;
    // DXF's station precision, clipping and relief-overlap checks form the
    // common geometry contract for both exports and the derived 3-D solid.
    QByteArray referenceDxf;MultiBendFlatPattern pattern;
    if(!buildFoldedSheetFlatPatternDxf(folded.foldedSheetFlangesMm,
            folded.foldedSheetWidthMm,folded.foldedSheetThicknessMm,
            folded.foldedSheetBends,k,relief,&referenceDxf,&pattern,error))return false;
    try{
        const double width=folded.foldedSheetWidthMm;
        const double thickness=folded.foldedSheetThicknessMm;
        TopoDS_Shape candidate=BRepPrimAPI_MakeBox(pattern.flatLengthMm,width,thickness).Shape();
        if(relief.widthMm>0.0){
            // Each station receives a matching pair of through-thickness
            // rectangular edge reliefs. Cutting tools extend beyond the blank
            // in Y/Z to avoid a coincident exterior face on the Boolean tool.
            for(double station:pattern.bendLinePositionsMm){
                const double left=station-relief.widthMm/2.0;
                const double right=station+relief.widthMm/2.0;
                const TopoDS_Shape lower=BRepPrimAPI_MakeBox(
                    gp_Pnt(left,-thickness,-thickness),
                    gp_Pnt(right,relief.depthMm,2.0*thickness)).Shape();
                const TopoDS_Shape upper=BRepPrimAPI_MakeBox(
                    gp_Pnt(left,width-relief.depthMm,-thickness),
                    gp_Pnt(right,width+thickness,2.0*thickness)).Shape();
                for(const TopoDS_Shape& tool:{lower,upper}){
                    BRepAlgoAPI_Cut cut(candidate,tool);cut.Build();
                    if(!cut.IsDone()||cut.Shape().IsNull()||!BRepCheck_Analyzer(cut.Shape()).IsValid()){
                        if(error)*error="Linked flat relief Boolean produced an invalid solid.";
                        return false;
                    }
                    candidate=cut.Shape();
                }
            }
        }
        if(candidate.IsNull()||!BRepCheck_Analyzer(candidate).IsValid()){
            if(error)*error="Linked developed blank failed B-rep topology validation.";
            return false;
        }
        GProp_GProps measured;BRepGProp::VolumeProperties(candidate,measured);
        const double expected=(pattern.flatLengthMm*width-
            2.0*pattern.bendLinePositionsMm.size()*relief.widthMm*relief.depthMm)*thickness;
        if(!std::isfinite(measured.Mass())||expected<=1e-9||
           std::abs(measured.Mass()-expected)>std::max(1e-6,expected*1e-7)){
            if(error)*error="Linked flat blank volume differs from the validated relief development.";
            return false;
        }
        *blank=candidate;return true;
    }catch(const Standard_Failure& e){
        if(error)*error=QStringLiteral("Linked developed blank kernel failed: %1").arg(e.what());
    }
    return false;
}
}

bool SceneDocument::addLinkedFoldedFlatBlank(const QString& foldedBodyId,QString* error){
    return addLinkedFoldedFlatBlank(foldedBodyId,{},SheetMetalRelief{},error);
}

bool SceneDocument::addLinkedFoldedFlatBlank(const QString& foldedBodyId,
        const QVector<double>& kFactors,const SheetMetalRelief& relief,QString* error){
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    const Part* source=find(foldedBodyId);
    if(!source){setError(error,"Select a saved folded sheet-metal body.");return false;}
    TopoDS_Shape blank;
    if(!createLinkedFlatBlank(*source,kFactors,relief,&blank,error))return false;
    Part derived{generatedId(),uniqueName("Linked Flat Blank"),blank,true,source->color};
    derived.linkedFoldedFlatSourceId=foldedBodyId;
    derived.linkedFoldedFlatSourceToken=source->geometryToken;
    derived.linkedFoldedFlatKFactors=kFactors;
    derived.linkedFoldedFlatRelief=relief;
    derived.materialName=source->materialName;
    derived.materialDensityKgPerM3=source->materialDensityKgPerM3;
    derived.materialEngineering=source->materialEngineering;
    checkpoint();parts_.append(std::move(derived));
    if(error)error->clear();return true;
}

bool SceneDocument::editLinkedFoldedFlatDevelopment(const QString& flatBodyId,
        const QVector<double>& kFactors,const SheetMetalRelief& relief,QString* error){
    const Part* existing=find(flatBodyId);
    if(!existing||existing->linkedFoldedFlatSourceId.isEmpty()){
        setError(error,"Select a linked folded flat blank to edit its development.");return false;
    }
    const Part* source=find(existing->linkedFoldedFlatSourceId);
    if(!source||source->geometryToken!=existing->linkedFoldedFlatSourceToken){
        setError(error,"Linked flat source is missing or out of date.");return false;
    }
    TopoDS_Shape rebuilt;
    if(!createLinkedFlatBlank(*source,kFactors,relief,&rebuilt,error))return false;
    if(existing->linkedFoldedFlatKFactors==kFactors&&
       existing->linkedFoldedFlatRelief.widthMm==relief.widthMm&&
       existing->linkedFoldedFlatRelief.depthMm==relief.depthMm){
        if(error)error->clear();return true; // An unchanged edit is not a revision.
    }
    QVector<Part> staged=parts_;
    for(Part& part:staged)if(part.id==flatBodyId){
        part.shape=rebuilt;
        part.geometryToken=generatedId();
        part.linkedFoldedFlatKFactors=kFactors;
        part.linkedFoldedFlatRelief=relief;
        break;
    }
    if(!propagateLinkedBodies(&staged,error))return false;
    checkpoint();parts_=std::move(staged);
    if(error)error->clear();return true;
}

bool SceneDocument::addFoldedSheetMetal(const QVector<double>& flanges,double sheetWidth,double thickness,
        const QVector<FoldedSheetBend>& bends,QString* error) {
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=makeFoldedSheetSolid(flanges,sheetWidth,thickness,bends,error);
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Folded Sheet Metal"));
        checkpoint();parts_.append({generatedId(),name,shape,true,QStringLiteral("#8ba6b8")});
        Part& part=parts_.last();part.hasFoldedSheetMetalDefinition=true;part.foldedSheetGeometryToken=part.geometryToken;
        part.foldedSheetFlangesMm=flanges;part.foldedSheetWidthMm=sheetWidth;
        part.foldedSheetThicknessMm=thickness;part.foldedSheetBends=bends;
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Folded sheet-metal kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::foldedSheetMetalDefinition(const QString& partId,QVector<double>* flanges,
        double* width,double* thickness,QVector<FoldedSheetBend>* bends,bool* current) const {
    const Part* part=find(partId);
    if(!part||!part->hasFoldedSheetMetalDefinition)return false;
    if(flanges)*flanges=part->foldedSheetFlangesMm;
    if(width)*width=part->foldedSheetWidthMm;
    if(thickness)*thickness=part->foldedSheetThicknessMm;
    if(bends)*bends=part->foldedSheetBends;
    if(current)*current=part->geometryToken==part->foldedSheetGeometryToken;
    return true;
}

bool SceneDocument::previewFoldedSheetMetalEdit(const QString& partId,
        const QVector<double>& flanges,double width,double thickness,
        const QVector<FoldedSheetBend>& bends,double* solidVolumeMm3,
        double* neutralFlatLengthMm,QString* error) const {
    if(!solidVolumeMm3||!neutralFlatLengthMm){
        setError(error,"Folded-body preview needs volume and flat-length outputs.");return false;
    }
    const Part* part=find(partId);
    if(!part||!part->hasFoldedSheetMetalDefinition||
       part->geometryToken!=part->foldedSheetGeometryToken){
        setError(error,"Folded-body preview requires a current editable folded definition.");return false;
    }
    try{
        // Use the same production solid builder as a committed edit, without
        // checkpointing or changing any source body, history or drawing link.
        const TopoDS_Shape candidate=makeFoldedSheetSolid(flanges,width,thickness,bends,error);
        if(!validShape(candidate,error))return false;
        GProp_GProps measured;
        BRepGProp::VolumeProperties(candidate,measured);
        const double volume=measured.Mass();
        if(!std::isfinite(volume)||volume<=1e-9){
            setError(error,"The proposed folded solid has invalid measured volume.");return false;
        }
        MultiBendFlatPattern flat;
        if(!calculateFoldedSheetFlatPattern(flanges,width,thickness,bends,0.33,&flat,error))return false;
        if(!std::isfinite(flat.flatLengthMm)||flat.flatLengthMm<=0){
            setError(error,"The proposed neutral development is invalid.");return false;
        }
        // Commit both numeric outputs together only after all validation.
        *solidVolumeMm3=volume;
        *neutralFlatLengthMm=flat.flatLengthMm;
        if(error)error->clear();
        return true;
    }catch(const Standard_Failure& e){
        setError(error,QStringLiteral("Folded-body preview failed in OCCT: %1").arg(e.what()));
    }catch(const std::exception& e){
        setError(error,QStringLiteral("Folded-body preview failed: %1").arg(QString::fromUtf8(e.what())));
    }
    return false;
}

bool SceneDocument::auditFoldedSheetRefoldGeometry(const QString& partId,
        QByteArray* csv,QString* error) const{
    if(!csv){setError(error,"Provide an output buffer for the refold geometry audit.");return false;}
    const Part* source=find(partId);
    if(!source||!source->hasFoldedSheetMetalDefinition||
       source->geometryToken!=source->foldedSheetGeometryToken||source->shape.IsNull()){
        setError(error,"The folded sheet source is missing, stale or not an OCCT solid.");return false;
    }
    QVector<QPointF> outline;
    QString issue;
    if(!buildFoldedSheetOutline(source->foldedSheetFlangesMm,
            source->foldedSheetWidthMm,source->foldedSheetThicknessMm,
            source->foldedSheetBends,&outline,&issue)){
        setError(error,issue);return false;
    }
    std::vector<detail::ContourPoint> contour;contour.reserve(outline.size());
    for(const auto& point:outline)contour.push_back({point.x(),point.y()});
    std::vector<double> straight(source->foldedSheetFlangesMm.cbegin(),
                                 source->foldedSheetFlangesMm.cend());
    std::vector<refold::Bend> bends;
    for(const auto& bend:source->foldedSheetBends)
        bends.push_back({bend.angleDegrees,bend.insideRadiusMm});
    refold::Audit audit;
    std::string geometryIssue;
    if(!refold::inspect(straight,source->foldedSheetThicknessMm,
            source->foldedSheetWidthMm,bends,contour,&audit,&geometryIssue)){
        setError(error,QString::fromStdString(geometryIssue));return false;
    }
    try{
        GProp_GProps storedProperties,reproducedProperties;
        BRepGProp::VolumeProperties(source->shape,storedProperties);
        const TopoDS_Shape reproduced=makeFoldedSheetSolid(source->foldedSheetFlangesMm,source->foldedSheetWidthMm,
            source->foldedSheetThicknessMm,source->foldedSheetBends,&issue);
        if(reproduced.IsNull()||!validShape(reproduced,&issue)){
            setError(error,QStringLiteral("Refold regeneration failed: %1").arg(issue));return false;
        }
        BRepGProp::VolumeProperties(reproduced,reproducedProperties);
        const double storedVolume=storedProperties.Mass();
        const double rebuiltVolume=reproducedProperties.Mass();
        const double reference=audit.predictedSolidVolumeMm3;
        if(!std::isfinite(storedVolume)||!std::isfinite(rebuiltVolume)||
           storedVolume<=0||rebuiltVolume<=0||
           std::abs(storedVolume-rebuiltVolume)>std::max(1e-4,rebuiltVolume*1e-6)||
           std::abs(rebuiltVolume-reference)>std::max(1e-4,reference*1e-6)){
            setError(error,"Stored folded OCCT volume differs from regenerated refold geometry.");return false;
        }
        const QString content=QStringLiteral(
            "CADWorkbench independent refold geometry review,NOT_ISSUED\r\n"
            "Body,Flanges,Bends,Direction reversals,Mid-surface flat length mm,"
            "Ideal cross-section area mm2,Faceted cross-section area mm2,"
            "Relative area error,Regenerated OCCT volume mm3,Stored OCCT volume mm3\r\n")+
            QStringLiteral("REFOLD,%1,%2,%3,%4,%5,%6,%7,%8,%9\r\n")
                .arg(source->foldedSheetFlangesMm.size())
                .arg(audit.bendCount).arg(audit.reversedBendCount)
                .arg(audit.exactMidSurfaceDevelopedLengthMm,0,'g',16)
                .arg(audit.exactCrossSectionAreaMm2,0,'g',16)
                .arg(audit.facetedCrossSectionAreaMm2,0,'g',16)
                .arg(audit.relativeAreaError,0,'g',16)
                .arg(rebuiltVolume,0,'g',16)
                .arg(storedVolume,0,'g',16);
        *csv=content.toUtf8();
        if(error)error->clear();return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("OCCT refold check failed: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QStringLiteral("Refold check failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::buildLinkedFlatDevelopmentSet(const QString& flatBodyId,
        QByteArray* svg,QByteArray* dxf,QByteArray* bendCsv,QString* error) const{
    if(!svg||!dxf||!bendCsv||svg==dxf||svg==bendCsv||dxf==bendCsv){
        setError(error,"Three separate output buffers are required for a complete flat-development set.");return false;
    }
    const Part* flat=find(flatBodyId);
    if(!flat||flat->linkedFoldedFlatSourceId.isEmpty()){
        setError(error,"Select a saved associative flat blank, not the folded source.");return false;
    }
    const Part* folded=find(flat->linkedFoldedFlatSourceId);
    if(!folded||!folded->hasFoldedSheetMetalDefinition||
       folded->geometryToken!=folded->foldedSheetGeometryToken||
       flat->linkedFoldedFlatSourceToken!=folded->geometryToken){
        setError(error,"The linked flat development is missing or stale; rebuild the folded source.");return false;
    }
    const QVector<double> factors=flat->linkedFoldedFlatKFactors.isEmpty()?
        QVector<double>(folded->foldedSheetBends.size(),0.33):flat->linkedFoldedFlatKFactors;
    QByteArray stagedSvg,stagedDxf,stagedCsv;
    MultiBendFlatPattern visual,manufacturing;
    QString issue;
    if(!buildFoldedSheetFlatPatternSvg(folded->foldedSheetFlangesMm,
           folded->foldedSheetWidthMm,folded->foldedSheetThicknessMm,
           folded->foldedSheetBends,factors,flat->linkedFoldedFlatRelief,
           &stagedSvg,&visual,&issue)||
       !buildFoldedSheetFlatPatternDxf(folded->foldedSheetFlangesMm,
           folded->foldedSheetWidthMm,folded->foldedSheetThicknessMm,
           folded->foldedSheetBends,factors,flat->linkedFoldedFlatRelief,
           &stagedDxf,&manufacturing,&issue)||
       !buildFoldedSheetBendTableCsv(folded->foldedSheetFlangesMm,
           folded->foldedSheetWidthMm,folded->foldedSheetThicknessMm,
           folded->foldedSheetBends,factors,&stagedCsv,&issue)||
       !equivalentFoldedSheetDevelopment(visual,manufacturing,&issue)||
       stagedSvg.isEmpty()||stagedDxf.isEmpty()||stagedCsv.isEmpty()){
        setError(error,issue.isEmpty()?"Linked SVG/DXF/CSV set could not be validated.":issue);
        return false; // Caller outputs remain unchanged even if one generator failed.
    }
    // Independent neutral-axis audit against the saved folded definition.
    // This catches a drift between the geometry exporter and the bend schedule
    // before ANY caller buffer is modified or a package is published.
    std::vector<double> nativeFlanges,stations,allowances;
    std::vector<cad::sheet::BendInput> nativeBends;
    nativeFlanges.reserve(static_cast<std::size_t>(folded->foldedSheetFlangesMm.size()));
    nativeBends.reserve(static_cast<std::size_t>(folded->foldedSheetBends.size()));
    for(double mm:folded->foldedSheetFlangesMm)nativeFlanges.push_back(mm);
    for(int i=0;i<folded->foldedSheetBends.size();++i)
        nativeBends.push_back({folded->foldedSheetBends[i].angleDegrees,
            folded->foldedSheetBends[i].insideRadiusMm,factors[i]});
    for(double mm:manufacturing.bendLinePositionsMm)stations.push_back(mm);
    for(double mm:manufacturing.bendAllowancesMm)allowances.push_back(mm);
    cad::sheet::LinkedFlatAudit reference;
    std::string referenceError;
    if(!cad::sheet::auditLinkedFlatDevelopment(nativeFlanges,
            folded->foldedSheetWidthMm,folded->foldedSheetThicknessMm,
            nativeBends,manufacturing.flatLengthMm,manufacturing.widthMm,
            stations,allowances,&reference,&referenceError)){
        setError(error,QStringLiteral("Linked development cross-check failed: %1")
            .arg(QString::fromStdString(referenceError)));
        return false;
    }
    *svg=std::move(stagedSvg);*dxf=std::move(stagedDxf);*bendCsv=std::move(stagedCsv);
    if(error)error->clear();
    return true;
}

bool SceneDocument::exportLinkedFlatManufacturingPackage(const QString& flatBodyId,
        const QString& newDirectory,QString* error) const{
    const QFileInfo destination(newDirectory);
    if(newDirectory.trimmed().isEmpty()||destination.exists()||
       !destination.isAbsolute()||!QDir(destination.absolutePath()).exists()){
        setError(error,"Choose a new package folder under an existing directory; existing packages are not overwritten.");return false;
    }
    const Part* flat=find(flatBodyId);
    const Part* folded=flat?find(flat->linkedFoldedFlatSourceId):nullptr;
    if(!flat||!folded){setError(error,"A current saved associative flat blank is required.");return false;}
    QByteArray svg,dxf,bendCsv;
    if(!buildLinkedFlatDevelopmentSet(flatBodyId,&svg,&dxf,&bendCsv,error))return false;
    const QVector<double> factors=flat->linkedFoldedFlatKFactors.isEmpty()?
        QVector<double>(folded->foldedSheetBends.size(),0.33):flat->linkedFoldedFlatKFactors;
    QJsonObject manifest;
    manifest.insert("format","CADWorkbench associative multi-flange package v1");
    manifest.insert("folded_source_id",folded->id);
    manifest.insert("flat_body_id",flat->id);
    manifest.insert("source_geometry_token",folded->geometryToken);
    manifest.insert("linked_source_token",flat->linkedFoldedFlatSourceToken);
    manifest.insert("source_name",folded->name);
    manifest.insert("width_mm",folded->foldedSheetWidthMm);
    manifest.insert("thickness_mm",folded->foldedSheetThicknessMm);
    manifest.insert("relief_width_mm",flat->linkedFoldedFlatRelief.widthMm);
    manifest.insert("relief_depth_mm",flat->linkedFoldedFlatRelief.depthMm);
    QJsonArray flanges,kValues,bends;
    for(double length:folded->foldedSheetFlangesMm)flanges.append(length);
    for(double factor:factors)kValues.append(factor);
    for(const auto& bend:folded->foldedSheetBends){
        QJsonObject entry;entry.insert("signed_angle_degrees",bend.angleDegrees);
        entry.insert("inside_radius_mm",bend.insideRadiusMm);bends.append(entry);
    }
    manifest.insert("straight_flange_lengths_mm",flanges);
    manifest.insert("per_bend_k_factors",kValues);
    manifest.insert("bends",bends);
    QJsonObject hashes;
    const QMap<QString,QByteArray> files{
        {QStringLiteral("flat-pattern.svg"),svg},
        {QStringLiteral("flat-pattern.dxf"),dxf},
        {QStringLiteral("bend-schedule.csv"),bendCsv}};
    for(auto it=files.cbegin();it!=files.cend();++it)
        hashes.insert(it.key(),QString::fromLatin1(QCryptographicHash::hash(it.value(),
            QCryptographicHash::Sha256).toHex()));
    manifest.insert("sha256",hashes);
    manifest.insert("note","Neutral flat geometry is not proof of forming, tooling clearance, or springback accuracy.");
    QTemporaryDir staging(QDir(destination.absolutePath()).filePath(".cadworkbench-package-XXXXXX"));
    if(!staging.isValid()){
        setError(error,"Could not create a temporary manufacturing package folder.");return false;
    }
    auto write=[&](const QString& name,const QByteArray& data)->bool{
        QSaveFile output(QDir(staging.path()).filePath(name));
        if(!output.open(QIODevice::WriteOnly)||output.write(data)!=data.size()||!output.commit()){
            setError(error,QString("Failed to write manufacturing package file: %1").arg(name));return false;
        }
        return true;
    };
    for(auto it=files.cbegin();it!=files.cend();++it)
        if(!write(it.key(),it.value()))return false;
    if(!write("manifest.json",QJsonDocument(manifest).toJson(QJsonDocument::Indented)))return false;
    if(QDir(newDirectory).exists()||!QDir().rename(staging.path(),newDirectory)){
        setError(error,"Could not publish manufacturing package; the temporary export was discarded.");return false;
    }
    staging.setAutoRemove(false);
    if(error)error->clear();return true;
}

bool SceneDocument::exportLinkedFlatManufacturingBatch(const QStringList& flatBodyIds,
        const QString& newDirectory,QString* error,
        const StockLayoutSettings& stock) const{
    if(flatBodyIds.size()<2||flatBodyIds.size()>24){
        setError(error,"Select 2-24 distinct associative flat blanks for a batch job.");return false;
    }
    QSet<QString> unique;
    struct Item{QString id;QString source;QString folder;QByteArray svg,dxf,csv;
        double flatLengthMm=0.0,sheetWidthMm=0.0;
        std::vector<contour_nesting::Rect> reliefCutouts;};
    QVector<Item> items;
    items.reserve(flatBodyIds.size());
    for(int i=0;i<flatBodyIds.size();++i){
        const QString id=flatBodyIds[i];
        const Part* flat=find(id);
        if(unique.contains(id)||!flat||flat->linkedFoldedFlatSourceId.isEmpty()){
            setError(error,"Every selected body must be a unique associative flat blank.");return false;
        }
        unique.insert(id);
        Item item;item.id=id;item.source=flat->linkedFoldedFlatSourceId;
        item.folder=QString("part_%1_%2").arg(i+1,3,10,QChar('0')).arg(id.left(12));
        if(!buildLinkedFlatDevelopmentSet(id,&item.svg,&item.dxf,&item.csv,error))return false;
        if(stock.enabled){
            const Part* folded=find(item.source);
            if(!folded){setError(error,"A linked flat blank lost its folded source.");return false;}
            const QVector<double> factors=flat->linkedFoldedFlatKFactors.isEmpty()?
                QVector<double>(folded->foldedSheetBends.size(),0.33):flat->linkedFoldedFlatKFactors;
            MultiBendFlatPattern developed;
            if(!calculateFoldedSheetFlatPattern(folded->foldedSheetFlangesMm,
                    folded->foldedSheetWidthMm,folded->foldedSheetThicknessMm,
                    folded->foldedSheetBends,factors,&developed,error))return false;
            item.flatLengthMm=developed.flatLengthMm;
            item.sheetWidthMm=developed.widthMm;
            const SheetMetalRelief relief=flat->linkedFoldedFlatRelief;
            // This IS the same neutral bend-station rule used by linked B-rep
            // relief cutting and DXF. No approximation to a filled rectangle
            // is needed when the blank has orthogonal end reliefs.
            if(relief.widthMm>0){
                for(double station:developed.bendLinePositionsMm){
                    const double left=station-relief.widthMm/2.0;
                    item.reliefCutouts.push_back({left,0,relief.widthMm,relief.depthMm});
                    item.reliefCutouts.push_back({left,developed.widthMm-relief.depthMm,
                        relief.widthMm,relief.depthMm});
                }
            }
        }
        items.append(std::move(item));
    }
    stock_material::Result gaugeReport;
    if(stock.enabled&&stock.requireCompatibleGaugeAndMaterial){
        std::vector<stock_material::Part> gaugeItems;
        gaugeItems.reserve(static_cast<std::size_t>(items.size()));
        for(const Item& item:items){
            const Part* folded=find(item.source);
            const Part* flat=find(item.id);
            if(!folded||!flat){
                setError(error,"A stock item lost its source or flat body.");return false;
            }
            const QString grade=folded->materialName.isEmpty()?
                flat->materialName:folded->materialName;
            gaugeItems.push_back({item.id.toStdString(),grade.trimmed().toUpper().toStdString(),
                folded->foldedSheetThicknessMm});
        }
        std::string detail;
        if(!stock_material::validate(gaugeItems,&gaugeReport,&detail)){
            setError(error,QStringLiteral("Stock material/gauge issue: ")+
                QString::fromStdString(detail));return false;
        }
    }
    stock_layout::Plan sheetPlan;
    contour_nesting::Layout contourPlan;
    const bool reliefAware=stock.enabled&&std::any_of(items.cbegin(),items.cend(),
        [](const Item& item){return !item.reliefCutouts.empty();});
    if(stock.enabled){
        std::string issue;
        if(reliefAware){
            std::vector<contour_nesting::Blank> blanks;
            for(const Item& item:items)
                blanks.push_back({item.id.toStdString(),item.flatLengthMm,
                    item.sheetWidthMm,item.reliefCutouts});
            if(!contour_nesting::build(blanks,stock.stockWidthMm,stock.stockHeightMm,
                    stock.gapMm,stock.allowRotation,&contourPlan,&issue)){
                setError(error,QStringLiteral("Relief-aware contour planning rejected: ")+
                    QString::fromStdString(issue));return false;
            }
            sheetPlan=contourPlan.plan;
        }else{
            std::vector<stock_layout::Blank> blanks;
            for(const Item& item:items)blanks.push_back({item.id.toStdString(),
                item.flatLengthMm,item.sheetWidthMm});
            if(!stock_layout::build(blanks,stock.stockWidthMm,stock.stockHeightMm,
                    stock.gapMm,stock.allowRotation,&sheetPlan,&issue)){
                setError(error,QStringLiteral("Stock-layout planning rejected: ")+
                    QString::fromStdString(issue));return false;
            }
        }
    }
    const QFileInfo destination(newDirectory);
    if(newDirectory.trimmed().isEmpty()||!destination.isAbsolute()||
       destination.exists()||!QDir(destination.absolutePath()).exists()){
        setError(error,"The manufacturing batch needs a new folder under an existing directory.");return false;
    }
    QTemporaryDir staging(QDir(destination.absolutePath()).filePath(".cadworkbench-batch-XXXXXX"));
    if(!staging.isValid()){setError(error,"Cannot stage batch package in destination directory.");return false;}
    QJsonArray manifestParts;
    QByteArray index("part_folder,flat_body_id,folded_source_id,svg_sha256,dxf_sha256,bend_csv_sha256\n");
    const auto hash=[](const QByteArray& bytes){return QString::fromLatin1(
        QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());};
    for(const Item& item:items){
        const QString folder=QDir(staging.path()).filePath(item.folder);
        // Reuse the normal per-part package implementation, including its
        // geometry provenance and SHA-256 manifest. The outer temporary dir
        // removes every partial part if any later item fails.
        if(!exportLinkedFlatManufacturingPackage(item.id,folder,error))return false;
        QJsonObject entry;
        entry.insert("flat_body_id",item.id);entry.insert("folded_source_id",item.source);
        entry.insert("package_directory",item.folder);
        entry.insert("svg_sha256",hash(item.svg));
        entry.insert("dxf_sha256",hash(item.dxf));
        entry.insert("bend_csv_sha256",hash(item.csv));
        manifestParts.append(entry);
        index+=QString("%1,%2,%3,%4,%5,%6\n")
            .arg(item.folder,item.id,item.source,hash(item.svg),hash(item.dxf),hash(item.csv)).toUtf8();
    }
    QJsonObject indexManifest;
    indexManifest.insert("format","CADWorkbench multi-part associative sheet-metal job v1");
    QByteArray stockCsv,stockSvg,cutLengthCsv;
    double totalCutLength=0.0;
    if(stock.enabled){
        // Calculate exposed neutral-material cut length from each developed
        // contour, not the outside envelope. Interior seams created by our
        // orthogonal decomposition are excluded. This is planning evidence,
        // not a cutter toolpath, kerf allowance or piercing schedule.
        cutLengthCsv="flat_body_id,material_area_mm2,relief_area_mm2,exposed_cut_length_mm\n";
        for(const Item& item:items){
            contour_metrics::Result metrics;std::string detail;
            if(!contour_metrics::measure({item.id.toStdString(),item.flatLengthMm,
                    item.sheetWidthMm,item.reliefCutouts},&metrics,&detail)){
                setError(error,QStringLiteral("Cut-length planning failed for %1: %2")
                    .arg(item.id,QString::fromStdString(detail)));return false;
            }
            totalCutLength+=metrics.exposedCutLengthMm;
            cutLengthCsv+=QString("%1,%2,%3,%4\n").arg(item.id)
                .arg(metrics.netAreaMm2,0,'g',16)
                .arg(metrics.removedAreaMm2,0,'g',16)
                .arg(metrics.exposedCutLengthMm,0,'g',16).toUtf8();
        }
        // The manifest is assembled after stock-layout generation; attach
        // totalCutLength to its stock-layout record below.

        stockCsv="sheet,flat_body_id,x_mm,y_mm,length_mm,width_mm,rotated_90_deg\n";
        for(const auto& position:sheetPlan.placements){
            stockCsv+=QString("%1,%2,%3,%4,%5,%6,%7\n")
                .arg(static_cast<qulonglong>(position.sheet)).arg(QString::fromStdString(position.id))
                .arg(position.xMm,0,'g',16).arg(position.yMm,0,'g',16)
                .arg(position.lengthMm,0,'g',16).arg(position.widthMm,0,'g',16)
                .arg(position.rotated?"YES":"NO").toUtf8();
        }
        // SVG shows actual material regions for orthogonal relieved flats,
        // otherwise the enclosing rectangles. Neither is a certified cut path.
        QString svg=QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %1 %2\">\n")
            .arg(stock.stockWidthMm,0,'g',16)
            .arg(stock.stockHeightMm*sheetPlan.sheets+stock.gapMm*(sheetPlan.sheets-1),0,'g',16);
        for(std::size_t sheet=1;sheet<=sheetPlan.sheets;++sheet){
            const double oy=(sheet-1)*(stock.stockHeightMm+stock.gapMm);
            svg+=QStringLiteral("<rect x=\"0\" y=\"%1\" width=\"%2\" height=\"%3\" fill=\"none\" stroke=\"#555\" stroke-width=\"0.8\"/>\n")
                .arg(oy,0,'g',16).arg(stock.stockWidthMm,0,'g',16).arg(stock.stockHeightMm,0,'g',16);
        }
        if(reliefAware){
            // Draw ACTUAL material patches after the exact rectangular-relief
            // subtraction. Separate patches are nonoverlapping by construction.
            for(const auto& item:contourPlan.items){
                const double oy=(item.placement.sheet-1)*(stock.stockHeightMm+stock.gapMm);
                for(const auto& piece:item.material){
                    svg+=QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" fill=\"#d9e4f4\" stroke=\"#183c59\" stroke-width=\"0.15\"/>\n")
                        .arg(piece.x,0,'g',16).arg(piece.y+oy,0,'g',16)
                        .arg(piece.w,0,'g',16).arg(piece.h,0,'g',16);
                }
                svg+=QStringLiteral("<text x=\"%1\" y=\"%2\" font-size=\"4\" fill=\"#14334d\">%3</text>\n")
                    .arg(item.placement.xMm+2,0,'g',16)
                    .arg(item.placement.yMm+oy+5,0,'g',16)
                    .arg(QString::fromStdString(item.placement.id).left(12).toHtmlEscaped());
            }
        }else for(const auto& position:sheetPlan.placements){
            const double oy=(position.sheet-1)*(stock.stockHeightMm+stock.gapMm);
            svg+=QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" fill=\"#d9e4f4\" stroke=\"#183c59\" stroke-width=\"0.5\"/>\n")
                .arg(position.xMm,0,'g',16).arg(position.yMm+oy,0,'g',16)
                .arg(position.lengthMm,0,'g',16).arg(position.widthMm,0,'g',16);
            svg+=QStringLiteral("<text x=\"%1\" y=\"%2\" font-size=\"4\" fill=\"#14334d\">%3</text>\n")
                .arg(position.xMm+2,0,'g',16).arg(position.yMm+oy+5,0,'g',16)
                .arg(QString::fromStdString(position.id).left(12).toHtmlEscaped());
        }
        svg+=QStringLiteral("</svg>\n");stockSvg=svg.toUtf8();
        QJsonObject layout;
        layout.insert("stock_material_compatibility_checked",stock.requireCompatibleGaugeAndMaterial);
        if(stock.requireCompatibleGaugeAndMaterial){
            layout.insert("common_sheet_thickness_mm",gaugeReport.thicknessMm);
            layout.insert("common_material_grade",QString::fromStdString(gaugeReport.material));
            layout.insert("material_grade_assigned",gaugeReport.materialAssigned);
            layout.insert("grade_note",gaugeReport.materialAssigned?
                "Assigned grades agree; independently verify heat/lot and specification.":
                "No material grade is assigned; identify stock before manufacturing.");
        }
        layout.insert("stock_width_mm",stock.stockWidthMm);
        layout.insert("stock_height_mm",stock.stockHeightMm);
        layout.insert("gap_mm",stock.gapMm);
        layout.insert("allow_90_degree_rotation",stock.allowRotation);
        layout.insert("sheet_count",static_cast<int>(sheetPlan.sheets));
        // Backward-compatible envelope utilization is retained for legacy
        // consumers; contour-aware net material utilization is additional.
        double envelopeArea=0.0;
        for(const Item& item:items)envelopeArea+=item.flatLengthMm*item.sheetWidthMm;
        layout.insert("blank_envelope_area_mm2",envelopeArea);
        const double availableArea=stock.stockWidthMm*stock.stockHeightMm*sheetPlan.sheets;
        layout.insert("rectangular_utilization_fraction",envelopeArea/availableArea);
        layout.insert("material_utilization_fraction",sheetPlan.utilization);
        layout.insert("stock_layout_csv_sha256",hash(stockCsv));
        layout.insert("stock_layout_svg_sha256",hash(stockSvg));
        layout.insert("cut_length_csv_sha256",hash(cutLengthCsv));
        layout.insert("total_exposed_neutral_cut_length_mm",totalCutLength);
        layout.insert("cut_length_qualification","Neutral orthogonal contours only; no kerf, lead-ins, piercing or toolpath ordering.");
        layout.insert("relief_aware_contour_placement",reliefAware);
        layout.insert("net_contour_material_area_mm2",sheetPlan.totalBlankAreaMm2);
        layout.insert("qualification",reliefAware?
            "Orthogonal neutral-relief material regions with exact planar overlap checks; greedy placement, not CNC nesting or grain/kerf certification.":
            "Rectangular envelope free-region packing; not cut-contour nesting or grain/kerf certification.");
        indexManifest.insert("stock_layout",layout);
    }
    indexManifest.insert("part_count",static_cast<int>(items.size()));
    indexManifest.insert("parts",manifestParts);
    indexManifest.insert("index_csv_sha256",hash(index));
    indexManifest.insert("qualification","Neutral developments require independent shop validation.");
    const auto write=[&](const QString& name,const QByteArray& bytes)->bool{
        QSaveFile file(QDir(staging.path()).filePath(name));
        if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){
            setError(error,"Could not finish the staged batch index/manifest.");return false;
        }
        return true;
    };
    if(stock.enabled&&(!write("stock-layout.csv",stockCsv)||
        !write("stock-layout.svg",stockSvg)||
        !write("cut-length-summary.csv",cutLengthCsv)))return false;
    if(!write("batch-index.csv",index)||!write("batch-manifest.json",
            QJsonDocument(indexManifest).toJson(QJsonDocument::Indented)))return false;
    if(QDir(newDirectory).exists()||!QDir().rename(staging.path(),newDirectory)){
        setError(error,"Batch publication failed; no incomplete job was published.");return false;
    }
    staging.setAutoRemove(false);
    if(error)error->clear();return true;
}

bool SceneDocument::editFoldedSheetMetal(const QString& partId,const QVector<double>& flanges,
        double width,double thickness,const QVector<FoldedSheetBend>& bends,QString* error) {
    return editFoldedSheetMetalWithDevelopments(partId,flanges,width,thickness,bends,{},error);
}

bool SceneDocument::editFoldedSheetMetalWithDevelopments(const QString& partId,
        const QVector<double>& flanges,double width,double thickness,
        const QVector<FoldedSheetBend>& bends,
        const QMap<QString,QVector<double>>& linkedKFactors,QString* error) {
    // Qt 6's QVector (QList) is implicitly shared.  Store a stable integer
    // index BEFORE copying parts_: calling non-const parts_.begin() after
    // the copy may detach storage and invalidate a saved mutable iterator.
    // This previously caused undefined behavior (and a Windows segfault)
    // when the staged folded edit indexed back into its source scene.
    const auto found=std::find_if(parts_.cbegin(),parts_.cend(),[&](const Part& part){return part.id==partId;});
    if(found==parts_.cend()||!found->hasFoldedSheetMetalDefinition){setError(error,"Selected body has no editable folded sheet-metal definition.");return false;}
    if(found->geometryToken!=found->foldedSheetGeometryToken){setError(error,"Folded sheet-metal definition is stale because the body was changed by another modeling operation.");return false;}
    const qsizetype sourceIndex=static_cast<qsizetype>(std::distance(parts_.cbegin(),found));
    // Explicitly identify each associated blank. Unknown or unrelated IDs
    // cannot alter another source's K-factors, even if the source itself is
    // otherwise valid. A nonempty K-factor list must match every new bend.
    bool developmentChanged=false;
    for(auto it=linkedKFactors.cbegin();it!=linkedKFactors.cend();++it){
        const Part* linked=find(it.key());
        if(!linked||linked->linkedFoldedFlatSourceId!=partId||
           (!it.value().isEmpty()&&it.value().size()!=bends.size())){
            setError(error,"New linked development must name a flat blank of this source and provide one K-factor per new bend.");
            return false;
        }
        for(const double factor:it.value())if(!std::isfinite(factor)||factor<0||factor>1){
            setError(error,"Every linked development K-factor must be finite and between zero and one.");return false;
        }
        if(linked->linkedFoldedFlatKFactors!=it.value())developmentChanged=true;
    }
    // Diagnostic-only trace for Windows crash triage; no normal-console output.
    // The environment variable is explicitly set by RUN_V170_COMPREHENSIVE_AUDIT.ps1.
    const bool foldedTrace=qEnvironmentVariableIsSet("CAD_V170_FOLDED_TRACE");
    const auto traceFolded=[foldedTrace](const char* stage){
        if(foldedTrace){std::fprintf(stderr,"[folded commit] %s\n",stage);std::fflush(stderr);}
    };
    try{
        traceFolded("solid builder: before");
        const TopoDS_Shape rebuilt=makeFoldedSheetSolid(flanges,width,thickness,bends,error);
        traceFolded("solid builder: after");
        if(!validShape(rebuilt,error))return false;
        traceFolded("solid validation: after");
        if(flanges==found->foldedSheetFlangesMm&&width==found->foldedSheetWidthMm&&
           thickness==found->foldedSheetThicknessMm&&bends.size()==found->foldedSheetBends.size()){
            bool same=true;for(int i=0;i<bends.size();++i)same=same&&bends[i].angleDegrees==found->foldedSheetBends[i].angleDegrees&&
                bends[i].insideRadiusMm==found->foldedSheetBends[i].insideRadiusMm;
            if(same&&!developmentChanged)return true;
        }
        traceFolded("staged part copy: before");
        QVector<Part> proposed=parts_;
        traceFolded("staged part copy: after");
        Part& part=proposed[sourceIndex]; // stable index, no iterator into detached Qt storage
        part.shape=rebuilt;part.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        part.foldedSheetGeometryToken=part.geometryToken;part.foldedSheetFlangesMm=flanges;
        part.foldedSheetWidthMm=width;part.foldedSheetThicknessMm=thickness;part.foldedSheetBends=bends;
        for(Part& linked:proposed){
            const auto update=linkedKFactors.constFind(linked.id);
            if(update==linkedKFactors.cend())continue;
            if(linked.linkedFoldedFlatKFactors!=update.value()){
                linked.linkedFoldedFlatKFactors=update.value();
                linked.linkedFoldedFlatSourceToken.clear(); // force rebuild even for same source dimensions
            }
        }
        // The source body and every linked neutral blank regenerate in one
        // transaction. An invalid dependent rejects the entire proposed edit.
        traceFolded("linked propagation: before");
        if(!propagateLinkedBodies(&proposed,error))return false;
        traceFolded("linked propagation: after");
        traceFolded("undo snapshot: before");
        checkpoint();parts_=std::move(proposed);
        traceFolded("commit: after");
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Folded sheet-metal rebuild failed: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::loftRectangles(double bottomX,double bottomY,double bottomWidth,double bottomDepth,
                                   double topX,double topY,double topWidth,double topDepth,double height,QString* error) {
    if(!finiteOffset(bottomX)||!finiteOffset(bottomY)||!finiteOffset(topX)||!finiteOffset(topY)||
       !finitePositive(bottomWidth)||!finitePositive(bottomDepth)||!finitePositive(topWidth)||
       !finitePositive(topDepth)||!finitePositive(height)){
        setError(error,"Loft origins and section dimensions must be finite and positive.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        auto rectangleWire=[](double x,double y,double w,double d,double z){
            BRepBuilderAPI_MakePolygon polygon;
            polygon.Add(gp_Pnt(x,y,z));polygon.Add(gp_Pnt(x+w,y,z));polygon.Add(gp_Pnt(x+w,y+d,z));
            polygon.Add(gp_Pnt(x,y+d,z));polygon.Close();
            return polygon.Wire();
        };
        BRepOffsetAPI_ThruSections loft(true,true);
        loft.AddWire(rectangleWire(bottomX,bottomY,bottomWidth,bottomDepth,0));
        loft.AddWire(rectangleWire(topX,topY,topWidth,topDepth,height));
        loft.Build();
        if(!loft.IsDone()){setError(error,"The kernel could not loft between the rectangular sections.");return false;}
        const TopoDS_Shape result=loft.Shape();
        if(!validShape(result,error))return false;
        const QString name=uniqueName(QStringLiteral("Lofted Rectangle"));
        checkpoint();parts_.append({generatedId(),name,result});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::loftEllipticalSections(double bottomSemiAxisA,double bottomSemiAxisB,
                                           double topSemiAxisA,double topSemiAxisB,double height,
                                           double offsetX,double offsetY,QString* error) {
    if(!finitePositive(bottomSemiAxisA)||!finitePositive(bottomSemiAxisB)||
       !finitePositive(topSemiAxisA)||!finitePositive(topSemiAxisB)||!finitePositive(height)||
       !finiteOffset(offsetX)||!finiteOffset(offsetY)){
        setError(error,"Elliptical loft semi-axes and height must be positive; top offsets must be finite.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const auto ellipseWire=[](double centerX,double centerY,double axisA,double axisB,double z){
            const double major=std::max(axisA,axisB),minor=std::min(axisA,axisB);
            const gp_Dir majorDirection=axisA>=axisB?gp_Dir(1,0,0):gp_Dir(0,1,0);
            const gp_Ax2 frame(gp_Pnt(centerX,centerY,z),gp_Dir(0,0,1),majorDirection);
            return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Elips(frame,major,minor)).Edge()).Wire();
        };
        BRepOffsetAPI_ThruSections loft(true,true);
        loft.AddWire(ellipseWire(0,0,bottomSemiAxisA,bottomSemiAxisB,0));
        loft.AddWire(ellipseWire(offsetX,offsetY,topSemiAxisA,topSemiAxisB,height));
        loft.Build();
        if(!loft.IsDone()){setError(error,"The kernel could not loft between the elliptical sections.");return false;}
        const TopoDS_Shape result=loft.Shape();
        if(!validShape(result,error))return false;
        const QString name=uniqueName(QStringLiteral("Lofted Ellipse"));
        checkpoint();parts_.append({generatedId(),name,result});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Elliptical loft kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addOrientedCylinder(double radius,double height,double ox,double oy,double oz,double ax,double ay,double az,QString* error) {
    if(!finitePositive(radius)||!finitePositive(height)||!finiteOffset(ox)||!finiteOffset(oy)||!finiteOffset(oz)||
       !std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(az)||std::sqrt(ax*ax+ay*ay+az*az)<1e-8){
        setError(error,"Cylinder dimensions, origin, or axis are invalid.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(ox,oy,oz),gp_Dir(ax,ay,az)),radius,height).Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Oriented Cylinder"));checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addOrientedCone(double bottomRadius,double topRadius,double height,double ox,double oy,double oz,
                                    double ax,double ay,double az,QString* error) {
    if(!finitePositive(bottomRadius)||!std::isfinite(topRadius)||topRadius<0||topRadius>1e6||!finitePositive(height)||
       !finiteOffset(ox)||!finiteOffset(oy)||!finiteOffset(oz)||!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(az)||
       std::sqrt(ax*ax+ay*ay+az*az)<1e-8){setError(error,"Cone dimensions, origin, or axis are invalid.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(ox,oy,oz),gp_Dir(ax,ay,az)),bottomRadius,topRadius,height).Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Oriented Cone"));checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addTorusSegment(double majorRadius,double tubeRadius,double angleDegrees,QString* error) {
    if(!finitePositive(majorRadius)||!finitePositive(tubeRadius)||majorRadius<=tubeRadius||
       !std::isfinite(angleDegrees)||angleDegrees<=1.0||angleDegrees>360.0){
        setError(error,"Torus major radius must exceed tube radius; segment angle must be over 1 and at most 360 degrees.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=BRepPrimAPI_MakeTorus(majorRadius,tubeRadius,angleDegrees*std::numbers::pi/180.0).Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Torus Segment"));checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::sweepCircularProfile(double radius,double pathLength,double lateralOffset,double verticalRise,QString* error) {
    return addSweepProfile(true,false,radius,radius,pathLength,lateralOffset,verticalRise,error);
}

bool SceneDocument::sweepRectangularProfile(double width,double height,double pathLength,double lateralOffset,double verticalRise,QString* error) {
    return addSweepProfile(false,false,width,height,pathLength,lateralOffset,verticalRise,error);
}

bool SceneDocument::sweepEllipticalProfile(double semiAxisA,double semiAxisB,double pathLength,double lateralOffset,
                                            double verticalRise,QString* error) {
    return addSweepProfile(false,true,semiAxisA,semiAxisB,pathLength,lateralOffset,verticalRise,error);
}

bool SceneDocument::addHelicalSweep(double profileRadius,double coilRadius,double pitch,double turns,QString* error) {
    if(!finitePositive(profileRadius)||!finitePositive(coilRadius)||!finitePositive(pitch)||
       !finitePositive(turns)||turns<0.25||turns>20.0||coilRadius<=profileRadius||pitch<=2.0*profileRadius){
        setError(error,"Helix requires positive dimensions, at least 0.25 and at most 20 turns, coil radius greater than wire radius, and pitch greater than wire diameter.");
        return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        constexpr int samplesPerTurn=24;
        const int sampleCount=std::max(6,static_cast<int>(std::ceil(turns*samplesPerTurn)));
        Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,sampleCount+1);
        for(int i=0;i<=sampleCount;++i){
            const double angle=2.0*std::numbers::pi*turns*static_cast<double>(i)/sampleCount;
            points->SetValue(i+1,gp_Pnt(coilRadius*std::cos(angle),coilRadius*std::sin(angle),pitch*turns*static_cast<double>(i)/sampleCount));
        }
        GeomAPI_Interpolate interpolation(points,false,1e-6);
        interpolation.Perform();
        if(!interpolation.IsDone()){setError(error,"Could not interpolate the helical path.");return false;}
        const Handle(Geom_BSplineCurve) centerline=interpolation.Curve();
        const TopoDS_Wire spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(centerline).Edge()).Wire();
        gp_Pnt start,tangentPoint;gp_Vec tangent;
        centerline->D1(centerline->FirstParameter(),start,tangent);
        if(tangent.SquareMagnitude()<=1e-16){setError(error,"Helical path has no initial direction.");return false;}
        tangent.Normalize();
        gp_Vec reference(0,0,1);reference-=tangent*tangent.Dot(reference);
        if(reference.SquareMagnitude()<=1e-8){reference=gp_Vec(0,1,0);reference-=tangent*tangent.Dot(reference);}
        const gp_Ax2 frame(start,gp_Dir(tangent),gp_Dir(reference));
        const TopoDS_Wire profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,profileRadius)).Edge()).Wire();
        BRepOffsetAPI_MakePipeShell pipe(spine);
        pipe.Add(profile);
        pipe.Build();
        if(!pipe.IsDone()||!pipe.MakeSolid()){
            setError(error,"Open CASCADE could not build a solid from this helical path.");return false;
        }
        const TopoDS_Shape shape=pipe.Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Helical Sweep"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Helical sweep kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addVariablePitchHelicalSweep(double profileRadius,double coilRadius,double startPitch,
                                                  double endPitch,double turns,QString* error) {
    if(!finitePositive(profileRadius)||!finitePositive(coilRadius)||!finitePositive(startPitch)||
       !finitePositive(endPitch)||!finitePositive(turns)||turns<0.25||turns>20.0||
       coilRadius<=profileRadius||startPitch<=2.0*profileRadius||endPitch<=2.0*profileRadius){
        setError(error,"Variable-pitch helix requires positive dimensions, 0.25–20 turns, coil radius greater than wire radius, and both pitches greater than wire diameter.");
        return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        constexpr int samplesPerTurn=32;
        const int sampleCount=std::max(8,static_cast<int>(std::ceil(turns*samplesPerTurn)));
        Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,sampleCount+1);
        for(int i=0;i<=sampleCount;++i){
            const double t=static_cast<double>(i)/sampleCount;
            const double angle=2.0*std::numbers::pi*turns*t;
            const double z=turns*(startPitch*t+0.5*(endPitch-startPitch)*t*t);
            points->SetValue(i+1,gp_Pnt(coilRadius*std::cos(angle),coilRadius*std::sin(angle),z));
        }
        GeomAPI_Interpolate interpolation(points,false,1e-6);
        interpolation.Perform();
        if(!interpolation.IsDone()){setError(error,"Could not interpolate the variable-pitch helical path.");return false;}
        const Handle(Geom_BSplineCurve) centerline=interpolation.Curve();
        const TopoDS_Wire spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(centerline).Edge()).Wire();
        gp_Pnt start;gp_Vec tangent;
        centerline->D1(centerline->FirstParameter(),start,tangent);
        if(tangent.SquareMagnitude()<=1e-16){setError(error,"Variable-pitch path has no initial direction.");return false;}
        tangent.Normalize();
        gp_Vec reference(0,0,1);reference-=tangent*tangent.Dot(reference);
        if(reference.SquareMagnitude()<=1e-8){reference=gp_Vec(0,1,0);reference-=tangent*tangent.Dot(reference);}
        const gp_Ax2 frame(start,gp_Dir(tangent),gp_Dir(reference));
        const TopoDS_Wire profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,profileRadius)).Edge()).Wire();
        BRepOffsetAPI_MakePipeShell pipe(spine);
        pipe.Add(profile);pipe.Build();
        if(!pipe.IsDone()||!pipe.MakeSolid()){
            setError(error,"Open CASCADE could not build a solid from this variable-pitch path.");return false;
        }
        const TopoDS_Shape shape=pipe.Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Variable Pitch Helix"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Variable-pitch helix kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addTaperedHelicalSweep(double profileRadius,double startCoilRadius,double endCoilRadius,
                                            double pitch,double turns,QString* error) {
    if(!finitePositive(profileRadius)||!finitePositive(startCoilRadius)||!finitePositive(endCoilRadius)||
       !finitePositive(pitch)||!finitePositive(turns)||turns<0.25||turns>20.0||
       startCoilRadius<=profileRadius||endCoilRadius<=profileRadius||pitch<=2.0*profileRadius){
        setError(error,"Tapered helix requires positive dimensions, 0.25–20 turns, both coil radii greater than wire radius, and pitch greater than wire diameter.");
        return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        constexpr int samplesPerTurn=32;
        const int sampleCount=std::max(8,static_cast<int>(std::ceil(turns*samplesPerTurn)));
        Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,sampleCount+1);
        for(int i=0;i<=sampleCount;++i){
            const double t=static_cast<double>(i)/sampleCount;
            const double angle=2.0*std::numbers::pi*turns*t;
            const double radius=startCoilRadius+(endCoilRadius-startCoilRadius)*t;
            points->SetValue(i+1,gp_Pnt(radius*std::cos(angle),radius*std::sin(angle),pitch*turns*t));
        }
        GeomAPI_Interpolate interpolation(points,false,1e-6);
        interpolation.Perform();
        if(!interpolation.IsDone()){setError(error,"Could not interpolate the tapered helical path.");return false;}
        const Handle(Geom_BSplineCurve) centerline=interpolation.Curve();
        const TopoDS_Wire spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(centerline).Edge()).Wire();
        gp_Pnt start;gp_Vec tangent;
        centerline->D1(centerline->FirstParameter(),start,tangent);
        if(tangent.SquareMagnitude()<=1e-16){setError(error,"Tapered helical path has no initial direction.");return false;}
        tangent.Normalize();
        gp_Vec reference(0,0,1);reference-=tangent*tangent.Dot(reference);
        if(reference.SquareMagnitude()<=1e-8){reference=gp_Vec(0,1,0);reference-=tangent*tangent.Dot(reference);}
        const gp_Ax2 frame(start,gp_Dir(tangent),gp_Dir(reference));
        const TopoDS_Wire profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,profileRadius)).Edge()).Wire();
        BRepOffsetAPI_MakePipeShell pipe(spine);
        pipe.Add(profile);pipe.Build();
        if(!pipe.IsDone()||!pipe.MakeSolid()){
            setError(error,"Open CASCADE could not build a solid from this tapered helical path.");return false;
        }
        const TopoDS_Shape shape=pipe.Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Tapered Helix"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Tapered helix kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addSweepProfile(bool circular,bool elliptical,double width,double height,double pathLength,double lateralOffset,
                                    double verticalRise,QString* error) {
    if(!finitePositive(width)||(!circular&&!finitePositive(height))||!finitePositive(pathLength)||
       !finiteOffset(lateralOffset)||!finiteOffset(verticalRise)){
        setError(error,"Sweep dimensions must be finite and positive; path offsets must be within ±1,000,000 mm.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const gp_Pnt start(0,0,0),middle(pathLength*0.5,lateralOffset*0.35,verticalRise*0.5),
                     end(pathLength,lateralOffset,verticalRise);
        Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,3);
        points->SetValue(1,start);points->SetValue(2,middle);points->SetValue(3,end);
        GeomAPI_Interpolate interpolation(points,false,1e-6);
        interpolation.Perform();
        if(!interpolation.IsDone()){setError(error,"Could not interpolate the sweep path.");return false;}
        const Handle(Geom_BSplineCurve) centerline=interpolation.Curve();
        TopoDS_Wire spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(centerline).Edge()).Wire();
        gp_Pnt tangentPoint;gp_Vec tangent;
        centerline->D1(centerline->FirstParameter(),tangentPoint,tangent);
        if(tangent.SquareMagnitude()<=1e-16){setError(error,"Sweep path has no initial direction.");return false;}
        tangent.Normalize();
        gp_Vec reference(0,0,1);reference-=tangent*tangent.Dot(reference);
        if(reference.SquareMagnitude()<=1e-8){reference=gp_Vec(0,1,0);reference-=tangent*tangent.Dot(reference);}
        const gp_Ax2 frame(start,gp_Dir(tangent),gp_Dir(reference));
        TopoDS_Wire profile;
        if(circular){
            profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,width)).Edge()).Wire();
        }else if(elliptical){
            const double major=std::max(width,height),minor=std::min(width,height);
            profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Elips(frame,major,minor)).Edge()).Wire();
        }else{
            const gp_Vec x(frame.XDirection()),y(frame.YDirection());
            const auto pointAt=[&](double u,double v){gp_Pnt point=start;point.Translate(x*u+y*v);return point;};
            BRepBuilderAPI_MakePolygon section;
            section.Add(pointAt(-width/2,-height/2));section.Add(pointAt(width/2,-height/2));
            section.Add(pointAt(width/2,height/2));section.Add(pointAt(-width/2,height/2));section.Close();
            if(!section.IsDone()){setError(error,"Could not construct the sweep profile.");return false;}
            profile=section.Wire();
        }
        BRepOffsetAPI_MakePipeShell pipe(spine);
        pipe.Add(profile);
        pipe.Build();
        if(!pipe.IsDone()||!pipe.MakeSolid()){
            setError(error,"Open CASCADE could not build a solid from this profile and path.");return false;
        }
        const TopoDS_Shape shape=pipe.Shape();
        if(!validShape(shape,error))return false;
        const QString baseName=circular?QStringLiteral("Circular Sweep"):(elliptical?QStringLiteral("Elliptical Sweep"):QStringLiteral("Rectangular Sweep"));
        const QString name=uniqueName(baseName);
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Sweep kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::translate(const QString& id, double x, double y, double z, QString* error) {
    if (!finiteOffset(x) || !finiteOffset(y) || !finiteOffset(z)) { setError(error, "Translation offset out of range."); return false; }
    int rootIndex=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==id){rootIndex=i;break;}
    if (rootIndex<0) { setError(error, "Object not found."); return false; }
    if(parts_[rootIndex].hasLinkedBoolean||parts_[rootIndex].hasLinkedSketchExtrusion||parts_[rootIndex].hasInPlaceSketchFeature||!parts_[rootIndex].linkedFoldedFlatSourceId.isEmpty()){setError(error,"Edit a linked Boolean source instead of directly transforming its result.");return false;}
    if (x == 0 && y == 0 && z == 0) return true;
    try {
        gp_Trsf transform;
        transform.SetTranslation(gp_Vec(x,y,z));
        QStringList movingIds{ id };
        for(int cursor=0;cursor<movingIds.size();++cursor)
            for(const Part& candidate:parts_)
                if(candidate.assemblyJointParentBodyId==movingIds[cursor]&&!movingIds.contains(candidate.id))movingIds.append(candidate.id);
        struct TranslationUpdate{int index;TopoDS_Shape shape;};QVector<TranslationUpdate> updates;updates.reserve(movingIds.size());
        for(const QString& movingId:movingIds){
            int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==movingId){index=i;break;}
            if(index<0){setError(error,"A jointed occurrence is missing from the current assembly.");return false;}
            const TopoDS_Shape result=BRepBuilderAPI_Transform(parts_[index].shape,transform,true).Shape();
            const Part& target=parts_[index];
            if(target.isSurfaceBody?!validSurfaceBodyShape(result,error):!validShape(result,error))return false;
            if(!target.componentDefinitionId.isEmpty()&&
               (!finiteOffset(target.placementX+x)||!finiteOffset(target.placementY+y)||!finiteOffset(target.placementZ+z))){
                setError(error,"Assembly occurrence placement would exceed the supported coordinate range.");return false;
            }
            if((target.assemblyJointType==QStringLiteral("Revolute")||target.assemblyJointType==QStringLiteral("Prismatic"))&&
               (!finiteOffset(target.jointOriginX+x)||!finiteOffset(target.jointOriginY+y)||!finiteOffset(target.jointOriginZ+z))){
                setError(error,"Assembly joint origin would exceed the supported coordinate range.");return false;
            }
            updates.append({index,result});
        }
        QVector<Part> candidate=parts_;
        for(const TranslationUpdate& update:updates){
            Part& target=candidate[update.index];target.shape=update.shape;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
            if(!target.componentDefinitionId.isEmpty()){target.placementX+=x;target.placementY+=y;target.placementZ+=z;}
            if(target.assemblyJointType==QStringLiteral("Revolute")||target.assemblyJointType==QStringLiteral("Prismatic")){
                target.jointOriginX+=x;target.jointOriginY+=y;target.jointOriginZ+=z;
            }
        }
        if(!propagateLinkedBodies(&candidate,error))return false;
        checkpoint();parts_=std::move(candidate);
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::rotate(const QString& id, Axis axis, double degrees, QString* error) {
    if (!std::isfinite(degrees) || std::abs(degrees) > 36000) { setError(error, "Angle must be finite and within Â±36,000 degrees."); return false; }
    int rootIndex=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==id){rootIndex=i;break;}
    if (rootIndex<0) { setError(error, "Object not found."); return false; }
    if(parts_[rootIndex].hasLinkedBoolean||parts_[rootIndex].hasLinkedSketchExtrusion||parts_[rootIndex].hasInPlaceSketchFeature||!parts_[rootIndex].linkedFoldedFlatSourceId.isEmpty()){setError(error,"Edit a linked Boolean source instead of directly rotating its result.");return false;}
    if (degrees == 0.0) return true;
    try {
        const gp_Dir direction = axis == Axis::X ? gp_Dir(1,0,0) : axis == Axis::Y ? gp_Dir(0,1,0) : gp_Dir(0,0,1);
        gp_Trsf transform;
        transform.SetRotation(gp_Ax1(gp_Pnt(0,0,0), direction), degrees * std::numbers::pi / 180.0);
        QStringList movingIds{ id };
        for(int cursor=0;cursor<movingIds.size();++cursor)
            for(const Part& candidate:parts_)
                if(candidate.assemblyJointParentBodyId==movingIds[cursor]&&!movingIds.contains(candidate.id))movingIds.append(candidate.id);
        // Existing world-axis rotation can be represented as another yaw only
        // for world Z. Reject X/Y on already yaw-placed occurrences rather
        // than silently invalidating their persisted placement metadata.
        for(const Part& target:parts_){
            if(!movingIds.contains(target.id)||target.componentDefinitionId.isEmpty())continue;
            const bool unsupported=(axis==Axis::X&&(target.placementYawDegrees!=0.0||target.placementPitchDegrees!=0.0))||
                (axis==Axis::Y&&target.placementYawDegrees!=0.0);
            const double destinationAngle=axis==Axis::X?target.placementRollDegrees+degrees:
                axis==Axis::Y?target.placementPitchDegrees+degrees:target.placementYawDegrees+degrees;
            if(unsupported||!std::isfinite(destinationAngle)||std::abs(destinationAngle)>36000.0){
                setError(error,"This world-axis rotation cannot be represented by the occurrence's saved orientation.");return false;
            }
        }
        struct RotationUpdate{int index;TopoDS_Shape shape;};QVector<RotationUpdate> updates;updates.reserve(movingIds.size());
        for(const QString& movingId:movingIds){
            int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==movingId){index=i;break;}
            if(index<0){setError(error,"A jointed occurrence is missing from the current assembly.");return false;}
            const TopoDS_Shape result=BRepBuilderAPI_Transform(parts_[index].shape,transform,true).Shape();
            const Part& target=parts_[index];
            if(target.isSurfaceBody?!validSurfaceBodyShape(result,error):!validShape(result,error))return false;
            if(!target.componentDefinitionId.isEmpty()){
                const gp_Pnt position=gp_Pnt(target.placementX,target.placementY,target.placementZ).Transformed(transform);
                if(!finiteOffset(position.X())||!finiteOffset(position.Y())||!finiteOffset(position.Z())){
                    setError(error,"Assembly occurrence placement would exceed the supported coordinate range.");return false;
                }
            }
            if(target.assemblyJointType==QStringLiteral("Revolute")||target.assemblyJointType==QStringLiteral("Prismatic")){
                const gp_Pnt origin=gp_Pnt(target.jointOriginX,target.jointOriginY,target.jointOriginZ).Transformed(transform);
                if(!finiteOffset(origin.X())||!finiteOffset(origin.Y())||!finiteOffset(origin.Z())){
                    setError(error,"Assembly joint origin would exceed the supported coordinate range.");return false;
                }
            }
            updates.append({index,result});
        }
        QVector<Part> candidate=parts_;
        for(const RotationUpdate& update:updates){
            Part& target=candidate[update.index];target.shape=update.shape;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
            if(!target.componentDefinitionId.isEmpty()){
                const gp_Pnt position=gp_Pnt(target.placementX,target.placementY,target.placementZ).Transformed(transform);
                target.placementX=position.X();target.placementY=position.Y();target.placementZ=position.Z();
                if(axis==Axis::X)target.placementRollDegrees+=degrees;
                else if(axis==Axis::Y)target.placementPitchDegrees+=degrees;
                else target.placementYawDegrees+=degrees;
            }
            if(target.assemblyJointType==QStringLiteral("Revolute")||target.assemblyJointType==QStringLiteral("Prismatic")){
                const gp_Pnt origin=gp_Pnt(target.jointOriginX,target.jointOriginY,target.jointOriginZ).Transformed(transform);
                gp_Vec jointAxis(target.jointAxisX,target.jointAxisY,target.jointAxisZ);jointAxis.Transform(transform);
                const gp_Dir normalizedAxis(jointAxis);
                target.jointOriginX=origin.X();target.jointOriginY=origin.Y();target.jointOriginZ=origin.Z();
                target.jointAxisX=normalizedAxis.X();target.jointAxisY=normalizedAxis.Y();target.jointAxisZ=normalizedAxis.Z();
            }
        }
        if(!propagateLinkedBodies(&candidate,error))return false;
        checkpoint();parts_=std::move(candidate);
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::scale(const QString& id, double factor, QString* error) {
    if (!std::isfinite(factor) || factor < 1e-4 || factor > 1e4) { setError(error, "Scale must be 0.0001â€“10,000."); return false; }
    const Part* part = find(id);
    if (!part) { setError(error, "Object not found."); return false; }
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature){setError(error,"Edit a linked Boolean source rather than scaling the derived result.");return false;}
    if(!part->linkedFoldedFlatSourceId.isEmpty()){setError(error,"Associative flat blanks are read-only; edit the folded source.");return false;}
    if (factor == 1.0) return true;
    try {
        gp_Trsf transform;
        transform.SetScale(gp_Pnt(0,0,0), factor);
        const TopoDS_Shape result = BRepBuilderAPI_Transform(part->shape, transform, true).Shape();
        if(part->isSurfaceBody?!validSurfaceBodyShape(result,error):!validShape(result,error))return false;
        QVector<Part> candidate=parts_;
        for(auto& target:candidate)if(target.id==id){target.shape=result;target.geometryToken=generatedId();break;}
        if(!propagateLinkedBodies(&candidate,error))return false;
        checkpoint();parts_=std::move(candidate);
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::scaleNonUniform(const QString& id,double factorX,double factorY,double factorZ,QString* error) {
    constexpr double minimumFactor=1e-4,maximumFactor=1e4;
    const auto validFactor=[=](double factor){return std::isfinite(factor)&&factor>=minimumFactor&&factor<=maximumFactor;};
    if(!validFactor(factorX)||!validFactor(factorY)||!validFactor(factorZ)){
        setError(error,"Nonuniform scale factors must be finite and between 0.0001 and 10,000.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature){setError(error,"Edit a linked Boolean source rather than scaling its result.");return false;}
    if(!part->linkedFoldedFlatSourceId.isEmpty()){setError(error,"Associative flat blanks are read-only; edit the folded source.");return false;}
    if(factorX==1.0&&factorY==1.0&&factorZ==1.0)return true;
    try{
        const gp_GTrsf transform(gp_Mat(factorX,0,0,0,factorY,0,0,0,factorZ),gp_XYZ(0,0,0));
        const TopoDS_Shape result=BRepBuilderAPI_GTransform(part->shape,transform,true).Shape();
        if(part->isSurfaceBody?!validSurfaceBodyShape(result,error):!validShape(result,error))return false;
        QVector<Part> candidate=parts_;
        for(auto& target:candidate)if(target.id==id){target.shape=result;target.geometryToken=generatedId();break;}
        if(!propagateLinkedBodies(&candidate,error))return false;
        checkpoint();parts_=std::move(candidate);
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::duplicate(const QString& id, double dx, double dy, double dz, QString* error) {
    const Part* part = find(id);
    if (!part) { setError(error, "Object not found."); return false; }
    if (!finiteOffset(dx) || !finiteOffset(dy) || !finiteOffset(dz)) { setError(error, "Duplicate offset out of range."); return false; }
    if (!part->componentDefinitionId.isEmpty()&&hasDrivenRevoluteAncestor(*part,parts_)) {
        setError(error,"Reset revolute joints to zero before duplicating a component occurrence."); return false;
    }
    if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
    if (!part->componentDefinitionId.isEmpty() &&
        (!finiteOffset(part->placementX+dx) || !finiteOffset(part->placementY+dy) ||
         !finiteOffset(part->placementZ+dz))) {
        setError(error, "Duplicated occurrence placement would exceed the supported coordinate range."); return false;
    }
    try {
        gp_Trsf trsf;
        trsf.SetTranslation(gp_Vec(dx,dy,dz));
        const TopoDS_Shape result = BRepBuilderAPI_Transform(part->shape, trsf, true).Shape();
        if(part->isSurfaceBody?!validSurfaceBodyShape(result,error):!validShape(result,error))return false;
        Part copied{generatedId(), uniqueName(part->name + " Copy"), result, part->visible, part->color};
        copied.isSurfaceBody=part->isSurfaceBody;
        inheritMaterial(copied, *part);
        // A new occurrence shares the source component's BOM identity.
        if (!part->componentDefinitionId.isEmpty()) {
            copied.partNumber=part->partNumber; copied.description=part->description;
            copied.revision=part->revision; copied.assemblyPath=part->assemblyPath;
            copied.procurementType=part->procurementType; copied.supplier=part->supplier;
            copied.supplierPartNumber=part->supplierPartNumber;
        }
        copied.componentDefinitionId = part->componentDefinitionId;
        copied.componentName = part->componentName;
        if (!copied.componentDefinitionId.isEmpty()) {
            copied.occurrenceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            copied.placementX = part->placementX + dx; copied.placementY = part->placementY + dy; copied.placementZ = part->placementZ + dz;
            copied.placementYawDegrees = part->placementYawDegrees;
            copied.placementPitchDegrees = part->placementPitchDegrees;
            copied.placementRollDegrees = part->placementRollDegrees;
        }
        checkpoint();
        parts_.append(copied);
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::makeComponent(const QString& id,const QString& componentName,QString* error) {
    Part* part = nullptr;
    for (Part& candidate : parts_) if (candidate.id == id) { part = &candidate; break; }
    const QString name = componentName.trimmed();
    if (!part) { setError(error,"Object not found."); return false; }
    if (name.isEmpty() || name.size() > 120 || name.contains('\n') || name.contains('\r')) { setError(error,"Component name must contain 1 to 120 single-line characters."); return false; }
    if (!part->componentDefinitionId.isEmpty()) { setError(error,"This body is already a component occurrence."); return false; }
    checkpoint();
    part->componentDefinitionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    part->componentName = name;
    part->occurrenceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    part->placementX = part->placementY = part->placementZ = 0.0;
    return true;
}

bool SceneDocument::insertComponentOccurrence(const QString& sourceBodyId,double x,double y,double z,QString* error) {
    const Part* source = find(sourceBodyId);
    if (!source) { setError(error,"Source body not found."); return false; }
    if (source->componentDefinitionId.isEmpty()) { setError(error,"Make the source body a component before inserting occurrences."); return false; }
    if (hasDrivenRevoluteAncestor(*source,parts_)) {
        setError(error,"Reset revolute joints to zero before inserting from a component occurrence."); return false;
    }
    if (!finiteOffset(x) || !finiteOffset(y) || !finiteOffset(z)) { setError(error,"Occurrence placement is out of range."); return false; }
    if (parts_.size() >= kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    try {
        // Remove the source's stored rigid placement before placing a fresh,
        // unrotated occurrence at the requested world XYZ position.
        const TopoDS_Shape local=BRepBuilderAPI_Transform(source->shape,freeOccurrencePlacement(*source).Inverted(),true).Shape();
        gp_Trsf destination;destination.SetTranslation(gp_Vec(x,y,z));
        const TopoDS_Shape placed=BRepBuilderAPI_Transform(local,destination,true).Shape();
        if (!validShape(placed,error)) return false;
        Part occurrence{generatedId(),uniqueName(source->componentName),placed,source->visible,source->color};
        inheritMaterial(occurrence,*source);
        // Copy BOM identity with the definition so a new instance groups correctly.
        occurrence.partNumber=source->partNumber; occurrence.description=source->description;
        occurrence.revision=source->revision; occurrence.assemblyPath=source->assemblyPath;
        occurrence.procurementType=source->procurementType; occurrence.supplier=source->supplier;
        occurrence.supplierPartNumber=source->supplierPartNumber;
        occurrence.componentDefinitionId=source->componentDefinitionId;
        occurrence.componentName=source->componentName;
        occurrence.occurrenceId=QUuid::createUuid().toString(QUuid::WithoutBraces);
        occurrence.placementX=x; occurrence.placementY=y; occurrence.placementZ=z;
        checkpoint(); parts_.append(occurrence);
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::setFreeOccurrenceYaw(const QString& bodyId,double degrees,QString* error) {
    const Part* current=find(bodyId);
    return setFreeOccurrenceOrientation(bodyId,current?current->placementRollDegrees:0.0,
                                        current?current->placementPitchDegrees:0.0,degrees,error);
}

bool SceneDocument::setFreeOccurrenceOrientation(const QString& bodyId,double rollDegrees,double pitchDegrees,
                                                  double yawDegrees,QString* error) {
    if(!std::isfinite(rollDegrees)||!std::isfinite(pitchDegrees)||!std::isfinite(yawDegrees)||
       std::abs(rollDegrees)>36000.0||std::abs(pitchDegrees)>36000.0||std::abs(yawDegrees)>36000.0){
        setError(error,"Occurrence roll, pitch, and yaw must be finite and within +/-36,000 degrees.");return false;
    }
    Part* source=nullptr;
    for(Part& part:parts_)if(part.id==bodyId){source=&part;break;}
    if(!source||source->componentDefinitionId.isEmpty()){
        setError(error,"Select a component occurrence before setting its orientation.");return false;
    }
    // Do not confuse independent placement orientation with saved joint poses.
    if(!source->assemblyJointType.isEmpty()||
       std::any_of(parts_.cbegin(),parts_.cend(),[&](const Part& part){return part.assemblyJointParentBodyId==bodyId;})){
        setError(error,"Free occurrence orientation requires an occurrence without joints or joint descendants.");return false;
    }
    if(rollDegrees==source->placementRollDegrees&&pitchDegrees==source->placementPitchDegrees&&
       yawDegrees==source->placementYawDegrees)return true;
    try{
        const gp_Trsf oldPose=freeOccurrencePlacement(*source);
        Part proposed=*source;proposed.placementRollDegrees=rollDegrees;
        proposed.placementPitchDegrees=pitchDegrees;proposed.placementYawDegrees=yawDegrees;
        const TopoDS_Shape local=BRepBuilderAPI_Transform(source->shape,oldPose.Inverted(),true).Shape();
        const TopoDS_Shape changed=BRepBuilderAPI_Transform(local,freeOccurrencePlacement(proposed),true).Shape();
        if(!validShape(changed,error))return false;
        checkpoint();source->shape=changed;source->placementYawDegrees=yawDegrees;
        source->placementPitchDegrees=pitchDegrees;source->placementRollDegrees=rollDegrees;
        source->geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Occurrence orientation failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::synchronizeComponentOccurrences(const QString& sourceBodyId,QString* error) {
    const Part* source=find(sourceBodyId);
    if(!source||source->componentDefinitionId.isEmpty()){
        setError(error,"Select a saved component occurrence to update its reusable definition.");return false;
    }
    QVector<int> targets;
    for(int i=0;i<parts_.size();++i)
        if(parts_[i].componentDefinitionId==source->componentDefinitionId&&parts_[i].id!=sourceBodyId)targets.append(i);
    if(targets.isEmpty())return true;
    // Translation-only synchronization cannot preserve a driven revolute pose.
    if(hasDrivenRevoluteAncestor(*source,parts_)){
        setError(error,"Reset revolute joints to zero before synchronizing component occurrences.");return false;
    }
    for(int index:targets)if(hasDrivenRevoluteAncestor(parts_[index],parts_)){
        setError(error,"Reset revolute joints to zero before synchronizing component occurrences.");return false;
    }
    try{
        const TopoDS_Shape local=BRepBuilderAPI_Transform(source->shape,freeOccurrencePlacement(*source).Inverted(),true).Shape();
        if(!validShape(local,error))return false;
        struct Update{int index;TopoDS_Shape shape;};QVector<Update> updates;updates.reserve(targets.size());
        for(int index:targets){
            const Part& occurrence=parts_[index];
            const gp_Trsf place=freeOccurrencePlacement(occurrence);
            const TopoDS_Shape updated=BRepBuilderAPI_Transform(local,place,true).Shape();
            if(!validShape(updated,error))return false;
            updates.append({index,updated});
        }
        checkpoint();
        for(const Update& update:updates){
            Part& occurrence=parts_[update.index];occurrence.shape=update.shape;
            occurrence.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
            // Reusable instances share the definition's BOM and material data
            // as well as its geometry. Placement and visibility remain local
            // to each occurrence.
            inheritMaterial(occurrence,*source);
            occurrence.componentName=source->componentName;
            occurrence.color=source->color;
        }
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Component-definition update failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::linearPatternComponentOccurrences(const QString& sourceBodyId,int instanceCount,
        double spacingX,double spacingY,double spacingZ,QString* error) {
    const Part* source=find(sourceBodyId);
    if(!source||source->componentDefinitionId.isEmpty()||source->isSurfaceBody){
        setError(error,"Linear component patterns require one solid component occurrence.");return false;
    }
    if(instanceCount<2||instanceCount>100||!finiteOffset(spacingX)||!finiteOffset(spacingY)||!finiteOffset(spacingZ)||
       (spacingX==0.0&&spacingY==0.0&&spacingZ==0.0)){
        setError(error,"Choose 2 to 100 total occurrences and a nonzero finite XYZ spacing vector.");return false;
    }
    if(!source->assemblyJointType.isEmpty()||hasDrivenRevoluteAncestor(*source,parts_)||
       std::any_of(parts_.cbegin(),parts_.cend(),[&](const Part& part){return part.assemblyJointParentBodyId==sourceBodyId;})){
        setError(error,"Pattern a free component occurrence without joint links or joint descendants.");return false;
    }
    if(parts_.size()+instanceCount-1>kMaxObjects){setError(error,"The component pattern exceeds the scene object limit.");return false;}
    const double lastX=source->placementX+spacingX*(instanceCount-1);
    const double lastY=source->placementY+spacingY*(instanceCount-1);
    const double lastZ=source->placementZ+spacingZ*(instanceCount-1);
    if(!finiteOffset(lastX)||!finiteOffset(lastY)||!finiteOffset(lastZ)){
        setError(error,"The last patterned occurrence would exceed the supported coordinate range.");return false;
    }
    try{
        QVector<Part> occurrences;occurrences.reserve(instanceCount-1);
        for(int index=1;index<instanceCount;++index){
            const double dx=spacingX*index,dy=spacingY*index,dz=spacingZ*index;
            gp_Trsf placement;placement.SetTranslation(gp_Vec(dx,dy,dz));
            const TopoDS_Shape shape=BRepBuilderAPI_Transform(source->shape,placement,true).Shape();
            if(!validShape(shape,error))return false;
            Part occurrence{generatedId(),uniqueName(QStringLiteral("%1 Pattern %2").arg(source->name).arg(index+1)),
                            shape,source->visible,source->color};
            inheritMaterial(occurrence,*source);
            occurrence.componentDefinitionId=source->componentDefinitionId;
            occurrence.componentName=source->componentName;
            occurrence.occurrenceId=QUuid::createUuid().toString(QUuid::WithoutBraces);
            occurrence.placementX=source->placementX+dx;occurrence.placementY=source->placementY+dy;
            occurrence.placementZ=source->placementZ+dz;occurrence.placementYawDegrees=source->placementYawDegrees;
            occurrence.placementPitchDegrees=source->placementPitchDegrees;
            occurrence.placementRollDegrees=source->placementRollDegrees;
            occurrences.append(std::move(occurrence));
        }
        checkpoint();for(Part& occurrence:occurrences)parts_.append(std::move(occurrence));return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Linear component pattern failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::circularPatternComponentOccurrences(const QString& sourceBodyId,int instanceCount,
        double centerX,double centerY,double centerZ,double angleStepDegrees,QString* error) {
    const Part* source=find(sourceBodyId);
    if(!source||source->componentDefinitionId.isEmpty()||source->isSurfaceBody){
        setError(error,"Circular component patterns require one solid component occurrence.");return false;
    }
    if(instanceCount<2||instanceCount>100||!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(centerZ)||
       !std::isfinite(angleStepDegrees)||angleStepDegrees==0.0||std::abs(angleStepDegrees)>360.0||
       !std::isfinite(source->placementYawDegrees+angleStepDegrees*(instanceCount-1))||
       std::abs(source->placementYawDegrees+angleStepDegrees*(instanceCount-1))>36000.0){
        setError(error,"Choose 2 to 100 total occurrences, a valid axis center, and a nonzero Z-axis angle step within ±360 degrees.");return false;
    }
    if(!source->assemblyJointType.isEmpty()||hasDrivenRevoluteAncestor(*source,parts_)||
       std::any_of(parts_.cbegin(),parts_.cend(),[&](const Part& part){return part.assemblyJointParentBodyId==sourceBodyId;})){
        setError(error,"Pattern a free component occurrence without joint links or joint descendants.");return false;
    }
    if(parts_.size()+instanceCount-1>kMaxObjects){setError(error,"The component pattern exceeds the scene object limit.");return false;}
    try{
        QVector<Part> occurrences;occurrences.reserve(instanceCount-1);
        const gp_Pnt center(centerX,centerY,centerZ);const gp_Ax1 axis(center,gp_Dir(0,0,1));
        for(int index=1;index<instanceCount;++index){
            const double angle=angleStepDegrees*index;
            gp_Trsf rotation;rotation.SetRotation(axis,angle*std::numbers::pi/180.0);
            const TopoDS_Shape shape=BRepBuilderAPI_Transform(source->shape,rotation,true).Shape();
            if(!validShape(shape,error))return false;
            gp_Pnt position(source->placementX,source->placementY,source->placementZ);position.Transform(rotation);
            const double yaw=source->placementYawDegrees+angle;
            if(!finiteOffset(position.X())||!finiteOffset(position.Y())||!finiteOffset(position.Z())||std::abs(yaw)>36000.0){
                setError(error,"A circularly patterned occurrence would exceed the supported placement range.");return false;
            }
            Part occurrence{generatedId(),uniqueName(QStringLiteral("%1 Circular Pattern %2").arg(source->name).arg(index+1)),
                            shape,source->visible,source->color};
            inheritMaterial(occurrence,*source);
            occurrence.componentDefinitionId=source->componentDefinitionId;
            occurrence.componentName=source->componentName;
            occurrence.occurrenceId=QUuid::createUuid().toString(QUuid::WithoutBraces);
            occurrence.placementX=position.X();occurrence.placementY=position.Y();occurrence.placementZ=position.Z();
            occurrence.placementYawDegrees=yaw;
            occurrence.placementPitchDegrees=source->placementPitchDegrees;
            occurrence.placementRollDegrees=source->placementRollDegrees;
            occurrences.append(std::move(occurrence));
        }
        checkpoint();for(Part& occurrence:occurrences)parts_.append(std::move(occurrence));return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Circular component pattern failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addRigidJoint(const QString& parentBodyId,const QString& childBodyId,QString* error) {
    Part* parent=nullptr;Part* child=nullptr;
    for(Part& part:parts_){if(part.id==parentBodyId)parent=&part;if(part.id==childBodyId)child=&part;}
    if(!parent||!child||parent==child||parent->componentDefinitionId.isEmpty()||child->componentDefinitionId.isEmpty()){
        setError(error,"Rigid joints require two different component occurrences in the current scene.");return false;
    }
    if(!child->assemblyJointType.isEmpty()) {setError(error,"The child occurrence already belongs to a joint.");return false;}
    for(const Part* cursor=parent;cursor;){
        if(cursor->id==childBodyId){setError(error,"Assembly joints cannot create a cycle.");return false;}
        if(cursor->assemblyJointParentBodyId.isEmpty())break;
        cursor=find(cursor->assemblyJointParentBodyId);
    }
    checkpoint();child->assemblyJointType=QStringLiteral("Rigid");child->assemblyJointParentBodyId=parentBodyId;
    return true;
}

bool SceneDocument::addRevoluteJoint(const QString& parentBodyId,const QString& childBodyId,
        double originX,double originY,double originZ,double axisX,double axisY,double axisZ,
        double minimumDegrees,double maximumDegrees,QString* error) {
    Part* parent=nullptr;Part* child=nullptr;
    for(Part& part:parts_){if(part.id==parentBodyId)parent=&part;if(part.id==childBodyId)child=&part;}
    const double axisLength=std::sqrt(axisX*axisX+axisY*axisY+axisZ*axisZ);
    if(!parent||!child||parent==child||parent->componentDefinitionId.isEmpty()||child->componentDefinitionId.isEmpty()){
        setError(error,"Revolute joints require two different component occurrences in the current scene.");return false;
    }
    if(!child->assemblyJointType.isEmpty()){setError(error,"The child occurrence already belongs to a joint.");return false;}
    if(!finiteOffset(originX)||!finiteOffset(originY)||!finiteOffset(originZ)||!std::isfinite(axisLength)||axisLength<1e-9||
       !std::isfinite(minimumDegrees)||!std::isfinite(maximumDegrees)||minimumDegrees<-3600||maximumDegrees>3600||
       minimumDegrees>=maximumDegrees||minimumDegrees>0.0||maximumDegrees<0.0){
        // A new joint starts at zero degrees. Excluding zero creates an in-memory
        // joint that the scene loader would reject after save/reopen.
        setError(error,"Revolute joint axis, origin, or angle limits are invalid; limits must include zero degrees.");return false;
    }
    for(const Part* cursor=parent;cursor;){
        if(cursor->id==childBodyId){setError(error,"Assembly joints cannot create a cycle.");return false;}
        if(cursor->assemblyJointParentBodyId.isEmpty())break;
        cursor=find(cursor->assemblyJointParentBodyId);
    }
    checkpoint();child->assemblyJointType=QStringLiteral("Revolute");child->assemblyJointParentBodyId=parentBodyId;
    child->jointOriginX=originX;child->jointOriginY=originY;child->jointOriginZ=originZ;
    child->jointAxisX=axisX/axisLength;child->jointAxisY=axisY/axisLength;child->jointAxisZ=axisZ/axisLength;
    child->jointAngleDegrees=0;child->jointMinimumDegrees=minimumDegrees;child->jointMaximumDegrees=maximumDegrees;
    return true;
}

bool SceneDocument::driveRevoluteJoint(const QString& childBodyId,double angleDegrees,QString* error) {
    Part* child=nullptr;for(Part& part:parts_)if(part.id==childBodyId){child=&part;break;}
    if(!child||child->assemblyJointType!=QStringLiteral("Revolute")||child->assemblyJointParentBodyId.isEmpty()){
        setError(error,"Select an occurrence with a saved revolute joint.");return false;
    }
    if(!std::isfinite(angleDegrees)||angleDegrees<child->jointMinimumDegrees||angleDegrees>child->jointMaximumDegrees){
        setError(error,"Requested joint angle is outside the saved limits.");return false;
    }
    const double delta=(angleDegrees-child->jointAngleDegrees)*std::numbers::pi/180.0;
    if(std::abs(delta)<1e-12)return true;
    try{
        gp_Trsf rotation;rotation.SetRotation(gp_Ax1(gp_Pnt(child->jointOriginX,child->jointOriginY,child->jointOriginZ),
            gp_Dir(child->jointAxisX,child->jointAxisY,child->jointAxisZ)),delta);
        QStringList movingIds{childBodyId};
        for(int cursor=0;cursor<movingIds.size();++cursor)
            for(const Part& candidate:parts_)
                if(candidate.assemblyJointParentBodyId==movingIds[cursor]&&!movingIds.contains(candidate.id))movingIds.append(candidate.id);
        struct Update{int index;TopoDS_Shape shape;gp_Pnt placement;gp_Pnt jointOrigin;gp_Dir jointAxis;};
        QVector<Update> updates;updates.reserve(movingIds.size());
        for(const QString& id:movingIds){
            int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==id){index=i;break;}
            if(index<0){setError(error,"A jointed occurrence is missing from the current assembly.");return false;}
            const Part& target=parts_[index];const TopoDS_Shape moved=BRepBuilderAPI_Transform(target.shape,rotation,true).Shape();
            if(!validShape(moved,error))return false;
            gp_Pnt placement(target.placementX,target.placementY,target.placementZ);
            if(!target.componentDefinitionId.isEmpty()){
                placement.Transform(rotation);
                if(!finiteOffset(placement.X())||!finiteOffset(placement.Y())||!finiteOffset(placement.Z())){
                    setError(error,"Assembly occurrence placement would exceed the supported coordinate range.");return false;
                }
            }
            gp_Pnt jointOrigin(target.jointOriginX,target.jointOriginY,target.jointOriginZ);
            gp_Dir jointAxis(target.jointAxisX,target.jointAxisY,target.jointAxisZ);
            if(id!=childBodyId&&!target.assemblyJointType.isEmpty()){
                jointOrigin.Transform(rotation);
                if(!finiteOffset(jointOrigin.X())||!finiteOffset(jointOrigin.Y())||!finiteOffset(jointOrigin.Z())){
                    setError(error,"Nested joint origin would exceed the supported coordinate range.");return false;
                }
                gp_Vec axis(target.jointAxisX,target.jointAxisY,target.jointAxisZ);
                axis.Transform(rotation);jointAxis=gp_Dir(axis);
            }
            updates.append({index,moved,placement,jointOrigin,jointAxis});
        }
        checkpoint();
        for(const Update& update:updates){
            Part& target=parts_[update.index];target.shape=update.shape;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
            if(!target.componentDefinitionId.isEmpty()){
                target.placementX=update.placement.X();target.placementY=update.placement.Y();target.placementZ=update.placement.Z();
            }
            if(target.id!=childBodyId&&!target.assemblyJointType.isEmpty()){
                target.jointOriginX=update.jointOrigin.X();target.jointOriginY=update.jointOrigin.Y();target.jointOriginZ=update.jointOrigin.Z();
                target.jointAxisX=update.jointAxis.X();target.jointAxisY=update.jointAxis.Y();target.jointAxisZ=update.jointAxis.Z();
            }
        }
        child->jointAngleDegrees=angleDegrees;
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Revolute joint solve failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addPrismaticJoint(const QString& parentBodyId,const QString& childBodyId,
        double axisX,double axisY,double axisZ,double minimumMm,double maximumMm,QString* error) {
    Part* parent=nullptr;Part* child=nullptr;
    for(Part& part:parts_){if(part.id==parentBodyId)parent=&part;if(part.id==childBodyId)child=&part;}
    const double axisLength=std::sqrt(axisX*axisX+axisY*axisY+axisZ*axisZ);
    if(!parent||!child||parent==child||parent->componentDefinitionId.isEmpty()||child->componentDefinitionId.isEmpty()){
        setError(error,"Prismatic joints require two different component occurrences in the current scene.");return false;
    }
    if(!child->assemblyJointType.isEmpty()){setError(error,"The child occurrence already belongs to a joint.");return false;}
    if(!finiteOffset(parent->placementX)||!finiteOffset(parent->placementY)||!finiteOffset(parent->placementZ)){
        setError(error,"Prismatic joint origin would exceed the supported coordinate range.");return false;
    }
    if(!std::isfinite(axisLength)||axisLength<1e-9||!std::isfinite(minimumMm)||!std::isfinite(maximumMm)||
       minimumMm<-1e6||maximumMm>1e6||minimumMm>=0.0||maximumMm<=0.0||minimumMm>=maximumMm){
        setError(error,"Prismatic joint axis or travel limits are invalid; limits must include the current zero position.");return false;
    }
    for(const Part* cursor=parent;cursor;){
        if(cursor->id==childBodyId){setError(error,"Assembly joints cannot create a cycle.");return false;}
        if(cursor->assemblyJointParentBodyId.isEmpty())break;
        cursor=find(cursor->assemblyJointParentBodyId);
    }
    checkpoint();child->assemblyJointType=QStringLiteral("Prismatic");child->assemblyJointParentBodyId=parentBodyId;
    child->jointOriginX=parent->placementX;child->jointOriginY=parent->placementY;child->jointOriginZ=parent->placementZ;
    child->jointAxisX=axisX/axisLength;child->jointAxisY=axisY/axisLength;child->jointAxisZ=axisZ/axisLength;
    child->jointAngleDegrees=0.0;child->jointMinimumDegrees=-180.0;child->jointMaximumDegrees=180.0;
    child->jointTranslationMm=0.0;child->jointMinimumMm=minimumMm;child->jointMaximumMm=maximumMm;
    return true;
}

bool SceneDocument::drivePrismaticJoint(const QString& childBodyId,double offsetMm,QString* error) {
    Part* child=nullptr;for(Part& part:parts_)if(part.id==childBodyId){child=&part;break;}
    if(!child||child->assemblyJointType!=QStringLiteral("Prismatic")||child->assemblyJointParentBodyId.isEmpty()){
        setError(error,"Select an occurrence with a saved prismatic joint.");return false;
    }
    if(!std::isfinite(offsetMm)||offsetMm<child->jointMinimumMm||offsetMm>child->jointMaximumMm){
        setError(error,"Requested prismatic offset is outside the saved travel limits.");return false;
    }
    const double delta=offsetMm-child->jointTranslationMm;
    if(std::abs(delta)<1e-12)return true;
    const double dx=child->jointAxisX*delta,dy=child->jointAxisY*delta,dz=child->jointAxisZ*delta;
    if(!finiteOffset(dx)||!finiteOffset(dy)||!finiteOffset(dz)){setError(error,"Prismatic joint displacement exceeds the supported coordinate range.");return false;}
    try{
        gp_Trsf translation;translation.SetTranslation(gp_Vec(dx,dy,dz));
        QStringList movingIds{childBodyId};
        for(int cursor=0;cursor<movingIds.size();++cursor)
            for(const Part& candidate:parts_)
                if(candidate.assemblyJointParentBodyId==movingIds[cursor]&&!movingIds.contains(candidate.id))movingIds.append(candidate.id);
        struct Update{int index;TopoDS_Shape shape;gp_Pnt placement;};QVector<Update> updates;updates.reserve(movingIds.size());
        for(const QString& id:movingIds){
            int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==id){index=i;break;}
            if(index<0){setError(error,"A jointed occurrence is missing from the current assembly.");return false;}
            const Part& target=parts_[index];const TopoDS_Shape moved=BRepBuilderAPI_Transform(target.shape,translation,true).Shape();
            if(!validShape(moved,error))return false;
            gp_Pnt placement(target.placementX,target.placementY,target.placementZ);
            if(!target.componentDefinitionId.isEmpty()){
                placement.Transform(translation);
                if(!finiteOffset(placement.X())||!finiteOffset(placement.Y())||!finiteOffset(placement.Z())){
                    setError(error,"Assembly occurrence placement would exceed the supported coordinate range.");return false;
                }
            }
            if(id!=childBodyId&&!target.assemblyJointType.isEmpty()&&
               (!finiteOffset(target.jointOriginX+dx)||!finiteOffset(target.jointOriginY+dy)||!finiteOffset(target.jointOriginZ+dz))){
                setError(error,"Nested joint origin would exceed the supported coordinate range.");return false;
            }
            updates.append({index,moved,placement});
        }
        checkpoint();
        for(const Update& update:updates){
            Part& target=parts_[update.index];target.shape=update.shape;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
            if(!target.componentDefinitionId.isEmpty()){
                target.placementX=update.placement.X();target.placementY=update.placement.Y();target.placementZ=update.placement.Z();
            }
            // The driven joint remains anchored to its parent. Nested joint frames travel with this subassembly.
            if(target.id!=childBodyId&&!target.assemblyJointType.isEmpty()){
                target.jointOriginX+=dx;target.jointOriginY+=dy;target.jointOriginZ+=dz;
            }
        }
        child->jointTranslationMm=offsetMm;
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Prismatic joint solve failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::solveAssemblyPose(const QMap<QString,double>& jointPositions,QString* error) {
    if(jointPositions.isEmpty()||jointPositions.size()>500){
        setError(error,"Provide between one and 500 joint targets for an assembly pose.");return false;
    }
    struct Target{QString id;int depth=0;};QVector<Target> targets;targets.reserve(jointPositions.size());
    for(auto it=jointPositions.cbegin();it!=jointPositions.cend();++it){
        const Part* part=find(it.key());
        if(!part||(part->assemblyJointType!="Revolute"&&part->assemblyJointType!="Prismatic")||!std::isfinite(it.value())){
            setError(error,"Every assembly-pose target must reference a movable joint and have a finite value.");return false;
        }
        int depth=0;const Part* cursor=part;QSet<QString> visited;
        while(cursor&&!cursor->assemblyJointParentBodyId.isEmpty()){
            if(visited.contains(cursor->id)){setError(error,"Assembly-pose solve found a joint cycle.");return false;}
            visited.insert(cursor->id);cursor=find(cursor->assemblyJointParentBodyId);++depth;
            if(!cursor){setError(error,"Assembly-pose solve found a missing parent occurrence.");return false;}
        }
        targets.append({it.key(),depth});
    }
    std::sort(targets.begin(),targets.end(),[](const Target& a,const Target& b){return a.depth==b.depth?a.id<b.id:a.depth<b.depth;});
    SceneDocument candidate;candidate.parts_=parts_;
    for(const Target& target:targets){
        const Part* part=candidate.find(target.id);QString candidateError;bool applied=false;
        if(part->assemblyJointType=="Revolute")applied=candidate.driveRevoluteJoint(target.id,jointPositions.value(target.id),&candidateError);
        else applied=candidate.drivePrismaticJoint(target.id,jointPositions.value(target.id),&candidateError);
        if(!applied){setError(error,QStringLiteral("Assembly pose rejected at '%1': %2").arg(target.id,candidateError));return false;}
    }
    bool changed=false;
    for(const Target& target:targets){
        const Part* before=find(target.id);const Part* after=candidate.find(target.id);
        if(before->jointAngleDegrees!=after->jointAngleDegrees||before->jointTranslationMm!=after->jointTranslationMm){changed=true;break;}
    }
    if(!changed)return true;
    checkpoint();parts_=std::move(candidate.parts_);return true;
}

bool SceneDocument::solveAssemblyMates(const QString& parentId,const QString& childId,
                                       const std::vector<mates::Constraint>& constraints,
                                       bool attachRigidJoint,QString* error){
    const Part* parent=find(parentId);
    const Part* child=find(childId);
    if(!parent||!child||parentId==childId||parent->componentDefinitionId.isEmpty()||
       child->componentDefinitionId.isEmpty()||!child->assemblyJointType.isEmpty()||
       std::any_of(parts_.cbegin(),parts_.cend(),[&](const Part& part){
           return part.assemblyJointParentBodyId==childId;
       })||hasDrivenRevoluteAncestor(*child,parts_)){
        setError(error,"Mate solve needs a distinct parent and a free component child with no jointed descendants.");return false;
    }
    std::string reason;mates::Transform solution;
    if(!mates::solve(constraints,&solution,&reason)){
        setError(error,QString::fromStdString(reason));return false;
    }
    try{
        const auto& r=solution.rotation;
        gp_Trsf delta;
        delta.SetValues(r[0],r[1],r[2],solution.translation.x,
                        r[3],r[4],r[5],solution.translation.y,
                        r[6],r[7],r[8],solution.translation.z);
        const TopoDS_Shape moved=BRepBuilderAPI_Transform(child->shape,delta,true).Shape();
        if(!validShape(moved,error))return false;
        const gp_Trsf newPose=delta.Multiplied(freeOccurrencePlacement(*child));
        const gp_XYZ origin=newPose.TranslationPart();
        const double r11=newPose.Value(1,1),r21=newPose.Value(2,1),r31=newPose.Value(3,1);
        const double r12=newPose.Value(1,2),r22=newPose.Value(2,2);
        const double r32=newPose.Value(3,2),r33=newPose.Value(3,3);
        const double pitch=std::asin(std::clamp(-r31,-1.0,1.0));
        const double cp=std::cos(pitch);
        const double yaw=std::abs(cp)>1e-8?std::atan2(r21,r11):std::atan2(-r12,r22);
        const double roll=std::abs(cp)>1e-8?std::atan2(r32,r33):0.0;
        const double deg=180.0/std::numbers::pi;
        if(!finiteOffset(origin.X())||!finiteOffset(origin.Y())||!finiteOffset(origin.Z())||
           !std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(roll)){
            setError(error,"Mate solve produced an unsupported component placement.");return false;
        }
        QVector<Part> candidate=parts_;
        for(Part& part:candidate)if(part.id==childId){
            part.shape=moved;part.geometryToken=generatedId();
            part.placementX=origin.X();part.placementY=origin.Y();part.placementZ=origin.Z();
            part.placementYawDegrees=yaw*deg;part.placementPitchDegrees=pitch*deg;
            part.placementRollDegrees=roll*deg;
            if(attachRigidJoint){
                part.assemblyJointType=QStringLiteral("Rigid");
                part.assemblyJointParentBodyId=parentId;
            }
            break;
        }
        // Prove placement metadata replays the same rigid pose on save/reopen.
        const Part* updated=nullptr;
        for(const auto& part:candidate)if(part.id==childId){updated=&part;break;}
        const gp_Trsf reconstructed=freeOccurrencePlacement(*updated);
        for(int row=1;row<=3;++row)for(int column=1;column<=4;++column)
            if(std::abs(reconstructed.Value(row,column)-newPose.Value(row,column))>1e-5){
                setError(error,"Solved orientation cannot be represented in persistent yaw/pitch/roll placement.");return false;
            }
        checkpoint();parts_=std::move(candidate);return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Assembly mate geometry failed: %1").arg(failure.what()));
    }catch(const std::exception& failure){setError(error,QString::fromUtf8(failure.what()));}
    return false;
}

bool SceneDocument::solveAssemblyMateNetwork(const QString& fixedBodyId,
        const QVector<AssemblyNetworkMate>& connections,QString* error){
    if(fixedBodyId.isEmpty()||connections.isEmpty()||connections.size()>128){
        setError(error,"Provide a fixed component and 1-128 fully constrained mate connections.");return false;
    }
    QStringList ids{fixedBodyId};
    for(const auto& connection:connections){
        if(connection.parentBodyId.isEmpty()||connection.childBodyId.isEmpty()||
           connection.parentBodyId==connection.childBodyId){
            setError(error,"Mate network has a missing or self-linked component.");return false;
        }
        if(!ids.contains(connection.parentBodyId))ids.append(connection.parentBodyId);
        if(!ids.contains(connection.childBodyId))ids.append(connection.childBodyId);
    }
    if(ids.size()>32||ids.size()<2){setError(error,"Rigid mate networks require 2-32 component occurrences.");return false;}
    std::vector<mates::NetworkConnection> graph;
    graph.reserve(static_cast<std::size_t>(connections.size()));
    for(const auto& connection:connections){
        graph.push_back({static_cast<std::size_t>(ids.indexOf(connection.parentBodyId)),
                         static_cast<std::size_t>(ids.indexOf(connection.childBodyId)),
                         connection.constraints});
    }
    for(const QString& id:ids){
        const Part* part=find(id);
        if(!part||part->componentDefinitionId.isEmpty()||
           !part->assemblyJointType.isEmpty()||hasDrivenRevoluteAncestor(*part,parts_)||
           std::any_of(parts_.cbegin(),parts_.cend(),[&](const Part& candidate){
               return candidate.assemblyJointParentBodyId==id;
           })){
            setError(error,"All network components must exist as independent free occurrences without jointed descendants.");return false;
        }
    }
    mates::NetworkSolution solution;std::string detail;
    if(!mates::solveRigidNetwork(static_cast<std::size_t>(ids.size()),0,graph,&solution,&detail)){
        setError(error,QString::fromStdString(detail));return false;
    }
    // A refresh that is already satisfied must not invent new B-rep tokens,
    // create an undo entry, or invalidate dependent drawings.
    const auto needsMovement=[&](const mates::Transform& correction){
        if(std::abs(correction.translation.x)>1e-5||
           std::abs(correction.translation.y)>1e-5||
           std::abs(correction.translation.z)>1e-5)return true;
        for(int row=0;row<3;++row)for(int col=0;col<3;++col)
            if(std::abs(correction.rotation[3*row+col]-(row==col?1.0:0.0))>1e-7)
                return true;
        return false;
    };
    if(std::none_of(solution.worldCorrections.cbegin()+1,
                    solution.worldCorrections.cend(),needsMovement))return true;
    try{
        QVector<Part> candidate=parts_;
        const double degrees=180.0/std::numbers::pi;
        bool changed=false;
        for(int index=1;index<ids.size();++index){
            const QString& id=ids[index];
            auto at=std::find_if(candidate.begin(),candidate.end(),[&](const Part& part){return part.id==id;});
            if(at==candidate.end()){setError(error,"Assembly component disappeared during staged solve.");return false;}
            const auto& correction=solution.worldCorrections[static_cast<std::size_t>(index)];
            const auto& r=correction.rotation;
            gp_Trsf delta;
            delta.SetValues(r[0],r[1],r[2],correction.translation.x,
                            r[3],r[4],r[5],correction.translation.y,
                            r[6],r[7],r[8],correction.translation.z);
            const gp_Trsf newPose=delta.Multiplied(freeOccurrencePlacement(*at));
            const gp_XYZ translation=newPose.TranslationPart();
            const double r11=newPose.Value(1,1),r21=newPose.Value(2,1),r31=newPose.Value(3,1);
            const double r12=newPose.Value(1,2),r22=newPose.Value(2,2);
            const double r32=newPose.Value(3,2),r33=newPose.Value(3,3);
            const double pitch=std::asin(std::clamp(-r31,-1.0,1.0));
            const double cp=std::cos(pitch);
            const double yaw=std::abs(cp)>1e-8?std::atan2(r21,r11):std::atan2(-r12,r22);
            const double roll=std::abs(cp)>1e-8?std::atan2(r32,r33):0.0;
            if(!finiteOffset(translation.X())||!finiteOffset(translation.Y())||
               !finiteOffset(translation.Z())||!std::isfinite(pitch)||
               !std::isfinite(roll)||!std::isfinite(yaw)){
                setError(error,"Network mate placement exceeds supported coordinates.");return false;
            }
            const TopoDS_Shape moved=BRepBuilderAPI_Transform(at->shape,delta,true).Shape();
            if(!validShape(moved,error))return false;
            at->shape=moved;
            at->placementX=translation.X();at->placementY=translation.Y();at->placementZ=translation.Z();
            at->placementYawDegrees=yaw*degrees;at->placementPitchDegrees=pitch*degrees;
            at->placementRollDegrees=roll*degrees;
            const gp_Trsf roundTrip=freeOccurrencePlacement(*at);
            for(int row=1;row<=3;++row)for(int col=1;col<=4;++col)
                if(std::abs(roundTrip.Value(row,col)-newPose.Value(row,col))>1e-5){
                    setError(error,"Mate network placement cannot be serialized with current Euler coordinates.");return false;
                }
            at->geometryToken=generatedId();
            changed=true;
        }
        if(!changed)return true;
        // Do not move the pinned component; commit only after *all* B-reps and
        // persistent placements have been verified in this staged copy.
        checkpoint();parts_=std::move(candidate);return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Assembly mate network failed in OCCT: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QString::fromUtf8(failure.what()));
    }
    return false;
}

// Convert world observations into the original component-local frames. The
// definitions then remain attached to each occurrence as placements change.
// No inference about persistent face identity is made by these frame mates.
namespace {
mates::Transform toMateFrame(const gp_Trsf& frame){
    mates::Transform out;
    out.translation={frame.Value(1,4),frame.Value(2,4),frame.Value(3,4)};
    out.rotation={{frame.Value(1,1),frame.Value(1,2),frame.Value(1,3),
                   frame.Value(2,1),frame.Value(2,2),frame.Value(2,3),
                   frame.Value(3,1),frame.Value(3,2),frame.Value(3,3)}};
    return out;
}
mates::Vec3 matePoint(const gp_Trsf& frame,mates::Vec3 v){
    gp_Pnt point(v.x,v.y,v.z);point.Transform(frame);
    return {point.X(),point.Y(),point.Z()};
}
mates::Vec3 mateAxis(const gp_Trsf& frame,mates::Vec3 v){
    gp_Vec axis(v.x,v.y,v.z);axis.Transform(frame);
    return {axis.X(),axis.Y(),axis.Z()};
}
}

bool SceneDocument::setLiveRigidMateNetwork(const QString& fixedId,
        const QVector<AssemblyNetworkMate>& worldConnections,QString* error){
    if(hasLiveArticulatedNetwork()){
        setError(error,"Remove the saved articulated network before creating a rigid mate graph.");return false;
    }
    if(fixedId.isEmpty()||worldConnections.isEmpty()||worldConnections.size()>128){
        setError(error,"A fixed occurrence and 1-128 connections are required.");return false;
    }
    QVector<AssemblyNetworkMate> local=worldConnections;
    try{
        for(AssemblyNetworkMate& connection:local){
            const Part* parent=find(connection.parentBodyId);
            const Part* child=find(connection.childBodyId);
            if(!parent||!child||connection.constraints.empty()||connection.constraints.size()>32){
                setError(error,"Live mates reference a missing occurrence or invalid constraint count.");return false;
            }
            const gp_Trsf parentToLocal=freeOccurrencePlacement(*parent).Inverted();
            const gp_Trsf childToLocal=freeOccurrencePlacement(*child).Inverted();
            for(auto& c:connection.constraints){
                if(c.kind!=mates::Kind::Point&&c.kind!=mates::Kind::CoaxialAxis&&
                   c.kind!=mates::Kind::CoincidentPlane){
                    setError(error,"Invalid live-mate constraint kind.");return false;
                }
                c.parentPoint=matePoint(parentToLocal,c.parentPoint);
                c.childPoint=matePoint(childToLocal,c.childPoint);
                if(c.kind!=mates::Kind::Point){
                    c.parentDirection=mateAxis(parentToLocal,c.parentDirection);
                    c.childDirection=mateAxis(childToLocal,c.childDirection);
                }
            }
        }
    }catch(const Standard_Failure& e){
        setError(error,QStringLiteral("Cannot capture local mate frames: %1").arg(e.what()));return false;
    }catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));return false;}
    const quint64 initialRevision=revision_;
    // The existing transactional OCCT path validates every mate and commits
    // all solved occurrences together, or none of them.
    if(!solveAssemblyMateNetwork(fixedId,worldConnections,error))return false;
    if(revision_==initialRevision)checkpoint(); // changed definitions, unchanged geometry
    liveRigidMateFixed_=fixedId;
    liveRigidMates_=std::move(local);
    return true;
}

// Materialize the currently saved local observations as world observations.
// This helper is read-only; committing or changing definitions happens only
// after validation by the existing atomic rigid network solver.
namespace {
bool liveMateWorldObservations(const QVector<Part>& parts,
                               const QVector<SceneDocument::AssemblyNetworkMate>& local,
                               QVector<SceneDocument::AssemblyNetworkMate>* world,
                               QString* error){
    if(!world)return false;
    QVector<SceneDocument::AssemblyNetworkMate> projected=local;
    const auto partFor=[&](const QString& id)->const Part*{
        for(const auto& part:parts)if(part.id==id)return &part;
        return nullptr;
    };
    try{
        for(auto& link:projected){
            const Part* parent=partFor(link.parentBodyId);
            const Part* child=partFor(link.childBodyId);
            if(!parent||!child){if(error)*error="A saved mate references a removed occurrence.";return false;}
            const gp_Trsf pw=freeOccurrencePlacement(*parent);
            const gp_Trsf cw=freeOccurrencePlacement(*child);
            for(auto& constraint:link.constraints){
                constraint.parentPoint=matePoint(pw,constraint.parentPoint);
                constraint.childPoint=matePoint(cw,constraint.childPoint);
                if(constraint.kind!=mates::Kind::Point){
                    constraint.parentDirection=mateAxis(pw,constraint.parentDirection);
                    constraint.childDirection=mateAxis(cw,constraint.childDirection);
                }
            }
        }
    }catch(const Standard_Failure& e){
        if(error)*error=QStringLiteral("Cannot project saved mate frames: %1").arg(e.what());return false;
    }catch(const std::exception& e){if(error)*error=QString::fromUtf8(e.what());return false;}
    *world=std::move(projected);return true;
}
}

bool SceneDocument::liveRigidMateWorldDefinitions(QVector<AssemblyNetworkMate>* world,
        QString* error) const{
    if(!hasLiveRigidMateNetwork()||!world){
        setError(error,"A saved rigid mate network and a valid output are required.");return false;
    }
    return liveMateWorldObservations(parts_,liveRigidMates_,world,error);
}

namespace {
bool mateObservationsExactlyEqual(const SceneDocument::AssemblyNetworkMate& a,
                                  const SceneDocument::AssemblyNetworkMate& b){
    if(a.parentBodyId!=b.parentBodyId||a.childBodyId!=b.childBodyId||
       a.constraints.size()!=b.constraints.size())return false;
    const auto equal=[](mates::Vec3 x,mates::Vec3 y){
        return x.x==y.x&&x.y==y.y&&x.z==y.z;
    };
    for(std::size_t i=0;i<a.constraints.size();++i){
        const auto& x=a.constraints[i];
        const auto& y=b.constraints[i];
        if(x.kind!=y.kind||x.offsetMm!=y.offsetMm||
           !equal(x.parentPoint,y.parentPoint)||
           !equal(x.childPoint,y.childPoint))return false;
        if(x.kind!=mates::Kind::Point&&
           (!equal(x.parentDirection,y.parentDirection)||
            !equal(x.childDirection,y.childDirection)))return false;
    }
    return true;
}
}

bool SceneDocument::previewLiveRigidMateConnectionEdit(int index,
        const AssemblyNetworkMate& proposed,QString* report,QString* error,
        double requiredClearanceMm) const{
    if(!fit::validThreshold(requiredClearanceMm)){
        setError(error,"Mate preview clearance must be between 0 and 1,000,000 mm.");return false;
    }
    if(!report||!hasLiveRigidMateNetwork()||index<0||index>=liveRigidMates_.size()){
        setError(error,"Choose a valid saved mate connection and preview destination.");return false;
    }
    const auto& old=liveRigidMates_.at(index);
    if(proposed.parentBodyId!=old.parentBodyId||proposed.childBodyId!=old.childBodyId){
        setError(error,"Editing cannot silently rebind mate endpoints.");return false;
    }
    QVector<AssemblyNetworkMate> world;
    if(!liveRigidMateWorldDefinitions(&world,error))return false;
    world[index]=proposed;
    QStringList ids{liveRigidMateFixed_};
    std::vector<mates::NetworkConnection> edges;
    edges.reserve(static_cast<std::size_t>(world.size()));
    for(const auto& connection:world){
        if(connection.parentBodyId.isEmpty()||connection.childBodyId.isEmpty()||
           connection.parentBodyId==connection.childBodyId){
            setError(error,"Mate preview contains an invalid component pair.");return false;
        }
        if(!ids.contains(connection.parentBodyId))ids.append(connection.parentBodyId);
        if(!ids.contains(connection.childBodyId))ids.append(connection.childBodyId);
    }
    if(ids.size()<2||ids.size()>32){
        setError(error,"Mate preview supports 2-32 connected occurrences.");return false;
    }
    for(const auto& id:ids){
        const Part* part=find(id);
        if(!part||part->componentDefinitionId.isEmpty()||
           !part->assemblyJointType.isEmpty()||hasDrivenRevoluteAncestor(*part,parts_)||
           std::any_of(parts_.cbegin(),parts_.cend(),[&](const Part& other){
               return other.assemblyJointParentBodyId==id;
           })){
            setError(error,"Mate preview requires independent free component occurrences.");return false;
        }
    }
    for(const auto& connection:world){
        edges.push_back({static_cast<std::size_t>(ids.indexOf(connection.parentBodyId)),
                         static_cast<std::size_t>(ids.indexOf(connection.childBodyId)),
                         connection.constraints});
    }
    mates::NetworkSolution solution;std::string detail;
    if(!mates::solveRigidNetwork(static_cast<std::size_t>(ids.size()),0,edges,&solution,&detail)){
        setError(error,QString::fromStdString(detail));return false;
    }
    QStringList lines;
    lines << "READ-ONLY KINEMATIC PREVIEW: nominal geometry check, NOT continuous collision certification";
    int moved=0;
    try{
        for(int j=1;j<ids.size();++j){
            const Part* part=find(ids.at(j));
            const auto& delta=solution.worldCorrections.at(static_cast<std::size_t>(j));
            gp_Pnt origin(0,0,0);
            origin.Transform(freeOccurrencePlacement(*part));
            const mates::Vec3 original{origin.X(),origin.Y(),origin.Z()};
            const auto next=mates::apply(delta,original);
            const double x=next.x-original.x,y=next.y-original.y,z=next.z-original.z;
            const double distance=std::sqrt(x*x+y*y+z*z);
            const auto& a=delta.rotation;
            const double cosine=std::clamp((a[0]+a[4]+a[8]-1.0)/2.0,-1.0,1.0);
            const double degrees=std::acos(cosine)*180.0/std::numbers::pi;
            if(!std::isfinite(distance)||!std::isfinite(degrees)||distance>1.0e6){
                setError(error,"Kinematic preview exceeds supported placement bounds.");return false;
            }
            if(distance>1e-5||degrees>1e-5)++moved;
            lines << QString("%1 (%2): origin displacement %3 mm, rotation %4 degrees")
                 .arg(part->name,ids.at(j),QString::number(distance,'f',6),
                      QString::number(degrees,'f',6));
        }
    }catch(const Standard_Failure& e){
        setError(error,QStringLiteral("Cannot preview occurrence placement: %1").arg(e.what()));return false;
    }catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));return false;}
    lines.prepend(QString("Predicted moving occurrences: %1 / %2")
                       .arg(moved).arg(ids.size()-1));
    lines << QString("Worst mate-loop closure residual: %1 mm-equivalent")
                 .arg(solution.worstClosureResidualMm,0,'g',10);
    lines << QString("Worst linear gap: %1 mm; largest axis error: %2 degrees")
                 .arg(solution.worstPointOrAxisGapMm,0,'g',10)
                 .arg(solution.worstAngularErrorDegrees,0,'g',10);
    // Read-only connection diagnostics are returned only when the entire
    // network is consistent. An inconsistent edit never commits any poses.
    for(std::size_t i=0;i<solution.perConnection.size()&&i<8;++i){
        const auto& closure=solution.perConnection[i];
        lines << QString("Mate connection %1: mixed %2 mm-equivalent; linear %3 mm; angular %4 deg")
            .arg(i+1)
            .arg(closure.maximumMixedResidualMm,0,'g',8)
            .arg(closure.maximumLinearGapMm,0,'g',8)
            .arg(closure.maximumAngularErrorDegrees,0,'g',8);
    }
    if(solution.perConnection.size()>8)
        lines << QString("%1 additional connections omitted from preview summary.")
                    .arg(solution.perConnection.size()-8);
    // Materialize the proposed rigid solution into a detached scene. Neither
    // the OCCT solids nor the source document's revision/undo history change.
    // Every moved B-rep is validated before the preview can be reported.
    try{
        SceneDocument staged;
        staged.parts_=parts_;
        for(int j=1;j<ids.size();++j){
            auto stagedPart=std::find_if(staged.parts_.begin(),staged.parts_.end(),
                [&](const Part& candidate){return candidate.id==ids.at(j);});
            if(stagedPart==staged.parts_.end()){
                setError(error,"Mate preview component was not found in its staged scene.");return false;
            }
            const auto& correction=solution.worldCorrections.at(static_cast<std::size_t>(j));
            const auto& r=correction.rotation;
            gp_Trsf delta;
            delta.SetValues(r[0],r[1],r[2],correction.translation.x,
                            r[3],r[4],r[5],correction.translation.y,
                            r[6],r[7],r[8],correction.translation.z);
            const TopoDS_Shape moved=BRepBuilderAPI_Transform(stagedPart->shape,delta,true).Shape();
            if(!validShape(moved,error))return false;
            stagedPart->shape=moved;
        }
        lines << "GEOMETRY PREFLIGHT: all predicted occurrence solids passed B-rep validation.";
        // Exact pairwise OCCT checks grow quadratically. Larger graphs still
        // get B-rep validation but are explicitly NOT collision-certified.
        constexpr int maxFitPreviewBodies=12;
        if(ids.size()<=maxFitPreviewBodies){
            QVector<AssemblyFitPair> fitPairs;
            if(!inspectAssemblyFit(staged,ids,requiredClearanceMm,&fitPairs,error))return false;
            int overlaps=0,contacts=0,belowRequired=0;
            double smallestPositiveGap=std::numeric_limits<double>::infinity();
            QStringList overlappingNames;
            for(const auto& pair:fitPairs){
                if(pair.status==fit::Status::Interference){
                    ++overlaps;
                    if(overlappingNames.size()<5)
                        overlappingNames << QString("%1 / %2: %3 mm^3")
                            .arg(pair.firstName,pair.secondName,
                                 QString::number(pair.overlapVolumeMm3,'g',10));
                }else if(pair.status==fit::Status::Contact){
                    ++contacts;if(requiredClearanceMm>0)++belowRequired;
                }
                else if(pair.status==fit::Status::InsufficientClearance)++belowRequired;
                else smallestPositiveGap=std::min(smallestPositiveGap,pair.distanceMm);
            }
            lines << QString("NOMINAL FIT PREVIEW: %1 exact pair checks; %2 interference pairs; %3 contact pairs; %4 below-required-clearance pairs (required %5 mm).")
                         .arg(fitPairs.size()).arg(overlaps).arg(contacts).arg(belowRequired)
                         .arg(requiredClearanceMm,0,'g',10);
            if(std::isfinite(smallestPositiveGap))
                lines << QString("Minimum separated-pair gap: %1 mm")
                             .arg(smallestPositiveGap,0,'g',10);
            // Preserve numeric evidence when a required-clearance threshold
            // is exactly met by one pair while another pair falls short.
            // The summary must count only genuinely below-limit nominal pairs.
            for(const auto& pair:fitPairs)
                lines << QString("PAIR: %1 / %2 | nominal gap %3 mm | %4")
                    .arg(pair.firstName,pair.secondName)
                    .arg(pair.distanceMm,0,'g',12)
                    .arg(QString::fromLatin1(fit::statusName(pair.status)));
            for(const auto& row:overlappingNames)lines << QString("OVERLAP: %1").arg(row);
            if(overlaps>overlappingNames.size())
                lines << QString("Additional overlap pairs: %1").arg(overlaps-overlappingNames.size());
        }else{
            lines << QString("NOMINAL FIT PREVIEW SKIPPED: %1 bodies exceed the 12-body exact-pair preview budget.")
                         .arg(ids.size());
        }
    }catch(const Standard_Failure& e){
        setError(error,QStringLiteral("Predicted geometry preflight failed in OCCT: %1").arg(e.what()));return false;
    }catch(const std::exception& e){
        setError(error,QString("Predicted geometry preflight failed: %1").arg(QString::fromUtf8(e.what())));return false;
    }
    lines << "Nominal overlap may be intentional; no automatic interference veto is applied."
          << "The committed pose will repeat B-rep validation. This does not certify swept motion, manufacturing tolerances, or future geometry.";
    *report=lines.join('\n');
    return true;
}

bool SceneDocument::replaceLiveRigidMateConnection(int index,
        const AssemblyNetworkMate& newWorldConnection,QString* error){
    if(!hasLiveRigidMateNetwork()||index<0||index>=liveRigidMates_.size()){
        setError(error,"Choose an existing saved rigid mate connection to edit.");return false;
    }
    const auto& original=liveRigidMates_.at(index);
    if(newWorldConnection.parentBodyId!=original.parentBodyId||
       newWorldConnection.childBodyId!=original.childBodyId){
        setError(error,"Mate editing must retain the original occurrence pair; rebind separately.");
        return false;
    }
    if(newWorldConnection.constraints.empty()||newWorldConnection.constraints.size()>32){
        setError(error,"An edited mate must have 1-32 constraints.");return false;
    }
    QVector<AssemblyNetworkMate> world;
    if(!liveRigidMateWorldDefinitions(&world,error))return false;
    // Re-opening the editor and accepting without changing any observation
    // must not create a new revision, B-rep token or undo entry.
    if(mateObservationsExactlyEqual(world.at(index),newWorldConnection))return true;
    world[index]=newWorldConnection;
    // Existing network solver checks closure, conflicting constraints and
    // placement representability before a single atomic document commit.
    // It also recaptures every constraint in its own component-local frame.
    return setLiveRigidMateNetwork(liveRigidMateFixed_,world,error);
}

bool SceneDocument::appendLiveRigidMateConnection(const AssemblyNetworkMate& connection,QString* error){
    if(!hasLiveRigidMateNetwork()){
        setError(error,"Save an initial rigid mate with a fixed component before adding connections.");return false;
    }
    if(liveRigidMates_.size()>=128||connection.parentBodyId==connection.childBodyId||
       connection.parentBodyId.isEmpty()||connection.childBodyId.isEmpty()){
        setError(error,"A mate network supports up to 128 valid non-self connections.");return false;
    }
    for(const auto& existing:liveRigidMates_){
        if((existing.parentBodyId==connection.parentBodyId&&existing.childBodyId==connection.childBodyId)||
           (existing.parentBodyId==connection.childBodyId&&existing.childBodyId==connection.parentBodyId)){
            setError(error,"This component pair already has a saved connection. Remove it before reauthoring.");return false;
        }
    }
    QVector<AssemblyNetworkMate> world;
    if(!liveMateWorldObservations(parts_,liveRigidMates_,&world,error))return false;
    world.append(connection);
    // setLiveRigidMateNetwork captures ALL observations into local frames,
    // validates the entire closed-loop graph, and commits in one transaction.
    return setLiveRigidMateNetwork(liveRigidMateFixed_,world,error);
}

bool SceneDocument::removeLiveRigidMateConnection(int index,QString* error){
    if(!hasLiveRigidMateNetwork()||index<0||index>=liveRigidMates_.size()){
        setError(error,"Choose an existing saved rigid mate connection.");return false;
    }
    if(liveRigidMates_.size()==1)return removeLiveRigidMateNetwork();
    QVector<AssemblyNetworkMate> world;
    if(!liveMateWorldObservations(parts_,liveRigidMates_,&world,error))return false;
    world.removeAt(index);
    QStringList ids{liveRigidMateFixed_};
    std::vector<mates::NetworkConnection> graph;
    for(const auto& link:world){
        if(!ids.contains(link.parentBodyId))ids.append(link.parentBodyId);
        if(!ids.contains(link.childBodyId))ids.append(link.childBodyId);
    }
    // Do not silently drop a formerly constrained occurrence when its only
    // connecting edge is removed. Explicitly remove the entire network first.
    QSet<QString> previouslyConstrained{liveRigidMateFixed_};
    for(const auto& link:liveRigidMates_){
        previouslyConstrained.insert(link.parentBodyId);
        previouslyConstrained.insert(link.childBodyId);
    }
    if(ids.size()!=previouslyConstrained.size()||ids.size()>32){
        setError(error,"Removing this connection would detach a saved component or exceed graph limits.");return false;
    }
    for(const auto& link:world){
        graph.push_back({static_cast<std::size_t>(ids.indexOf(link.parentBodyId)),
                         static_cast<std::size_t>(ids.indexOf(link.childBodyId)),link.constraints});
    }
    mates::NetworkSolution checked;std::string issue;
    if(!mates::solveRigidNetwork(static_cast<std::size_t>(ids.size()),0,graph,&checked,&issue)){
        setError(error,QStringLiteral("Removing this connection would leave an invalid network: ")+
                       QString::fromStdString(issue));return false;
    }
    checkpoint();liveRigidMates_.removeAt(index);
    if(error)error->clear();return true;
}

bool SceneDocument::refreshLiveRigidMateNetwork(QString* error){
    if(liveRigidMateFixed_.isEmpty()||liveRigidMates_.isEmpty()){
        setError(error,"This document has no saved live rigid mate network.");return false;
    }
    QVector<AssemblyNetworkMate> world;
    if(!liveMateWorldObservations(parts_,liveRigidMates_,&world,error))return false;
    return solveAssemblyMateNetwork(liveRigidMateFixed_,world,error);
}

bool SceneDocument::removeLiveRigidMateNetwork(){
    if(liveRigidMateFixed_.isEmpty())return false;
    checkpoint();liveRigidMateFixed_.clear();liveRigidMates_.clear();return true;
}

bool SceneDocument::solveArticulatedAssemblyNetwork(const QString& fixedBodyId,
        const QVector<ArticulatedNetworkJoint>& joints,QVector<double>* coordinates,QString* error){
    if(fixedBodyId.isEmpty()||joints.isEmpty()||joints.size()>64){
        setError(error,"Provide a fixed occurrence and 1-64 articulated joint definitions.");return false;
    }
    QStringList ids{fixedBodyId};
    for(const auto& joint:joints){
        if(joint.parentBodyId.isEmpty()||joint.childBodyId.isEmpty()||
           joint.parentBodyId==joint.childBodyId){
            setError(error,"Articulated joint has missing or self-linked occurrences.");return false;
        }
        if(!ids.contains(joint.parentBodyId))ids.append(joint.parentBodyId);
        if(!ids.contains(joint.childBodyId))ids.append(joint.childBodyId);
    }
    if(ids.size()>24){setError(error,"Articulated networks support at most 24 occurrences.");return false;}
    std::vector<articulated::Joint> model;model.reserve(static_cast<std::size_t>(joints.size()));
    for(const auto& joint:joints){
        articulated::Joint link;
        link.parent=static_cast<std::size_t>(ids.indexOf(joint.parentBodyId));
        link.child=static_cast<std::size_t>(ids.indexOf(joint.childBodyId));
        link.kind=joint.kind;link.childAtZero=joint.childAtZero;link.axis=joint.axis;
        link.initial=joint.initial;link.minimum=joint.minimum;
        link.maximum=joint.maximum;link.driven=joint.driven;
        model.push_back(link);
    }
    for(const auto& id:ids){
        const Part* part=find(id);
        if(!part||part->componentDefinitionId.isEmpty()||
           !part->assemblyJointType.isEmpty()||hasDrivenRevoluteAncestor(*part,parts_)||
           std::any_of(parts_.cbegin(),parts_.cend(),[&](const Part& candidate){
               return candidate.assemblyJointParentBodyId==id;
           })){
            setError(error,"Articulated networks require independent free component occurrences.");return false;
        }
    }
    articulated::Solution solution;std::string detail;
    if(!articulated::solveNetwork(static_cast<std::size_t>(ids.size()),0,model,&solution,&detail)){
        setError(error,QString::fromStdString(detail));return false;
    }
    try{
        QVector<Part> candidate=parts_;
        const double degrees=180.0/std::numbers::pi;
        for(int index=1;index<ids.size();++index){
            const QString& id=ids[index];
            auto at=std::find_if(candidate.begin(),candidate.end(),[&](const Part& p){return p.id==id;});
            if(at==candidate.end()){setError(error,"Component disappeared during articulated solve.");return false;}
            const auto& correction=solution.poses[static_cast<std::size_t>(index)];
            const auto& r=correction.rotation;
            gp_Trsf delta;
            delta.SetValues(r[0],r[1],r[2],correction.translation.x,
                            r[3],r[4],r[5],correction.translation.y,
                            r[6],r[7],r[8],correction.translation.z);
            const gp_Trsf pose=delta.Multiplied(freeOccurrencePlacement(*at));
            const gp_XYZ translation=pose.TranslationPart();
            const double r11=pose.Value(1,1),r21=pose.Value(2,1),r31=pose.Value(3,1);
            const double r12=pose.Value(1,2),r22=pose.Value(2,2);
            const double r32=pose.Value(3,2),r33=pose.Value(3,3);
            const double pitch=std::asin(std::clamp(-r31,-1.0,1.0));
            const double cp=std::cos(pitch);
            const double yaw=std::abs(cp)>1e-8?std::atan2(r21,r11):std::atan2(-r12,r22);
            const double roll=std::abs(cp)>1e-8?std::atan2(r32,r33):0.0;
            if(!finiteOffset(translation.X())||!finiteOffset(translation.Y())||
               !finiteOffset(translation.Z())||!std::isfinite(pitch)||
               !std::isfinite(roll)||!std::isfinite(yaw)){
                setError(error,"Articulated result exceeds supported pose coordinates.");return false;
            }
            TopoDS_Shape moved=BRepBuilderAPI_Transform(at->shape,delta,true).Shape();
            if(!validShape(moved,error))return false;
            at->shape=moved;at->placementX=translation.X();at->placementY=translation.Y();
            at->placementZ=translation.Z();at->placementYawDegrees=yaw*degrees;
            at->placementPitchDegrees=pitch*degrees;at->placementRollDegrees=roll*degrees;
            const gp_Trsf reconstructed=freeOccurrencePlacement(*at);
            for(int row=1;row<=3;++row)for(int col=1;col<=4;++col)
                if(std::abs(reconstructed.Value(row,col)-pose.Value(row,col))>1e-5){
                    setError(error,"Articulated pose cannot be stored in persistent Euler coordinates.");return false;
                }
            at->geometryToken=generatedId();
        }
        // Output arguments and the live scene change only after all geometries
        // and round-trip persistence checks have passed.
        QVector<double> solved;
        solved.reserve(static_cast<qsizetype>(solution.coordinates.size()));
        for(double q:solution.coordinates)solved.append(q);
        checkpoint();parts_=std::move(candidate);
        if(coordinates)*coordinates=std::move(solved);
        return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Articulated OCCT placement failed: %1").arg(failure.what()));
    }catch(const std::exception& failure){setError(error,QString::fromUtf8(failure.what()));}
    return false;
}

// Persist the ORIGINAL reference poses before applying the first articulated
// solution. A later refresh solves absolute target poses from those references
// and the current fixed-occurrence frame; it never reapplies a stale delta.
bool SceneDocument::setLiveArticulatedNetwork(const QString& fixedId,
        const QVector<ArticulatedNetworkJoint>& joints,QString* error){
    if(fixedId.isEmpty()||joints.isEmpty()||joints.size()>64){
        setError(error,"An articulated network needs a fixed component and 1-64 joints.");return false;
    }
    if(hasLiveRigidMateNetwork()){
        setError(error,"Remove the existing live rigid network before saving an articulated one.");return false;
    }
    QStringList ids{fixedId};
    for(const auto& joint:joints){
        if(joint.parentBodyId.isEmpty()||joint.childBodyId.isEmpty()||
           joint.parentBodyId==joint.childBodyId){
            setError(error,"Articulated joint references are invalid.");return false;
        }
        if(!ids.contains(joint.parentBodyId))ids.append(joint.parentBodyId);
        if(!ids.contains(joint.childBodyId))ids.append(joint.childBodyId);
    }
    if(ids.size()<2||ids.size()>24){
        setError(error,"A saved articulated network supports 2-24 component occurrences.");return false;
    }
    QMap<QString,mates::Transform> reference;
    for(const auto& id:ids){
        const Part* part=find(id);
        if(!part||part->componentDefinitionId.isEmpty()||!part->assemblyJointType.isEmpty()){
            setError(error,"Articulated reference pose requires independent component occurrences.");return false;
        }
        reference.insert(id,toMateFrame(freeOccurrencePlacement(*part)));
    }
    // Existing native solver checks rank, loops, limits, OCCT placements and
    // commits every moved occurrence as one undo transaction or none.
    QVector<double> solved;
    if(!solveArticulatedAssemblyNetwork(fixedId,joints,&solved,error))return false;
    // The solve created a checkpoint. Definitions are attached to that exact
    // transaction and restored together with placements by undo/redo.
    liveArticulatedFixed_=fixedId;
    liveArticulatedJoints_=joints;
    liveArticulatedReferencePoses_=std::move(reference);
    if(error)error->clear();
    return true;
}

bool SceneDocument::stageLiveArticulatedRegeneration(
        const QVector<ArticulatedNetworkJoint>& definitions,QVector<Part>* stagedResult,
        bool* geometryChanged,QString* error) const{
    if(!stagedResult||!geometryChanged||!hasLiveArticulatedNetwork()||definitions.isEmpty()){
        setError(error,"There are no saved live articulated joints to refresh.");return false;
    }
    QStringList ids{liveArticulatedFixed_};
    std::vector<articulated::Joint> joints;
    joints.reserve(static_cast<size_t>(definitions.size()));
    for(const auto& j:definitions){
        if(!ids.contains(j.parentBodyId))ids.append(j.parentBodyId);
        if(!ids.contains(j.childBodyId))ids.append(j.childBodyId);
    }
    if(ids.size()!=liveArticulatedReferencePoses_.size()||ids.size()<2||ids.size()>24){
        setError(error,"Saved articulated reference poses do not match the joint graph.");return false;
    }
    for(const auto& j:definitions){
        joints.push_back({static_cast<size_t>(ids.indexOf(j.parentBodyId)),
                          static_cast<size_t>(ids.indexOf(j.childBodyId)),j.kind,j.childAtZero,
                          j.axis,j.initial,j.minimum,j.maximum,j.driven});
    }
    articulated::Solution solved;std::string detail;
    if(!articulated::solveNetwork(static_cast<size_t>(ids.size()),0,joints,&solved,&detail)){
        setError(error,QStringLiteral("Saved articulated joint network cannot be solved: ")+
                       QString::fromStdString(detail));return false;
    }
    const Part* fixed=find(liveArticulatedFixed_);
    if(!fixed||!liveArticulatedReferencePoses_.contains(liveArticulatedFixed_)){
        setError(error,"The fixed articulated occurrence or its reference pose is missing.");return false;
    }
    QVector<Part> staged=parts_;
    bool changed=false;
    const double degrees=180.0/std::numbers::pi;
    try{
        const auto anchor=mates::multiply(toMateFrame(freeOccurrencePlacement(*fixed)),
            mates::inverseRigid(liveArticulatedReferencePoses_.value(liveArticulatedFixed_)));
        for(int i=1;i<ids.size();++i){
            const QString id=ids[i];
            if(!liveArticulatedReferencePoses_.contains(id)){
                setError(error,"An articulated occurrence has no original reference pose.");return false;
            }
            const auto present=std::find_if(staged.begin(),staged.end(),[&](const Part& p){return p.id==id;});
            if(present==staged.end()||present->componentDefinitionId.isEmpty()||
               !present->assemblyJointType.isEmpty()){
                setError(error,"An articulated occurrence is missing or no longer free.");return false;
            }
            const auto desired=mates::multiply(anchor,mates::multiply(
                solved.poses[static_cast<size_t>(i)],liveArticulatedReferencePoses_.value(id)));
            const auto current=toMateFrame(freeOccurrencePlacement(*present));
            const auto correction=mates::multiply(desired,mates::inverseRigid(current));
            bool moved=std::abs(correction.translation.x)>1e-5||
                       std::abs(correction.translation.y)>1e-5||
                       std::abs(correction.translation.z)>1e-5;
            for(int r=0;r<3;++r)for(int c=0;c<3;++c)
                if(std::abs(correction.rotation[3*r+c]-(r==c?1.0:0.0))>1e-7)moved=true;
            if(!moved)continue;
            const auto& r=correction.rotation;
            gp_Trsf delta;
            delta.SetValues(r[0],r[1],r[2],correction.translation.x,
                            r[3],r[4],r[5],correction.translation.y,
                            r[6],r[7],r[8],correction.translation.z);
            const auto& target=desired.rotation;
            gp_Trsf world;
            world.SetValues(target[0],target[1],target[2],desired.translation.x,
                            target[3],target[4],target[5],desired.translation.y,
                            target[6],target[7],target[8],desired.translation.z);
            const double r11=world.Value(1,1),r21=world.Value(2,1),r31=world.Value(3,1);
            const double r12=world.Value(1,2),r22=world.Value(2,2);
            const double r32=world.Value(3,2),r33=world.Value(3,3);
            const double pitch=std::asin(std::clamp(-r31,-1.0,1.0));
            const double cp=std::cos(pitch);
            const double yaw=std::abs(cp)>1e-8?std::atan2(r21,r11):std::atan2(-r12,r22);
            const double roll=std::abs(cp)>1e-8?std::atan2(r32,r33):0.0;
            if(!std::isfinite(pitch)||!std::isfinite(yaw)||!std::isfinite(roll)||
               !finiteOffset(desired.translation.x)||!finiteOffset(desired.translation.y)||
               !finiteOffset(desired.translation.z)){
                setError(error,"Articulated refresh produced an unrepresentable placement.");return false;
            }
            const TopoDS_Shape movedShape=BRepBuilderAPI_Transform(present->shape,delta,true).Shape();
            if(!validShape(movedShape,error))return false;
            present->shape=movedShape;
            present->placementX=desired.translation.x;
            present->placementY=desired.translation.y;
            present->placementZ=desired.translation.z;
            present->placementYawDegrees=yaw*degrees;
            present->placementPitchDegrees=pitch*degrees;
            present->placementRollDegrees=roll*degrees;
            const gp_Trsf roundTrip=freeOccurrencePlacement(*present);
            for(int row=1;row<=3;++row)for(int col=1;col<=4;++col)
                if(std::abs(roundTrip.Value(row,col)-world.Value(row,col))>1e-5){
                    setError(error,"Articulated refresh placement cannot round-trip project persistence.");return false;
                }
            present->geometryToken=generatedId();
            changed=true;
        }
        *geometryChanged=changed;
        *stagedResult=std::move(staged);
        if(error)error->clear();return true;
    }catch(const Standard_Failure& e){
        setError(error,QStringLiteral("Articulated refresh failed in OpenCascade: %1").arg(e.what()));
    }catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::refreshLiveArticulatedNetwork(QString* error){
    QVector<Part> staged;bool changed=false;
    if(!stageLiveArticulatedRegeneration(liveArticulatedJoints_,&staged,&changed,error))return false;
    if(changed){checkpoint();parts_=std::move(staged);}
    if(error)error->clear();
    return true;
}

bool SceneDocument::previewLiveArticulatedJointCoordinate(int jointIndex,
        double requestedCoordinate,QString* report,QString* error,
        double requiredClearanceMm) const{
    if(!report||!fit::validThreshold(requiredClearanceMm)){
        setError(error,"Provide a preview report and a valid nonnegative clearance threshold.");return false;
    }
    if(!hasLiveArticulatedNetwork()||jointIndex<0||
       jointIndex>=liveArticulatedJoints_.size()||!std::isfinite(requestedCoordinate)){
        setError(error,"Choose a saved articulated joint and a finite target coordinate.");return false;
    }
    const auto& current=liveArticulatedJoints_[jointIndex];
    if(!current.driven||current.kind==articulated::JointKind::Fixed){
        setError(error,"Only driven revolute and prismatic joints can be previewed.");return false;
    }
    const double bound=current.kind==articulated::JointKind::Revolute?
        2.0*std::numbers::pi:1000.0;
    if(std::abs(requestedCoordinate)>bound){
        setError(error,"Joint coordinate is outside supported authoring bounds.");return false;
    }
    QVector<ArticulatedNetworkJoint> proposed=liveArticulatedJoints_;
    proposed[jointIndex].initial=requestedCoordinate;
    proposed[jointIndex].minimum=requestedCoordinate;
    proposed[jointIndex].maximum=requestedCoordinate;
    QVector<Part> stagedParts;bool geometryChanged=false;
    if(!stageLiveArticulatedRegeneration(proposed,&stagedParts,&geometryChanged,error))return false;
    QStringList ids{liveArticulatedFixed_};
    for(const auto& joint:proposed){
        if(!ids.contains(joint.parentBodyId))ids.append(joint.parentBodyId);
        if(!ids.contains(joint.childBodyId))ids.append(joint.childBodyId);
    }
    QStringList lines{
        "READ-ONLY ARTICULATED POSE PREVIEW: nominal solids only; NOT swept collision certification",
        QString("Driven joint %1 (%2): %3 -> %4")
            .arg(jointIndex+1)
            .arg(current.kind==articulated::JointKind::Revolute?"revolute (radians)":"prismatic (mm)")
            .arg(current.initial,0,'g',12).arg(requestedCoordinate,0,'g',12),
        QString("Regeneration: %1; network occurrences: %2")
            .arg(geometryChanged?"changed predicted solids":"no geometry change")
            .arg(ids.size()),
        "OCCT geometry preflight: proposed changed solids passed B-rep validity checks."
    };
    if(ids.size()<=12){
        SceneDocument staged;
        staged.parts_=std::move(stagedParts);
        QVector<AssemblyFitPair> pairs;
        if(!inspectAssemblyFit(staged,ids,requiredClearanceMm,&pairs,error))return false;
        int interferences=0,contacts=0,insufficient=0;
        double smallestGap=std::numeric_limits<double>::infinity();
        QStringList overlapLines;
        for(const auto& pair:pairs){
            if(pair.status==fit::Status::Interference){
                ++interferences;
                if(overlapLines.size()<5)overlapLines.append(
                    QString("INTERFERENCE %1 / %2: %3 mm^3")
                        .arg(pair.firstName,pair.secondName)
                        .arg(pair.overlapVolumeMm3,0,'g',10));
            }else if(pair.status==fit::Status::Contact){
                ++contacts;
                if(requiredClearanceMm>0)++insufficient;
            }else if(pair.status==fit::Status::InsufficientClearance){
                ++insufficient;
                smallestGap=std::min(smallestGap,pair.distanceMm);
            }else{
                smallestGap=std::min(smallestGap,pair.distanceMm);
            }
        }
        lines << QString("NOMINAL FIT: %1 exact pairs; %2 interferences; %3 contacts; %4 below required clearance (%5 mm).")
                  .arg(pairs.size()).arg(interferences).arg(contacts).arg(insufficient)
                  .arg(requiredClearanceMm,0,'g',10);
        if(std::isfinite(smallestGap))lines << QString("Minimum separated pair gap: %1 mm")
            .arg(smallestGap,0,'g',10);
        lines << overlapLines;
        if(interferences>overlapLines.size())lines << QString("Additional interferences: %1")
            .arg(interferences-overlapLines.size());
    }else{
        lines << QString("Exact pairwise fit NOT CHECKED: %1 occurrences exceed the 12-body preview budget.")
                    .arg(ids.size());
    }
    // Commit only after the entire preview succeeds. In particular, failures
    // from the solver or OCCT fit audit must preserve an older caller report.
    *report=lines.join('\n');
    if(error)error->clear();
    return true;
}

bool SceneDocument::updateLiveArticulatedJointCoordinate(int jointIndex,
        double requestedCoordinate,QString* error){
    if(!hasLiveArticulatedNetwork()||jointIndex<0||
       jointIndex>=liveArticulatedJoints_.size()||!std::isfinite(requestedCoordinate)){
        setError(error,"Choose a saved articulated joint and a finite target coordinate.");return false;
    }
    const auto& current=liveArticulatedJoints_[jointIndex];
    if(!current.driven||current.kind==articulated::JointKind::Fixed){
        setError(error,"Only driven revolute and prismatic joint coordinates are editable.");return false;
    }
    // Hard authoring bounds avoid nonsensical mechanisms. Revolute units here
    // are radians, prismatic units millimetres; UI converts degrees to radians.
    const double bound=current.kind==articulated::JointKind::Revolute?
        2.0*std::numbers::pi:1000.0;
    if(std::abs(requestedCoordinate)>bound){
        setError(error,"Joint coordinate is outside supported authoring bounds.");return false;
    }
    if(std::abs(current.initial-requestedCoordinate)<=1e-12){
        if(error)error->clear();return true;
    }
    QVector<ArticulatedNetworkJoint> candidate=liveArticulatedJoints_;
    auto& edited=candidate[jointIndex];
    edited.initial=requestedCoordinate;
    // A driven coordinate is a prescribed value. Moving it must not silently
    // retain yesterday's fixed minimum/maximum as an impossible constraint.
    edited.minimum=requestedCoordinate;edited.maximum=requestedCoordinate;
    QVector<Part> staged;bool changed=false;
    if(!stageLiveArticulatedRegeneration(candidate,&staged,&changed,error))return false;
    // One undo entry captures BOTH old part placements and old joint values.
    // A failed rebuild never reaches this commit; a geometry no-op still
    // changes the design definition, and must therefore be undoable.
    checkpoint();
    if(changed)parts_=std::move(staged);
    liveArticulatedJoints_=std::move(candidate);
    if(error)error->clear();return true;
}

// Multi-drive editing is one scene transaction. All targets are validated,
// solved against the same original reference frames, and committed together.
// In particular a closed loop must never pass by changing its joints one by
// one, because an intermediate pose may be inconsistent or unattainable.
namespace {
bool applyArticulatedTargets(const QVector<SceneDocument::ArticulatedNetworkJoint>& original,
                             const QMap<int,double>& targets,
                             QVector<SceneDocument::ArticulatedNetworkJoint>* proposed,
                             bool* changed,QString* error){
    if(!proposed||!changed||targets.isEmpty()||targets.size()>64){
        if(error)*error="Select between 1 and 64 articulated drive coordinates.";return false;
    }
    auto next=original;bool any=false;
    for(auto it=targets.cbegin();it!=targets.cend();++it){
        if(it.key()<0||it.key()>=next.size()||!std::isfinite(it.value())){
            if(error)*error="Joint batch contains an invalid index or coordinate.";return false;
        }
        auto& joint=next[it.key()];
        if(!joint.driven||joint.kind==articulated::JointKind::Fixed){
            if(error)*error="Only driven revolute and prismatic joints support batch editing.";return false;
        }
        const double limit=joint.kind==articulated::JointKind::Revolute?
            2.0*std::numbers::pi:1000.0;
        if(std::abs(it.value())>limit){
            if(error)*error="Joint batch contains a coordinate outside supported authoring bounds.";return false;
        }
        if(joint.maximum-joint.minimum>1e-10){
            if(it.value()<joint.minimum-1e-12||it.value()>joint.maximum+1e-12){
                if(error)*error="Driven target lies outside the saved joint's explicit travel limits.";
                return false;
            }
        }else{
            // Legacy saved drives pin min/max to the previous target; allow
            // moving those drives, preserving their historic behavior.
            joint.minimum=it.value();joint.maximum=it.value();
        }
        any|=std::abs(joint.initial-it.value())>1e-12;
        joint.initial=it.value();
    }
    *proposed=std::move(next);*changed=any;return true;
}
}

bool SceneDocument::previewLiveArticulatedCoordinates(const QMap<int,double>& targets,
        QString* report,QString* error,double requiredClearanceMm,
        bool* hasUnsafeNominalPairs) const{
    if(!report||!fit::validThreshold(requiredClearanceMm)||!hasLiveArticulatedNetwork()){
        setError(error,"A saved mechanism, output report and valid clearance are required.");return false;
    }
    QVector<ArticulatedNetworkJoint> candidate;bool definitionsChanged=false;
    if(!applyArticulatedTargets(liveArticulatedJoints_,targets,&candidate,
                                &definitionsChanged,error))return false;
    QVector<Part> staged;bool changed=false;
    if(!stageLiveArticulatedRegeneration(candidate,&staged,&changed,error))return false;
    QStringList ids{liveArticulatedFixed_};
    for(const auto& joint:candidate){
        if(!ids.contains(joint.parentBodyId))ids.append(joint.parentBodyId);
        if(!ids.contains(joint.childBodyId))ids.append(joint.childBodyId);
    }
    QStringList lines{
        "READ-ONLY MULTI-DRIVE PREVIEW: all coordinates solved together; no document mutation",
        QString("Saved joints: %1; selected drives: %2; occurrences: %3")
            .arg(candidate.size()).arg(targets.size()).arg(ids.size()),
        QString("Proposed changes: %1; B-rep rebuild: %2")
            .arg(definitionsChanged?"yes":"none")
            .arg(changed?"changed valid solid placements":"no placement change")};
    for(auto it=targets.cbegin();it!=targets.cend();++it){
        const auto& current=liveArticulatedJoints_[it.key()];
        lines.append(QString("Joint %1 (%2): %3 -> %4")
            .arg(it.key()+1)
            .arg(current.kind==articulated::JointKind::Revolute?"rad":"mm")
            .arg(current.initial,0,'g',14).arg(it.value(),0,'g',14));
    }
    bool unsafeNominalPairs=ids.size()>12; // An unchecked mechanism cannot pass a strict policy.
    if(ids.size()<=12){
        SceneDocument scene;scene.parts_=std::move(staged);
        QVector<AssemblyFitPair> pairs;
        if(!inspectAssemblyFit(scene,ids,requiredClearanceMm,&pairs,error))return false;
        int overlaps=0,contacts=0,insufficient=0;
        double smallest=std::numeric_limits<double>::infinity();
        for(const auto& pair:pairs){
            if(pair.status==fit::Status::Interference){++overlaps;unsafeNominalPairs=true;
                lines.append(QString("OVERLAP %1 / %2: %3 mm^3")
                    .arg(pair.firstName,pair.secondName)
                    .arg(pair.overlapVolumeMm3,0,'g',12));
            }else if(pair.status==fit::Status::Contact){++contacts;
                if(requiredClearanceMm>0){++insufficient;unsafeNominalPairs=true;}
            }else if(pair.status==fit::Status::InsufficientClearance){
                ++insufficient;unsafeNominalPairs=true;smallest=std::min(smallest,pair.distanceMm);
            }else smallest=std::min(smallest,pair.distanceMm);
        }
        lines.append(QString("Exact nominal pairs: %1; overlaps: %2; contacts: %3; inadequate gaps: %4")
             .arg(pairs.size()).arg(overlaps).arg(contacts).arg(insufficient));
        if(std::isfinite(smallest))lines.append(QString("Minimum separated gap: %1 mm")
            .arg(smallest,0,'g',12));
    }else lines.append("Pairwise solid interference NOT CHECKED above 12 occurrences.");
    lines.append("Static pose and clearance only; no continuous collision certification.");
    *report=lines.join('\n');
    if(hasUnsafeNominalPairs)*hasUnsafeNominalPairs=unsafeNominalPairs;
    if(error)error->clear();return true;
}

bool SceneDocument::previewLiveArticulatedTrajectory(const QMap<int,double>& targets,
        int sampleCount,QString* report,QString* error,double requiredClearanceMm,
        bool rejectUnsafeSamples,int refinementsPerInterval,bool easeInOut) const{
    if(!report||!hasLiveArticulatedNetwork()||!fit::validThreshold(requiredClearanceMm)||
       targets.isEmpty()||sampleCount<2||sampleCount>12||
       refinementsPerInterval<1||refinementsPerInterval>4){
        setError(error,"A saved mechanism, driven targets, 2-12 base intervals, 1-4 refinements, valid clearance and a report are required.");
        return false;
    }
    // Validate all targets against the original joint graph before doing any
    // B-rep work. The staged final pose must also be reachable.
    QVector<ArticulatedNetworkJoint> finalDefinitions;
    bool anyChanged=false;
    if(!applyArticulatedTargets(liveArticulatedJoints_,targets,
        &finalDefinitions,&anyChanged,error))return false;
    if(!anyChanged){
        setError(error,"Select at least one changed driven coordinate to preview a trajectory.");
        return false;
    }
    const int totalIntervals=sampleCount*refinementsPerInterval;
    QStringList lines{
        "SAMPLED ARTICULATED MULTI-DRIVE MOTION PREVIEW (READ ONLY)",
        QString("Intervals: %1; sampled poses: %2; driven coordinates: %3")
            .arg(totalIntervals).arg(totalIntervals+1).arg(targets.size()),
        QString("Base intervals: %1; refinement factor: %2")
            .arg(sampleCount).arg(refinementsPerInterval),
        QString("Progression: %1 (not a dynamic simulation)")
            .arg(easeInOut?"ease in/out":"linear"),
        QString("Required nominal clearance: %1 mm").arg(requiredClearanceMm,0,'g',12),
        "Each sample independently uses the complete kinematic and solid-rebuild preflight.",
        "These are discrete static poses, NOT continuous collision certification.",
        "A passing sample does not establish safety between adjacent poses."
    };
    std::vector<joint_motion::Drive> drivePlan;
    drivePlan.reserve(static_cast<std::size_t>(targets.size()));
    for(auto it=targets.cbegin();it!=targets.cend();++it){
        const auto& joint=liveArticulatedJoints_.at(it.key());
        // Legacy saved driven joints pin min/max to the old target. Keep
        // their existing movable behavior; explicit nonzero travel ranges
        // remain mandatory constraints on every interpolated sample.
        const bool explicitTravel=joint.maximum-joint.minimum>1e-10;
        const double minimum=explicitTravel?joint.minimum:std::min(joint.initial,it.value());
        const double maximum=explicitTravel?joint.maximum:std::max(joint.initial,it.value());
        drivePlan.push_back({it.key(),joint.initial,it.value(),minimum,maximum});
    }
    for(int i=0;i<=totalIntervals;++i){
        const double fraction=static_cast<double>(i)/static_cast<double>(totalIntervals);
        std::vector<std::pair<int,double>> frameCoordinates;
        std::string interpolationIssue;
        if(!joint_motion::interpolate(drivePlan,fraction,
             easeInOut?joint_motion::Progression::Smoothstep:joint_motion::Progression::Linear,
             &frameCoordinates,&interpolationIssue)){
            setError(error,QString::fromStdString(interpolationIssue));return false;
        }
        QMap<int,double> frame;
        for(const auto& coordinate:frameCoordinates)
            frame.insert(coordinate.first,coordinate.second);
        QString frameReport,issue;
        bool unsafeFrame=false;
        if(!previewLiveArticulatedCoordinates(frame,&frameReport,&issue,requiredClearanceMm,&unsafeFrame)){
            setError(error,QString("Motion sample %1/%2 (fraction %3) failed: %4")
                .arg(i).arg(totalIntervals).arg(fraction,0,'g',8).arg(issue));
            return false; // The original document and the caller's report stay intact.
        }
        if(rejectUnsafeSamples&&unsafeFrame){
            setError(error,QString("Sampled motion pose %1/%2 has overlap, inadequate clearance, "
                "or exceeds the 12-body fit-audit limit. No change was committed.")
                .arg(i).arg(totalIntervals));
            return false;
        }
        lines<<QString("FRAME %1/%2, fraction %3")
                   .arg(i).arg(totalIntervals).arg(fraction,0,'g',8)
             <<frameReport<<QString();
    }
    *report=lines.join('\n');
    if(error)error->clear();
    return true;
}

bool SceneDocument::buildLiveArticulatedMotionCsv(const QMap<int,double>& targets,
        int baseIntervals,int refinement,bool easeInOut,double requiredClearanceMm,
        bool rejectUnsafe,QByteArray* csv,QString* error) const{
    if(!csv||!hasLiveArticulatedNetwork()||targets.isEmpty()||
       baseIntervals<2||baseIntervals>12||refinement<1||refinement>4||
       !fit::validThreshold(requiredClearanceMm)){
        setError(error,"Choose a saved mechanism, targets, 2-12 base intervals, 1-4 refinements and a valid clearance.");
        return false;
    }
    QVector<ArticulatedNetworkJoint> proposed;bool changes=false;
    if(!applyArticulatedTargets(liveArticulatedJoints_,targets,&proposed,&changes,error))return false;
    if(!changes){setError(error,"At least one drive must change for a motion export.");return false;}
    std::vector<joint_motion::Drive> drives;drives.reserve(static_cast<std::size_t>(targets.size()));
    for(auto it=targets.cbegin();it!=targets.cend();++it){
        const auto& joint=liveArticulatedJoints_.at(it.key());
        const bool explicitTravel=joint.maximum-joint.minimum>1e-10;
        drives.push_back({it.key(),joint.initial,it.value(),
            explicitTravel?joint.minimum:std::min(joint.initial,it.value()),
            explicitTravel?joint.maximum:std::max(joint.initial,it.value())});
    }
    QByteArray generated("sample,fraction,progression,unsafe_nominal_fit,required_clearance_mm");
    for(auto it=targets.cbegin();it!=targets.cend();++it)
        generated+=QString(",joint_%1_%2").arg(it.key()+1)
            .arg(liveArticulatedJoints_.at(it.key()).kind==articulated::JointKind::Revolute?
                "radians":"mm").toUtf8();
    generated+=",pose_review\n";
    const int intervals=baseIntervals*refinement;
    for(int i=0;i<=intervals;++i){
        const double fraction=static_cast<double>(i)/intervals;
        std::vector<std::pair<int,double>> coordinates;
        std::string interpolationIssue;
        if(!joint_motion::interpolate(drives,fraction,
           easeInOut?joint_motion::Progression::Smoothstep:joint_motion::Progression::Linear,
           &coordinates,&interpolationIssue)){
            setError(error,QString::fromStdString(interpolationIssue));return false;
        }
        QMap<int,double> frame;
        for(const auto& coordinate:coordinates)frame.insert(coordinate.first,coordinate.second);
        QString pose,issue;bool unsafe=false;
        if(!previewLiveArticulatedCoordinates(frame,&pose,&issue,requiredClearanceMm,&unsafe)){
            setError(error,QString("Motion CSV sample %1/%2 failed: %3")
                .arg(i).arg(intervals).arg(issue));return false;
        }
        if(rejectUnsafe&&unsafe){
            setError(error,QString("Motion CSV sample %1/%2 violates the requested nominal fit policy.")
                .arg(i).arg(intervals));return false;
        }
        generated+=QString("%1,%2,%3,%4,%5")
            .arg(i).arg(fraction,0,'g',16).arg(easeInOut?"smoothstep":"linear")
            .arg(unsafe?"YES":"NO").arg(requiredClearanceMm,0,'g',16).toUtf8();
        for(const auto& coordinate:coordinates)
            generated+=QString(",%1").arg(coordinate.second,0,'g',16).toUtf8();
        // Encode full pose diagnostics as one escaped CSV cell, not additional
        // records. JSON and scene state are never modified by this read-only run.
        pose.replace('"',"\"\"");pose.replace('\n'," \\n ");
        generated+=",\""+pose.toUtf8()+"\"\n";
    }
    *csv=std::move(generated);
    if(error)error->clear();return true;
}

bool SceneDocument::buildLiveArticulatedEnvelopeCsv(const QMap<int,double>& targets,
        int baseIntervals,int refinement,bool easeInOut,QByteArray* csv,QString* error,
        bool* translationSweepSupported,double* closestSweptBoxGapMm) const{
    if(!csv||!hasLiveArticulatedNetwork()||targets.isEmpty()||
       baseIntervals<2||baseIntervals>12||refinement<1||refinement>4){
        setError(error,"Saved articulated network, targets and 2-12 intervals / 1-4 refinements required.");
        return false;
    }
    QVector<ArticulatedNetworkJoint> finalDefinitions;bool changed=false;
    if(!applyArticulatedTargets(liveArticulatedJoints_,targets,&finalDefinitions,&changed,error))return false;
    if(!changed){setError(error,"The envelope requires at least one changed drive.");return false;}
    QStringList moving;
    for(const auto& joint:liveArticulatedJoints_)
        if(!moving.contains(joint.childBodyId))moving.append(joint.childBodyId);
    // Include the fixed reference in pair checks: otherwise a moving slider
    // can approach the fixture without appearing in a pairwise result.
    if(moving.size()>19){setError(error,"Workspace report supports 19 moving bodies plus the fixed reference.");return false;}
    QStringList tracked=moving;
    if(!tracked.contains(liveArticulatedFixed_))tracked.append(liveArticulatedFixed_);
    // An endpoint AABB cannot be swept linearly for a rotating mechanism.
    // Any revolute joint, even if not driven in this request, conservatively
    // disables the continuous translation evidence.
    const bool translationOnly=std::all_of(liveArticulatedJoints_.cbegin(),liveArticulatedJoints_.cend(),
        [](const ArticulatedNetworkJoint& joint){return joint.kind!=articulated::JointKind::Revolute;});
    std::vector<joint_motion::Drive> drives;
    for(auto it=targets.cbegin();it!=targets.cend();++it){
        const auto& joint=liveArticulatedJoints_.at(it.key());
        const bool hasTravel=joint.maximum-joint.minimum>1e-10;
        drives.push_back({it.key(),joint.initial,it.value(),
            hasTravel?joint.minimum:std::min(joint.initial,it.value()),
            hasTravel?joint.maximum:std::max(joint.initial,it.value())});
    }
    QMap<QString,Bnd_Box> envelopes;
    proximity::OrderInvariantTracker sameTime;
    proximity::OrderInvariantSweptTracker betweenPoses(0.0);
    bool sweepAvailable=translationOnly;
    const int intervals=baseIntervals*refinement;
    for(int sample=0;sample<=intervals;++sample){
        const double fraction=static_cast<double>(sample)/intervals;
        std::vector<std::pair<int,double>> coordinates;
        std::string interpolationError;
        if(!joint_motion::interpolate(drives,fraction,
                easeInOut?joint_motion::Progression::Smoothstep:joint_motion::Progression::Linear,
                &coordinates,&interpolationError)){
            setError(error,QString::fromStdString(interpolationError));return false;
        }
        QMap<int,double> requested;
        for(const auto& coordinate:coordinates)requested.insert(coordinate.first,coordinate.second);
        QVector<ArticulatedNetworkJoint> frameJoints;bool frameChanged=false;
        if(!applyArticulatedTargets(liveArticulatedJoints_,requested,&frameJoints,&frameChanged,error))return false;
        QVector<Part> frameParts;bool geometryChanged=false;
        if(!stageLiveArticulatedRegeneration(frameJoints,&frameParts,&geometryChanged,error)){
            if(error)*error=QString("Envelope sample %1/%2: %3").arg(sample).arg(intervals).arg(*error);
            return false;
        }
        std::vector<proximity::BodyBox> simultaneous;
        simultaneous.reserve(static_cast<std::size_t>(tracked.size()));
        for(const QString& id:tracked){
            const auto found=std::find_if(frameParts.cbegin(),frameParts.cend(),
                [&](const Part& body){return body.id==id;});
            if(found==frameParts.cend()||found->shape.IsNull()||
               !validShape(found->shape,error)){
                setError(error,"A sampled moving body has missing or invalid geometry.");return false;
            }
            try{
                Bnd_Box current;
                BRepBndLib::Add(found->shape,current);
                if(current.IsVoid()){
                    setError(error,"Sampled moving-body geometry has no bounds.");return false;
                }
                proximity::Bounds b{};
                current.Get(b.x0,b.y0,b.z0,b.x1,b.y1,b.z1);
                simultaneous.push_back({id.toStdString(),b});
                BRepBndLib::Add(found->shape,envelopes[id]);
            }catch(const Standard_Failure& exception){
                setError(error,QString("Sampled body bounding geometry failed: %1").arg(exception.what()));
                return false;
            }
        }
        std::string proximityIssue;
        if(!sameTime.observe(simultaneous,&proximityIssue)){
            setError(error,QString("Simultaneous sampled-body bounds failed at pose %1: %2")
                .arg(sample).arg(QString::fromStdString(proximityIssue)));return false;
        }
        if(sweepAvailable){
            // A shape with changing world-axis extents (or invalid numeric
            // bounds) cannot truthfully use the translational AABB solver.
            // Keep sampled evidence, but mark the sweep unsupported.
            std::string sweepIssue;
            if(!betweenPoses.observe(fraction,simultaneous,&sweepIssue))
                sweepAvailable=false;
        }
    }
    // Compare unions of sampled body-space AABBs. A zero gap signals
    // potential shared workspace, NOT a same-time solid interference.
    struct Bounds{double x0,y0,z0,x1,y1,z1;};
    QMap<QString,Bounds> boxes;
    for(const QString& id:tracked){
        if(!envelopes.contains(id)||envelopes[id].IsVoid()){
            setError(error,"A moving body has no sampled workspace envelope.");return false;
        }
        Bounds b{};envelopes[id].Get(b.x0,b.y0,b.z0,b.x1,b.y1,b.z1);
        boxes.insert(id,b);
    }
    const auto aabbGap=[](const Bounds& a,const Bounds& b){
        const double dx=std::max({0.0,b.x0-a.x1,a.x0-b.x1});
        const double dy=std::max({0.0,b.y0-a.y1,a.y0-b.y1});
        const double dz=std::max({0.0,b.z0-a.z1,a.z0-b.z1});
        return std::hypot(dx,dy,dz);
    };
    QByteArray generated("body_id,body_name,samples,x_min_mm,y_min_mm,z_min_mm,x_max_mm,y_max_mm,z_max_mm,box_volume_mm3,possible_workspace_overlap_count,nearest_other_body_id,nearest_workspace_box_gap_mm,same_time_aabb_overlap_samples,closest_same_time_aabb_gap_mm,closest_same_time_sample,continuous_translation_sweep_status,swept_aabb_contact_segments,swept_aabb_minimum_gap_mm,swept_aabb_closest_segment,coverage_note\n");
    for(const QString& id:moving){
        const Part* part=find(id);
        if(!part||!boxes.contains(id)){
            setError(error,"A sampled moving body has an empty bounding envelope.");return false;
        }
        const Bounds b=boxes.value(id);
        const double volume=(b.x1-b.x0)*(b.y1-b.y0)*(b.z1-b.z0);
        if(!std::isfinite(volume)||volume<0){
            setError(error,"A sampled moving-body envelope is nonfinite.");return false;
        }
        int possibleOverlaps=0;QString nearestId;
        double closest=std::numeric_limits<double>::infinity();
        for(const QString& other:tracked){
            if(other==id)continue;
            const double gap=aabbGap(b,boxes.value(other));
            if(!std::isfinite(gap)){
                setError(error,"Nonfinite moving-body workspace separation.");return false;
            }
            if(gap<=1e-9)++possibleOverlaps;
            if(gap<closest){closest=gap;nearestId=other;}
        }
        // Pairwise broad-phase distances use the SAME sampled pose.
        std::size_t sameTimeOverlaps=0,closestSample=0;
        double nearestSimultaneous=std::numeric_limits<double>::infinity();
        for(const auto& pair:sameTime.pairs()){
            if(pair.firstId!=id.toStdString()&&pair.secondId!=id.toStdString())continue;
            sameTimeOverlaps+=pair.simultaneousOverlapSamples;
            if(pair.minimumAabbGapMm<nearestSimultaneous){
                nearestSimultaneous=pair.minimumAabbGapMm;
                closestSample=pair.closestSample;
            }
        }
        std::size_t sweptContacts=0,sweptSegment=0;
        double sweptGap=std::numeric_limits<double>::infinity();
        if(sweepAvailable){
            for(const auto& pair:betweenPoses.pairs()){
                if(pair.firstId!=id.toStdString()&&pair.secondId!=id.toStdString())continue;
                sweptContacts+=pair.contactSegments;
                if(pair.minimumGapMm<sweptGap){
                    sweptGap=pair.minimumGapMm;
                    sweptSegment=pair.closestSegment;
                }
            }
        }
        auto quote=[](QString value){value.replace('"',QStringLiteral("\"\""));return QStringLiteral("\"")+value+QStringLiteral("\"");};
        generated+=QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21\n")
            .arg(quote(id),quote(part->name)).arg(intervals+1)
            .arg(b.x0,0,'g',16).arg(b.y0,0,'g',16).arg(b.z0,0,'g',16)
            .arg(b.x1,0,'g',16).arg(b.y1,0,'g',16).arg(b.z1,0,'g',16)
            .arg(volume,0,'g',16).arg(possibleOverlaps)
            .arg(quote(nearestId))
            .arg(std::isfinite(closest)?QString::number(closest,'g',16):QStringLiteral("N/A"))
            .arg(static_cast<qulonglong>(sameTimeOverlaps))
            .arg(std::isfinite(nearestSimultaneous)?QString::number(nearestSimultaneous,'g',16):QStringLiteral("N/A"))
            .arg(std::isfinite(nearestSimultaneous)?QString::number(static_cast<qulonglong>(closestSample)):QStringLiteral("N/A"))
            .arg(sweepAvailable?"SUPPORTED_TRANSLATION":translationOnly?"UNAVAILABLE_NUMERICS":"UNSUPPORTED_ROTATION")
            .arg(sweepAvailable?QString::number(static_cast<qulonglong>(sweptContacts)):QStringLiteral("N/A"))
            .arg(sweepAvailable&&std::isfinite(sweptGap)?QString::number(sweptGap,'g',16):QStringLiteral("N/A"))
            .arg(sweepAvailable&&std::isfinite(sweptGap)?QString::number(static_cast<qulonglong>(sweptSegment)):QStringLiteral("N/A"))
            .arg(quote(sweepAvailable?
                "sampled pose AABB and translational inter-pose AABB are broad-phase only; neither box overlap proves solid collision":
                "sampled pose AABB is broad-phase only; neither box overlap proves solid collision; inter-pose sweep unavailable")) .toUtf8();
    }
    // A strict caller uses the aggregated minimum to reject potential
    // collisions between recorded samples. No outputs change on failure.
    double globallyClosest=std::numeric_limits<double>::infinity();
    if(sweepAvailable)for(const auto& pair:betweenPoses.pairs())
        globallyClosest=std::min(globallyClosest,pair.minimumGapMm);
    *csv=std::move(generated);
    if(translationSweepSupported)*translationSweepSupported=sweepAvailable;
    if(closestSweptBoxGapMm)*closestSweptBoxGapMm=globallyClosest;
    if(error)error->clear();return true;
}

bool SceneDocument::updateLiveArticulatedCoordinates(const QMap<int,double>& targets,
        QString* error){
    if(!hasLiveArticulatedNetwork()){
        setError(error,"There is no saved articulated network.");return false;
    }
    QVector<ArticulatedNetworkJoint> proposed;bool definitionsChanged=false;
    if(!applyArticulatedTargets(liveArticulatedJoints_,targets,&proposed,
                                &definitionsChanged,error))return false;
    if(!definitionsChanged){if(error)error->clear();return true;}
    QVector<Part> staged;bool geometryChanged=false;
    if(!stageLiveArticulatedRegeneration(proposed,&staged,&geometryChanged,error))return false;
    checkpoint();
    if(geometryChanged)parts_=std::move(staged);
    liveArticulatedJoints_=std::move(proposed);
    if(error)error->clear();return true;
}

bool SceneDocument::commitLiveArticulatedCoordinatesChecked(
        const QMap<int,double>& targets,double requiredClearanceMm,
        bool rejectUnsafe,int baseIntervals,int refinement,bool smoothstep,
        QString* error){
    if(!fit::validThreshold(requiredClearanceMm)||baseIntervals<0||baseIntervals>12||
       refinement<1||refinement>4||!hasLiveArticulatedNetwork()){
        setError(error,"Saved mechanism, nonnegative clearance, 0-12 base intervals and 1-4 refinements are required.");
        return false;
    }
    QString preview,issue;
    bool unsafe=false;
    // Re-run the policy at the commit boundary. A previous GUI preview is
    // not evidence that a later direct API call is safe.
    if(!previewLiveArticulatedCoordinates(targets,&preview,&issue,
            requiredClearanceMm,&unsafe)){
        setError(error,issue);return false;
    }
    if(rejectUnsafe&&unsafe){
        setError(error,"The committed pose violates the requested clearance policy or exceeds the fit-audit limit.");
        return false;
    }
    bool changed=false;
    for(auto it=targets.cbegin();it!=targets.cend();++it){
        if(std::abs(liveArticulatedJoints_.at(it.key()).initial-it.value())>1e-12){changed=true;break;}
    }
    // A true no-op consumes no undo checkpoint and requires no trajectory.
    if(!changed){if(error)error->clear();return true;}
    if(baseIntervals>0){
        QString trajectory;
        if(!previewLiveArticulatedTrajectory(targets,baseIntervals,&trajectory,&issue,
                requiredClearanceMm,rejectUnsafe,refinement,smoothstep)){
            setError(error,issue);return false;
        }
        if(rejectUnsafe&&std::all_of(liveArticulatedJoints_.cbegin(),liveArticulatedJoints_.cend(),
                [](const ArticulatedNetworkJoint& joint){return joint.kind!=articulated::JointKind::Revolute;})){
            // The old policy examined discrete poses and could accept a slider
            // that passes THROUGH a fixture between two safe keyframes. For
            // translation-only mechanisms, enforce the additional continuous
            // broad-phase check at the atomic commit boundary. Rotation still
            // requires an independent rotational/narrow-phase solution.
            QByteArray evidence;
            bool supported=false;
            double sweptMinimum=std::numeric_limits<double>::infinity();
            if(!buildLiveArticulatedEnvelopeCsv(targets,baseIntervals,refinement,smoothstep,
                    &evidence,&issue,&supported,&sweptMinimum)){
                setError(error,issue);return false;
            }
            if(!supported||!std::isfinite(sweptMinimum)||sweptMinimum<=requiredClearanceMm){
                setError(error,"Potential between-pose fixture contact, inadequate swept AABB clearance, "
                    "or unsupported translation data. Strict move not committed.");
                return false;
            }
        }
    }
    return updateLiveArticulatedCoordinates(targets,error);
}

namespace {
// An edited joint stays the same saved graph edge. Reparenting/retyping a
// connection is a separate topology-changing operation, not a harmless edit.
bool sameArticulatedConnection(const SceneDocument::ArticulatedNetworkJoint& a,
                               const SceneDocument::ArticulatedNetworkJoint& b){
    return a.parentBodyId==b.parentBodyId&&a.childBodyId==b.childBodyId&&
           a.kind==b.kind;
}
bool sameArticulatedDefinition(const SceneDocument::ArticulatedNetworkJoint& a,
                               const SceneDocument::ArticulatedNetworkJoint& b){
    if(!sameArticulatedConnection(a,b)||a.driven!=b.driven||
       a.initial!=b.initial||a.minimum!=b.minimum||a.maximum!=b.maximum||
       a.axis.x!=b.axis.x||a.axis.y!=b.axis.y||a.axis.z!=b.axis.z||
       a.childAtZero.translation.x!=b.childAtZero.translation.x||
       a.childAtZero.translation.y!=b.childAtZero.translation.y||
       a.childAtZero.translation.z!=b.childAtZero.translation.z)return false;
    for(int i=0;i<9;++i)if(a.childAtZero.rotation[i]!=b.childAtZero.rotation[i])return false;
    return true;
}
}

bool SceneDocument::previewLiveArticulatedJointDefinitionEdit(int index,
        const ArticulatedNetworkJoint& proposed,QString* report,QString* error,
        double requiredClearanceMm) const{
    if(!report||!fit::validThreshold(requiredClearanceMm)||
       !hasLiveArticulatedNetwork()||index<0||index>=liveArticulatedJoints_.size()){
        setError(error,"A saved joint, output report and nonnegative clearance are required.");return false;
    }
    const auto& existing=liveArticulatedJoints_[index];
    if(!sameArticulatedConnection(existing,proposed)){
        setError(error,"Joint definition edit cannot change its endpoints or joint kind.");return false;
    }
    QVector<ArticulatedNetworkJoint> definitions=liveArticulatedJoints_;
    definitions[index]=proposed;
    QVector<Part> staged;bool moved=false;
    if(!stageLiveArticulatedRegeneration(definitions,&staged,&moved,error))return false;
    QStringList ids{liveArticulatedFixed_};
    for(const auto& joint:definitions){
        if(!ids.contains(joint.parentBodyId))ids.append(joint.parentBodyId);
        if(!ids.contains(joint.childBodyId))ids.append(joint.childBodyId);
    }
    QStringList lines{"READ-ONLY ARTICULATED JOINT-DEFINITION PREVIEW",
        QString("Joint %1: %2 -> %3; unchanged graph connection")
            .arg(index+1).arg(existing.parentBodyId,existing.childBodyId),
        QString("Definition changed: %1; geometry changed: %2")
            .arg(sameArticulatedDefinition(existing,proposed)?"no":"yes")
            .arg(moved?"yes":"no"),
        QString("Coordinate: %1; limits: [%2, %3]")
            .arg(proposed.initial,0,'g',15).arg(proposed.minimum,0,'g',15)
            .arg(proposed.maximum,0,'g',15)};
    if(ids.size()<=12){
        SceneDocument detached;detached.parts_=std::move(staged);
        QVector<AssemblyFitPair> pairs;
        if(!inspectAssemblyFit(detached,ids,requiredClearanceMm,&pairs,error))return false;
        int overlaps=0,contacts=0,insufficient=0;
        for(const auto& pair:pairs){
            if(pair.status==fit::Status::Interference)++overlaps;
            else if(pair.status==fit::Status::Contact)++contacts;
            else if(pair.status==fit::Status::InsufficientClearance)++insufficient;
        }
        lines<<QString("Static nominal fit: %1 pairs, %2 interferences, %3 contacts, %4 inadequate clearances")
            .arg(pairs.size()).arg(overlaps).arg(contacts).arg(insufficient);
    }else lines<<"Static nominal fit skipped above 12 component occurrences.";
    lines<<"The preview does not certify clearance during continuous motion.";
    *report=lines.join('\n');if(error)error->clear();return true;
}

bool SceneDocument::replaceLiveArticulatedJointDefinition(int index,
        const ArticulatedNetworkJoint& proposed,QString* error){
    if(!hasLiveArticulatedNetwork()||index<0||index>=liveArticulatedJoints_.size()){
        setError(error,"No valid saved articulated joint was selected.");return false;
    }
    const auto& existing=liveArticulatedJoints_[index];
    if(!sameArticulatedConnection(existing,proposed)){
        setError(error,"A saved joint's endpoints and kind cannot be changed in place.");return false;
    }
    if(sameArticulatedDefinition(existing,proposed)){
        if(error)error->clear();return true; // no checkpoint for no-op
    }
    QVector<ArticulatedNetworkJoint> definitions=liveArticulatedJoints_;
    definitions[index]=proposed;
    QVector<Part> staged;bool changed=false;
    if(!stageLiveArticulatedRegeneration(definitions,&staged,&changed,error))return false;
    checkpoint();
    if(changed)parts_=std::move(staged);
    liveArticulatedJoints_=std::move(definitions);
    if(error)error->clear();return true;
}

// Coordinate multi-joint authoring: validate the whole proposed definition
// set before changing ANY live occurrence. All revisions and the caller's
// report remain intact if a single connection or solid cannot be solved.
bool SceneDocument::stageArticulatedDefinitionBatch(
        const QMap<int,ArticulatedNetworkJoint>& edits,
        QVector<ArticulatedNetworkJoint>* definitions,
        QVector<Part>* staged,bool* geometryChanged,bool* definitionsChanged,
        QString* report,QString* error,double clearanceMm,
        bool rejectUnsafeNominal) const{
    if(!hasLiveArticulatedNetwork()||edits.isEmpty()||
       edits.size()>liveArticulatedJoints_.size()||!definitions||!staged||
       !geometryChanged||!definitionsChanged||!fit::validThreshold(clearanceMm)){
        setError(error,"A saved articulated graph, 1-N joint edits, staging buffers and a valid clearance are required.");return false;
    }
    QVector<ArticulatedNetworkJoint> proposed=liveArticulatedJoints_;
    bool changed=false;
    for(auto it=edits.cbegin();it!=edits.cend();++it){
        if(it.key()<0||it.key()>=proposed.size()||
           !sameArticulatedConnection(proposed.at(it.key()),it.value())){
            setError(error,"A batch edit must target a saved joint index without changing its endpoints or type.");return false;
        }
        changed=changed||!sameArticulatedDefinition(proposed.at(it.key()),it.value());
        proposed[it.key()]=it.value();
    }
    QVector<Part> candidate;bool moved=false;
    if(changed&&!stageLiveArticulatedRegeneration(proposed,&candidate,&moved,error))return false;
    if(!changed)candidate=parts_;
    QStringList ids{liveArticulatedFixed_};
    for(const auto& joint:proposed){
        if(!ids.contains(joint.parentBodyId))ids<<joint.parentBodyId;
        if(!ids.contains(joint.childBodyId))ids<<joint.childBodyId;
    }
    QStringList lines{QStringLiteral("ATOMIC ARTICULATED MATE-DEFINITION BATCH PREVIEW"),
        QStringLiteral("edited_joints=%1; changed_definitions=%2; staged_geometry_changed=%3")
            .arg(edits.size()).arg(changed?1:0).arg(moved?1:0),
        QStringLiteral("connected_occurrences=%1; strict_nominal=%2")
            .arg(ids.size()).arg(rejectUnsafeNominal?1:0)};
    if(ids.size()<=12){
        SceneDocument detached;detached.parts_=candidate;
        QVector<AssemblyFitPair> pairs;
        if(!inspectAssemblyFit(detached,ids,clearanceMm,&pairs,error))return false;
        int overlaps=0,contacts=0,insufficient=0;
        for(const auto& pair:pairs){
            if(pair.status==fit::Status::Interference)++overlaps;
            if(pair.status==fit::Status::Contact)++contacts;
            if(pair.status==fit::Status::InsufficientClearance)++insufficient;
        }
        lines<<QStringLiteral("exact_nominal_pairs=%1; overlaps=%2; contacts=%3; inadequate_clearances=%4")
            .arg(pairs.size()).arg(overlaps).arg(contacts).arg(insufficient);
        if(rejectUnsafeNominal&&(overlaps>0||insufficient>0||
           (clearanceMm>0.0&&contacts>0))){
            setError(error,"Batch mate edit failed the exact nominal clearance policy; no change committed.");return false;
        }
    }else{
        lines<<QStringLiteral("exact_nominal_fit=NOT_CHECKED (>12 occurrences)");
        if(rejectUnsafeNominal){
            setError(error,"Strict mate edit requires inspecting at most twelve occurrences.");return false;
        }
    }
    lines<<QStringLiteral("motion_intervals=NOT_CERTIFIED; manufacturing_approval=NOT_ISSUED");
    *definitions=std::move(proposed);
    *staged=std::move(candidate);
    *geometryChanged=moved;
    *definitionsChanged=changed;
    if(report)*report=lines.join('\n');
    if(error)error->clear();
    return true;
}

bool SceneDocument::previewLiveArticulatedJointDefinitionBatch(
        const QMap<int,ArticulatedNetworkJoint>& edits,QString* report,
        QString* error,double clearanceMm,bool rejectUnsafeNominal) const{
    if(!report){setError(error,"Provide a read-only batch preview report.");return false;}
    QVector<ArticulatedNetworkJoint> definitions;
    QVector<Part> staged;bool geometry=false,changed=false;
    QString next;
    if(!stageArticulatedDefinitionBatch(edits,&definitions,&staged,&geometry,&changed,
                &next,error,clearanceMm,rejectUnsafeNominal))return false;
    *report=std::move(next);
    return true;
}

bool SceneDocument::replaceLiveArticulatedJointDefinitionBatch(
        const QMap<int,ArticulatedNetworkJoint>& edits,QString* error,
        double clearanceMm,bool rejectUnsafeNominal){
    QVector<ArticulatedNetworkJoint> definitions;
    QVector<Part> staged;bool geometry=false,changed=false;
    if(!stageArticulatedDefinitionBatch(edits,&definitions,&staged,&geometry,&changed,
                nullptr,error,clearanceMm,rejectUnsafeNominal))return false;
    if(!changed){if(error)error->clear();return true;}
    checkpoint();
    if(geometry)parts_=std::move(staged);
    liveArticulatedJoints_=std::move(definitions);
    if(error)error->clear();
    return true;
}

namespace {
// Saved network member identities are a contract: a reference edit may
// change edges but must not quietly introduce/remove component occurrences.
bool validArticulatedRebind(const QString& fixed,
        const QMap<QString,mates::Transform>& reference,
        const QVector<SceneDocument::ArticulatedNetworkJoint>& joints,
        int index,const QString& parent,const QString& child,QString* error){
    if(fixed.isEmpty()||index<0||index>=joints.size()||
       parent.isEmpty()||child.isEmpty()||parent==child||
       !reference.contains(parent)||!reference.contains(child)){
        if(error)*error="Choose two distinct EXISTING articulated-network occurrences.";
        return false;
    }
    return true;
}
}

bool SceneDocument::previewLiveArticulatedJointRebind(int index,
        const QString& newParentBodyId,const QString& newChildBodyId,
        QString* report,QString* error,double requiredClearanceMm) const{
    if(!report||!fit::validThreshold(requiredClearanceMm)){
        setError(error,"A preview report and nonnegative clearance are required.");
        return false;
    }
    if(!validArticulatedRebind(liveArticulatedFixed_,liveArticulatedReferencePoses_,
           liveArticulatedJoints_,index,newParentBodyId,newChildBodyId,error))return false;
    QVector<ArticulatedNetworkJoint> proposed=liveArticulatedJoints_;
    const ArticulatedNetworkJoint original=proposed[index];
    proposed[index].parentBodyId=newParentBodyId;
    proposed[index].childBodyId=newChildBodyId;
    QVector<Part> staged;bool changed=false;
    if(!stageLiveArticulatedRegeneration(proposed,&staged,&changed,error))return false;
    QStringList participating{liveArticulatedFixed_};
    for(const auto& joint:proposed){
        if(!participating.contains(joint.parentBodyId))participating<<joint.parentBodyId;
        if(!participating.contains(joint.childBodyId))participating<<joint.childBodyId;
    }
    QStringList result{
        "READ-ONLY ARTICULATED JOINT REBIND PREVIEW",
        QString("Joint index: %1; type and drive parameters retained").arg(index+1),
        QString("Old connection: %1 -> %2").arg(original.parentBodyId,original.childBodyId),
        QString("New connection: %1 -> %2").arg(newParentBodyId,newChildBodyId),
        QString("Changed: %1; staged geometry changed: %2")
            .arg(original.parentBodyId!=newParentBodyId||original.childBodyId!=newChildBodyId?"yes":"no")
            .arg(changed?"yes":"no")};
    if(participating.size()<=12){
        SceneDocument detached;
        detached.parts_=std::move(staged);
        QVector<AssemblyFitPair> pairs;
        if(!inspectAssemblyFit(detached,participating,requiredClearanceMm,&pairs,error))return false;
        int overlaps=0,contacts=0,insufficient=0;
        for(const auto& pair:pairs){
            if(pair.status==fit::Status::Interference)++overlaps;
            else if(pair.status==fit::Status::Contact)++contacts;
            else if(pair.status==fit::Status::InsufficientClearance)++insufficient;
        }
        result<<QString("Static nominal fit: %1 pairs, %2 overlaps, %3 contacts, %4 inadequate clearances")
            .arg(pairs.size()).arg(overlaps).arg(contacts).arg(insufficient);
    }else result<<"Static fit skipped above 12 component occurrences.";
    result<<"No model, saved joint, revision, or undo state was modified."
          <<"A static fit is not a continuous motion collision guarantee.";
    *report=result.join('\n');
    if(error)error->clear();
    return true;
}

bool SceneDocument::rebindLiveArticulatedJoint(int index,
        const QString& newParentBodyId,const QString& newChildBodyId,
        QString* error){
    if(!validArticulatedRebind(liveArticulatedFixed_,liveArticulatedReferencePoses_,
            liveArticulatedJoints_,index,newParentBodyId,newChildBodyId,error))return false;
    const auto& original=liveArticulatedJoints_[index];
    if(original.parentBodyId==newParentBodyId&&original.childBodyId==newChildBodyId){
        if(error)error->clear();return true; // no-op does not checkpoint
    }
    QVector<ArticulatedNetworkJoint> proposed=liveArticulatedJoints_;
    proposed[index].parentBodyId=newParentBodyId;
    proposed[index].childBodyId=newChildBodyId;
    QVector<Part> staged;bool geometryChanged=false;
    if(!stageLiveArticulatedRegeneration(proposed,&staged,&geometryChanged,error))return false;
    checkpoint();
    if(geometryChanged)parts_=std::move(staged);
    liveArticulatedJoints_=std::move(proposed);
    if(error)error->clear();
    return true;
}

bool SceneDocument::removeLiveArticulatedNetwork(){
    if(!hasLiveArticulatedNetwork())return false;
    checkpoint();liveArticulatedFixed_.clear();liveArticulatedJoints_.clear();
    liveArticulatedReferencePoses_.clear();return true;
}

bool SceneDocument::setJointMotionKeyframes(const QString& childBodyId,const QVector<JointMotionKeyframe>& keyframes,QString* error) {
    Part* child=nullptr;for(Part& part:parts_)if(part.id==childBodyId){child=&part;break;}
    if(!child||(child->assemblyJointType!="Revolute"&&child->assemblyJointType!="Prismatic")){
        setError(error,"Motion keyframes require a revolute or prismatic child occurrence.");return false;
    }
    if(keyframes.size()<2||keyframes.size()>200){setError(error,"A joint motion track must contain 2 to 200 keyframes.");return false;}
    double previousTime=-1.0;
    for(const JointMotionKeyframe& keyframe:keyframes){
        if(!std::isfinite(keyframe.timeSeconds)||keyframe.timeSeconds<0.0||keyframe.timeSeconds>3600.0||
           keyframe.timeSeconds<=previousTime||!std::isfinite(keyframe.value)){
            setError(error,"Motion keyframe times must increase strictly from 0 to 3,600 seconds, with finite values.");return false;
        }
        const bool withinLimits=child->assemblyJointType=="Revolute"?
            keyframe.value>=child->jointMinimumDegrees&&keyframe.value<=child->jointMaximumDegrees:
            keyframe.value>=child->jointMinimumMm&&keyframe.value<=child->jointMaximumMm;
        if(!withinLimits){setError(error,"A motion keyframe exceeds the saved joint limits.");return false;}
        previousTime=keyframe.timeSeconds;
    }
    bool same=child->jointMotionKeyframes.size()==keyframes.size();
    if(same)for(int i=0;i<keyframes.size();++i)
        if(child->jointMotionKeyframes[i].timeSeconds!=keyframes[i].timeSeconds||child->jointMotionKeyframes[i].value!=keyframes[i].value){same=false;break;}
    if(same)return true;
    checkpoint();child->jointMotionKeyframes=keyframes;return true;
}

// Preview and commit share this one validation path. Validates persisted
// source joint identities and each complete rate/limit schedule before any
// assignment, including keys that another editor may have changed meanwhile.
bool SceneDocument::validateJointMotionKeyframesBatch(
        const QMap<QString,QVector<JointMotionKeyframe>>& tracks,
        bool* changed,QString* report,QString* error) const {
    if(!changed||tracks.isEmpty()||tracks.size()>32){
        setError(error,"Batch motion editing needs 1-32 saved joint tracks.");return false;
    }
    QStringList notes{QStringLiteral("READ-ONLY MULTI-JOINT MOTION TIMELINE PREVIEW"),
        QStringLiteral("motion_collision_proof=NOT_CERTIFIED; release=NOT_ISSUED")};
    bool anyChange=false;
    qsizetype changedCount=0;
    for(auto it=tracks.cbegin();it!=tracks.cend();++it){
        const Part* child=find(it.key());
        const bool revolute=child&&child->assemblyJointType==QStringLiteral("Revolute");
        const bool prismatic=child&&child->assemblyJointType==QStringLiteral("Prismatic");
        if(!child||(!revolute&&!prismatic)){
            setError(error,QStringLiteral("Batch motion track '%1' must refer to an existing revolute/prismatic occurrence.").arg(it.key()));
            return false;
        }
        const double min=revolute?child->jointMinimumDegrees:child->jointMinimumMm;
        const double max=revolute?child->jointMaximumDegrees:child->jointMaximumMm;
        std::vector<cad::motion::RateKeyframe> frames;
        frames.reserve(it.value().size());
        for(const auto& key:it.value())frames.push_back({key.timeSeconds,key.value});
        cad::motion::RateAudit audit;
        std::string why;
        if(!cad::motion::auditKeyframeRates(frames,min,max,&audit,&why)){
            setError(error,QStringLiteral("Invalid track for '%1': %2").arg(it.key(),QString::fromStdString(why)));
            return false;
        }
        const auto& prior=child->jointMotionKeyframes;
        bool same=prior.size()==it.value().size();
        if(same)for(qsizetype i=0;i<prior.size();++i){
            if(prior[i].timeSeconds!=it.value()[i].timeSeconds||prior[i].value!=it.value()[i].value){
                same=false;break;
            }
        }
        if(!same)++changedCount;
        anyChange=anyChange||!same;
        notes<<QStringLiteral("body=%1; kind=%2; frames=%3; duration_s=%4; peak_rate=%5 %6/s; velocity_steps=%7; edited=%8")
            .arg(it.key(),child->assemblyJointType)
            .arg(it.value().size())
            .arg(audit.durationSeconds,0,'g',15)
            .arg(audit.maximumAbsoluteRatePerSecond,0,'g',15)
            .arg(revolute?QStringLiteral("deg"):QStringLiteral("mm"))
            .arg(static_cast<qulonglong>(audit.velocityDiscontinuities))
            .arg(same?0:1);
    }
    notes<<QStringLiteral("changed_tracks=%1; one_undo_checkpoint=%2")
        .arg(changedCount).arg(anyChange?1:0);
    *changed=anyChange;
    if(report)*report=notes.join('\n');
    if(error)error->clear();return true;
}

bool SceneDocument::previewJointMotionKeyframesBatch(
        const QMap<QString,QVector<JointMotionKeyframe>>& tracks,
        QString* report,QString* error) const {
    if(!report){setError(error,"Provide an output report for the read-only motion preview.");return false;}
    bool changed=false;
    QString next;
    if(!validateJointMotionKeyframesBatch(tracks,&changed,&next,error))return false;
    *report=std::move(next);
    return true;
}

bool SceneDocument::setJointMotionKeyframesBatch(
        const QMap<QString,QVector<JointMotionKeyframe>>& tracks,
        QString* error){
    bool changed=false;
    if(!validateJointMotionKeyframesBatch(tracks,&changed,nullptr,error))return false;
    if(!changed){if(error)error->clear();return true;}
    QVector<Part> candidate=parts_;
    for(Part& part:candidate){
        const auto it=tracks.constFind(part.id);
        if(it!=tracks.cend())part.jointMotionKeyframes=it.value();
    }
    checkpoint();
    parts_=std::move(candidate);
    if(error)error->clear();return true;
}

bool SceneDocument::evaluateAssemblyMotionStudy(double timeSeconds,QString* error) {
    if(!std::isfinite(timeSeconds)||timeSeconds<0.0||timeSeconds>3600.0){
        setError(error,"Motion-study time must be finite and between 0 and 3,600 seconds.");return false;
    }
    QMap<QString,double> targets;
    for(const Part& part:parts_){
        const QVector<JointMotionKeyframe>& keys=part.jointMotionKeyframes;if(keys.isEmpty())continue;
        double value=keys.first().value;
        if(timeSeconds>=keys.last().timeSeconds)value=keys.last().value;
        else if(timeSeconds>keys.first().timeSeconds){
            for(int i=1;i<keys.size();++i)if(timeSeconds<=keys[i].timeSeconds){
                const JointMotionKeyframe& a=keys[i-1];const JointMotionKeyframe& b=keys[i];
                const double ratio=(timeSeconds-a.timeSeconds)/(b.timeSeconds-a.timeSeconds);
                value=a.value+(b.value-a.value)*ratio;break;
            }
        }
        targets.insert(part.id,value);
    }
    if(targets.isEmpty()){setError(error,"No saved joint motion tracks are available in this assembly.");return false;}
    return solveAssemblyPose(targets,error);
}

static bool motionSampleTimes(const QVector<Part>& parts,double startSeconds,double endSeconds,
                              int sampleCount,QVector<double>* times,QString* error) {
    if(!times)return false;
    QVector<double> candidate;candidate.reserve(sampleCount);
    for(int index=0;index<sampleCount;++index)
        candidate.append(startSeconds+(endSeconds-startSeconds)*index/(sampleCount-1));
    for(const Part& part:parts)for(const JointMotionKeyframe& key:part.jointMotionKeyframes)
        if(key.timeSeconds>startSeconds&&key.timeSeconds<endSeconds)candidate.append(key.timeSeconds);
    std::sort(candidate.begin(),candidate.end());
    // Keep distinct requested positions even for a sub-nanosecond interval.
    // Approximate equality is not transitive and can erase a valid endpoint.
    const auto last=std::unique(candidate.begin(),candidate.end());
    candidate.erase(last,candidate.end());
    if(candidate.size()>256){
        if(error)*error=QStringLiteral("Motion scan has more than 256 uniform/keyframe positions; choose a shorter time interval.");
        return false;
    }
    *times=std::move(candidate);return true;
}

bool SceneDocument::scanAssemblyMotionInterference(const QString& firstId,const QString& secondId,
                                                    double startSeconds,double endSeconds,int sampleCount,
                                                    QVector<MotionInterferenceSample>* samples,QString* error) const {
    if(!samples){setError(error,"Provide an output list for motion interference samples.");return false;}
    if(firstId==secondId||!find(firstId)||!find(secondId)){
        setError(error,"Select two distinct existing bodies for the motion scan.");return false;
    }
    if(!std::isfinite(startSeconds)||!std::isfinite(endSeconds)||startSeconds<0||endSeconds>3600||
       endSeconds<=startSeconds||sampleCount<2||sampleCount>51){
        setError(error,"Motion scan needs increasing times within 0-3,600 seconds and 2-51 samples.");return false;
    }
    QVector<double> times;
    if(!motionSampleTimes(parts_,startSeconds,endSeconds,sampleCount,&times,error))return false;
    QVector<MotionInterferenceSample> candidate;candidate.reserve(times.size());
    for(double time:times){
        SceneDocument frame;frame.parts_=parts_;
        QString frameError;
        if(!frame.evaluateAssemblyMotionStudy(time,&frameError)){
            setError(error,QStringLiteral("Motion scan failed at %1 s: %2").arg(time,0,'f',3).arg(frameError));return false;
        }
        InterferenceResult interference;
        if(!frame.measureInterference(firstId,secondId,&interference,&frameError)){
            setError(error,QStringLiteral("Interference scan failed at %1 s: %2").arg(time,0,'f',3).arg(frameError));return false;
        }
        MinimumDistanceResult clearance;
        if(!frame.measureMinimumDistance(firstId,secondId,&clearance,&frameError)){
            setError(error,QStringLiteral("Clearance scan failed at %1 s: %2").arg(time,0,'f',3).arg(frameError));return false;
        }
        candidate.append({time,interference.volume,interference.volume>1e-7,
                          std::max(0.0,clearance.distance),
                          clearance.firstX,clearance.firstY,clearance.firstZ,
                          clearance.secondX,clearance.secondY,clearance.secondZ});
    }
    *samples=std::move(candidate);return true;
}

bool SceneDocument::scanAssemblyMotionAdaptive(const QString& firstId,const QString& secondId,
                                               double startSeconds,double endSeconds,int maximumSamples,
                                               double safetyClearanceMm,double coarseStepSeconds,
                                               double fineStepSeconds,
                                               QVector<MotionInterferenceSample>* samples,
                                               bool* unresolvedIntervals,QString* error,
                                               double provenRelativeSpeedBoundMmPerSecond,
                                               bool* clearanceBoundSatisfied,
                                               double* conservativeClearanceLowerBoundMm,
                                               QVector<MotionClearanceInterval>* intervalAudit) const {
    if(!samples||!unresolvedIntervals||firstId==secondId||!find(firstId)||!find(secondId)){
        setError(error,"Adaptive motion scan requires two existing distinct bodies and output destinations.");
        return false;
    }
    const Part* first=find(firstId);
    const Part* second=find(secondId);
    if(first->isSurfaceBody||second->isSurfaceBody||first->shape.IsNull()||second->shape.IsNull()){
        setError(error,"Adaptive motion scanning requires two nonempty solid bodies.");return false;
    }
    if(!std::isfinite(startSeconds)||!std::isfinite(endSeconds)||startSeconds<0||
       endSeconds>3600||endSeconds<=startSeconds||maximumSamples<2||maximumSamples>256){
        setError(error,"Adaptive motion scan needs increasing times within 0-3,600 s and a 2-256 sample budget.");
        return false;
    }
    QVector<double> times;
    if(!motionSampleTimes(parts_,startSeconds,endSeconds,2,&times,error))return false;
    std::vector<double> initial;
    initial.reserve(static_cast<std::size_t>(times.size()));
    for(double time:times)initial.push_back(time);
    cad::motion::Settings settings;
    settings.maximumSamples=maximumSamples;
    settings.safetyClearanceMm=safetyClearanceMm;
    settings.coarseStepSeconds=coarseStepSeconds;
    settings.fineStepSeconds=fineStepSeconds;
    settings.provenRelativeSpeedBoundMmPerSecond=provenRelativeSpeedBoundMmPerSecond;
    QMap<double,MotionInterferenceSample> evaluated;
    const cad::motion::Evaluate measure=[&](double time,cad::motion::Observation* observation,
                                              std::string* reason)->bool {
        SceneDocument frame;
        frame.parts_=parts_;
        QString failure;
        if(!frame.evaluateAssemblyMotionStudy(time,&failure)){
            if(reason)*reason=QStringLiteral("Motion solve failed at %1 s: %2")
                .arg(time,0,'g',15).arg(failure).toStdString();
            return false;
        }
        InterferenceResult overlap;
        MinimumDistanceResult clearance;
        if(!frame.measureInterference(firstId,secondId,&overlap,&failure)||
           !frame.measureMinimumDistance(firstId,secondId,&clearance,&failure)){
            if(reason)*reason=QStringLiteral("Geometry evaluation failed at %1 s: %2")
                .arg(time,0,'g',15).arg(failure).toStdString();
            return false;
        }
        if(!std::isfinite(overlap.volume)||overlap.volume<0||
           !std::isfinite(clearance.distance)||clearance.distance<0){
            if(reason)*reason="CAD kernel reported invalid overlap or clearance.";
            return false;
        }
        observation->clearanceMm=clearance.distance;
        observation->overlapVolumeMm3=overlap.volume;
        evaluated.insert(time,{time,overlap.volume,overlap.volume>1e-7,
                               clearance.distance,clearance.firstX,clearance.firstY,clearance.firstZ,
                               clearance.secondX,clearance.secondY,clearance.secondZ});
        return true;
    };
    cad::motion::Result result;
    std::string reason;
    if(!cad::motion::scanAdaptive(initial,settings,measure,&result,&reason)){
        setError(error,QString::fromStdString(reason));return false;
    }
    QVector<MotionInterferenceSample> candidate;
    candidate.reserve(static_cast<qsizetype>(result.observations.size()));
    for(const auto& observation:result.observations)
        candidate.append(evaluated.value(observation.timeSeconds));
    QVector<MotionClearanceInterval> intervalCandidate;
    if(intervalAudit){
        intervalCandidate.reserve(static_cast<qsizetype>(result.intervals.size()));
        for(const auto& item:result.intervals)
            intervalCandidate.append({item.startSeconds,item.endSeconds,
                item.minimumEndpointClearanceMm,item.clearanceLowerBoundMm,
                item.speedBoundAvailable,item.sampledOverlap,item.provenAboveMargin,
                item.refinementNeeded});
    }
    *samples=std::move(candidate);
    *unresolvedIntervals=result.budgetExhausted;
    if(intervalAudit)*intervalAudit=std::move(intervalCandidate);
    if(clearanceBoundSatisfied)*clearanceBoundSatisfied=result.clearanceBoundSatisfied;
    if(conservativeClearanceLowerBoundMm)
        *conservativeClearanceLowerBoundMm=result.conservativeClearanceLowerBoundMm;
    if(error)error->clear();
    return true;
}

bool SceneDocument::scanAssemblyMotionAllPairs(double startSeconds,double endSeconds,int sampleCount,
                                                QVector<MotionInterferenceContact>* contacts,QString* error) const {
    if(!contacts){setError(error,"Provide an output list for assembly motion contacts.");return false;}
    if(!std::isfinite(startSeconds)||!std::isfinite(endSeconds)||startSeconds<0||endSeconds>3600||
       endSeconds<=startSeconds||sampleCount<2||sampleCount>51){
        setError(error,"Motion scan needs increasing times within 0-3,600 seconds and 2-51 samples.");return false;
    }
    QVector<int> solidIndexes;
    for(int index=0;index<parts_.size();++index){
        if(parts_[index].isSurfaceBody)continue;
        if(parts_[index].shape.IsNull()){
            setError(error,"Assembly motion scan found a solid body without geometry.");return false;
        }
        solidIndexes.append(index);
    }
    if(solidIndexes.size()<2||solidIndexes.size()>20){
        setError(error,"All-pairs motion scanning needs 2 to 20 solid bodies.");return false;
    }
    QVector<double> times;
    if(!motionSampleTimes(parts_,startSeconds,endSeconds,sampleCount,&times,error))return false;
    QVector<MotionInterferenceContact> candidate;
    for(double time:times){
        SceneDocument frame;frame.parts_=parts_;
        QString frameError;
        if(!frame.evaluateAssemblyMotionStudy(time,&frameError)){
            setError(error,QStringLiteral("All-pairs motion scan failed at %1 s: %2")
                .arg(time,0,'f',3).arg(frameError));return false;
        }
        QVector<Bnd_Box> bounds;bounds.reserve(solidIndexes.size());
        try{
            for(int index:solidIndexes){
                Bnd_Box box;BRepBndLib::Add(frame.parts_[index].shape,box);
                if(box.IsVoid()){setError(error,"Assembly motion scan could not bound a solid body.");return false;}
                bounds.append(box);
            }
        }catch(const Standard_Failure& failure){
            setError(error,QStringLiteral("Could not prepare motion-scan bounds: %1").arg(failure.what()));return false;
        }
        for(int first=0;first<solidIndexes.size();++first){
            for(int second=first+1;second<solidIndexes.size();++second){
                if(bounds[first].IsOut(bounds[second]))continue;
                const QString& firstId=frame.parts_[solidIndexes[first]].id;
                const QString& secondId=frame.parts_[solidIndexes[second]].id;
                InterferenceResult interference;
                if(!frame.measureInterference(firstId,secondId,&interference,&frameError)){
                    setError(error,QStringLiteral("Pairwise motion scan failed at %1 s: %2")
                        .arg(time,0,'f',3).arg(frameError));return false;
                }
                if(interference.volume>1e-7)
                    candidate.append({time,firstId,secondId,interference.volume});
            }
        }
    }
    *contacts=std::move(candidate);return true;
}

bool SceneDocument::setDrawingSheetSettings(const QString& partId,const DrawingSheetSettings& settings,QString* error) {
    DrawingSheetSettings persisted;
    if(!prepareDrawingSheetSettings(partId,settings,&persisted,error))return false;
    Part* part=nullptr;for(Part& candidate:parts_)if(candidate.id==partId){part=&candidate;break;}
    // An unchanged sheet does not consume undo history or clear redo.
    if(part->hasDrawingSettings&&part->drawingSettings==persisted)return true;
    checkpoint();part->drawingSettings=std::move(persisted);part->hasDrawingSettings=true;return true;
}

// Validate and assign stable drawing identities without modifying the scene.
// Exporters use this preflight so failed SVG/PDF output never consumes undo history.
bool SceneDocument::prepareDrawingSheetSettings(const QString& partId,const DrawingSheetSettings& settings,
                                                DrawingSheetSettings* prepared,QString* error) const {
    if(!prepared){setError(error,"Provide an output record for prepared drawing settings.");return false;}
    const Part* part=find(partId);
    if(!part){setError(error,"Object not found.");return false;}
    DrawingSheetSettings persisted=settings;
    if(persisted.sourceBodyIds.isEmpty()){
        // An annotation update that omits the source list must not reduce
        // an existing multi-body drawing to its sheet owner.
        if(part->hasDrawingSettings&&!part->drawingSettings.sourceBodyIds.isEmpty())
            persisted.sourceBodyIds=part->drawingSettings.sourceBodyIds;
        else
            persisted.sourceBodyIds.append(partId);
    }
    for(const QString& sourceId:persisted.sourceBodyIds)if(!find(sourceId)){
        setError(error,"A drawing source body no longer exists in this scene.");return false;
    }
    const bool sourceSetChanged=part->hasDrawingSettings&&
        persisted.sourceBodyIds!=part->drawingSettings.sourceBodyIds;
    // Omitted stable IDs in a partial sheet update must not silently create
    // a different sheet, set of projected views, or linked parts-list record.
    // An explicit nonempty ID is still honored (e.g. a new export revision).
    if(part->hasDrawingSettings){
        const DrawingSheetSettings& previous=part->drawingSettings;
        if(persisted.sheetId.isEmpty())persisted.sheetId=previous.sheetId;
        if(persisted.partsListAnnotation.id.isEmpty())persisted.partsListAnnotation.id=previous.partsListAnnotation.id;
        for(auto it=previous.viewIds.cbegin();it!=previous.viewIds.cend();++it)
            if(persisted.viewIds.value(it.key()).isEmpty())persisted.viewIds.insert(it.key(),it.value());
        if(persisted.dimensionAnnotations.isEmpty())persisted.dimensionAnnotations=previous.dimensionAnnotations;
        if(persisted.noteAnnotations.isEmpty()&&!persisted.generalNotes.isEmpty()){
            for(int i=0;i<persisted.generalNotes.size()&&i<previous.noteAnnotations.size();++i){
                DrawingNoteAnnotation note=previous.noteAnnotations[i];
                note.text=persisted.generalNotes[i];
                persisted.noteAnnotations.append(note);
            }
        }
    }
    if(persisted.sheetId.isEmpty())persisted.sheetId=QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(persisted.partsListAnnotation.id.isEmpty())persisted.partsListAnnotation.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(persisted.sourceGeometryToken.isEmpty()){
        // Leaving an existing sheet's token blank must not silently acknowledge
        // geometry changes. Only refreshDrawingSheetSource() explicitly re-links it.
        if(part->hasDrawingSettings){
            persisted.sourceGeometryToken=part->drawingSettings.sourceGeometryToken;
        }else if(persisted.sourceBodyIds.size()==1){
            persisted.sourceGeometryToken=find(persisted.sourceBodyIds.first())->geometryToken;
        }else{
            QVector<const Part*> sources;
            sources.reserve(persisted.sourceBodyIds.size());
            for(const QString& id:persisted.sourceBodyIds)sources.append(find(id));
            persisted.sourceGeometryToken=drawingSourceFingerprint(sources);
        }
    }
    if(persisted.sourceDataToken.isEmpty()||sourceSetChanged){
        if(part->hasDrawingSettings&&!sourceSetChanged)persisted.sourceDataToken=part->drawingSettings.sourceDataToken;
        else{
            QVector<const Part*> sources;sources.reserve(persisted.sourceBodyIds.size());
            for(const QString& id:persisted.sourceBodyIds)sources.append(find(id));
            persisted.sourceDataToken=drawingSourceDataFingerprint(sources);
        }
    }
    for(const QString& view:{QStringLiteral("TOP"),QStringLiteral("FRONT"),QStringLiteral("RIGHT"),QStringLiteral("AUXILIARY")})
        if(persisted.viewIds.value(view).isEmpty())persisted.viewIds.insert(view,QUuid::createUuid().toString(QUuid::WithoutBraces));
    const double paperScale=persisted.sheetSize=="A3"?420.0/297.0:1.0;
    const double notesX=(persisted.projection=="Third-angle"?12.0:151.0)*paperScale;
    if(persisted.noteAnnotations.isEmpty()&&!persisted.generalNotes.isEmpty()){
        for(int i=0;i<persisted.generalNotes.size();++i)
            persisted.noteAnnotations.append({{},persisted.generalNotes[i],notesX,(180.0+4.0*i)*paperScale});
    }else if(!persisted.noteAnnotations.isEmpty()){
        bool textMatches=persisted.generalNotes.size()==persisted.noteAnnotations.size();
        if(textMatches)for(int i=0;i<persisted.generalNotes.size();++i)if(persisted.generalNotes[i]!=persisted.noteAnnotations[i].text){textMatches=false;break;}
        if(!textMatches){
            const QVector<DrawingNoteAnnotation> previous=persisted.noteAnnotations;persisted.noteAnnotations.clear();
            for(int i=0;i<persisted.generalNotes.size();++i){
                DrawingNoteAnnotation note;
                if(i<previous.size()){note=previous[i];note.text=persisted.generalNotes[i];}
                else{note.text=persisted.generalNotes[i];note.xMm=notesX;note.yMm=(180.0+4.0*i)*paperScale;}
                persisted.noteAnnotations.append(note);
            }
        }
    }
    for(DrawingNoteAnnotation& note:persisted.noteAnnotations)if(note.id.isEmpty())note.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(persisted.dimensionAnnotations.isEmpty()){
        for(const QString& axis:{QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")})
            persisted.dimensionAnnotations.append({QUuid::createUuid().toString(QUuid::WithoutBraces),axis,9.0});
    }else for(DrawingDimensionAnnotation& dimension:persisted.dimensionAnnotations)
        if(dimension.id.isEmpty())dimension.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    for(DrawingEdgeDimensionAnnotation& dimension:persisted.edgeDimensionAnnotations)
        if(dimension.id.isEmpty())dimension.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    for(DrawingBalloonAnnotation& balloon:persisted.balloonAnnotations)
        if(balloon.id.isEmpty())balloon.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    for(DrawingDetailAnnotation& detail:persisted.detailAnnotations)
        if(detail.id.isEmpty())detail.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    for(DrawingSectionAnnotation& section:persisted.sectionAnnotations)
        if(section.id.isEmpty())section.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(!validDrawingSettings(persisted,error))return false;
    // A generic annotation edit must not "approve" an old witnessed edge by
    // overwriting only its source revision. Require a current, unique OCCT
    // match before accepting any changed token. The strict review API above
    // additionally re-measures the drawing and repairs the current edge index.
    if(part->hasDrawingSettings){
        for(const auto& dimension:persisted.edgeDimensionAnnotations){
            const auto& previous=part->drawingSettings.edgeDimensionAnnotations;
            const auto old=std::find_if(previous.cbegin(),previous.cend(),
                [&](const DrawingEdgeDimensionAnnotation& d){return d.id==dimension.id;});
            if(old==previous.cend()||old->sourceGeometryToken==dimension.sourceGeometryToken||
               dimension.edgeAnchor.isEmpty())continue;
            const Part* source=find(dimension.sourceBodyId);
            int uniqueIndex=0;
            QString why;
            if(!source||dimension.sourceGeometryToken!=source->geometryToken||
               (!resolveStableEdgeAnchor(source->shape,dimension.edgeAnchor,&uniqueIndex,&why)&&
                (dimension.kind==QStringLiteral("LENGTH")||
                 !resolveResizedFullCircleForDrawing(source->shape,dimension.edgeAnchor,
                                                     &uniqueIndex,&why)))){
                setError(error,QStringLiteral("Drawing edge review rejected: %1. Re-select the source edge.")
                    .arg(why.isEmpty()?QStringLiteral("source revision is not current"):why));
                return false;
            }
        }
    }
    // A persistent sheet is a document-level identity, not merely a per-body
    // label. Duplicate IDs make saved view/annotation links ambiguous.
    for(const Part& other:parts_){
        if(other.id!=partId&&other.hasDrawingSettings&&other.drawingSettings.sheetId==persisted.sheetId){
            setError(error,"Another saved drawing sheet already uses this sheet identity.");return false;
        }
    }
    *prepared=std::move(persisted);
    return true;
}

DrawingSheetSourceStatus SceneDocument::drawingSheetSourceStatus(const QString& partId) const {
    DrawingSheetSourceStatus report;const Part* master=find(partId);
    if(!master||!master->hasDrawingSettings)return report;
    report.sourceBodyIds=master->drawingSettings.sourceBodyIds;
    if(report.sourceBodyIds.isEmpty())report.sourceBodyIds.append(master->id);
    report.storedGeometryToken=master->drawingSettings.sourceGeometryToken;
    report.storedDataToken=master->drawingSettings.sourceDataToken;
    if(report.sourceBodyIds.size()>50){report.state=DrawingSourceState::Stale;return report;}
    QVector<const Part*> sources;sources.reserve(report.sourceBodyIds.size());
    for(const QString& sourceId:report.sourceBodyIds){
        const Part* source=find(sourceId);
        if(!source)report.missingSourceBodyIds.append(sourceId);
        else sources.append(source);
    }
    if(!report.missingSourceBodyIds.isEmpty()){
        report.missingSourceBodyIds.removeDuplicates();report.missingSourceBodyIds.sort();
        report.state=DrawingSourceState::MissingBodies;return report;
    }
    report.currentGeometryToken=drawingSourceFingerprint(sources);
    report.currentDataToken=drawingSourceDataFingerprint(sources);
    const bool matchesAggregate=report.storedGeometryToken==report.currentGeometryToken;
    const bool matchesSingleBody=sources.size()==1&&report.storedGeometryToken==sources.first()->geometryToken;
    const bool metadataMatches=report.storedDataToken==report.currentDataToken;
    // Sheet-level acknowledgement is separate from per-annotation approval.
    // A stale edge measurement or GD&T frame stays stale even after a user
    // refreshes the aggregate source fingerprint. This protects references
    // that may have been trimmed, split, merged or deleted in a Boolean edit.
    for(const DrawingEdgeDimensionAnnotation& annotation:master->drawingSettings.edgeDimensionAnnotations){
        const Part* source=find(annotation.sourceBodyId);
        if(!source||source->geometryToken!=annotation.sourceGeometryToken)
            report.staleAnnotationIds.append(annotation.id);
    }
    for(const DrawingGdtFrameAnnotation& annotation:master->drawingSettings.gdtFrames){
        const Part* source=find(annotation.sourceBodyId);
        if(!source||source->geometryToken!=annotation.sourceGeometryToken)
            report.staleAnnotationIds.append(annotation.id);
    }
    report.staleAnnotationIds.removeDuplicates();report.staleAnnotationIds.sort();
    report.state=((matchesAggregate||matchesSingleBody)&&metadataMatches&&
                  report.staleAnnotationIds.isEmpty())?
                 DrawingSourceState::Current:DrawingSourceState::Stale;
    return report;
}

QString SceneDocument::drawingSourceDataToken(const QStringList& sourceBodyIds,QString* error) const {
    if(sourceBodyIds.isEmpty()||sourceBodyIds.size()>50){setError(error,"A drawing source-data token requires one to 50 source bodies.");return {};}
    QVector<const Part*> sources;sources.reserve(sourceBodyIds.size());QSet<QString> unique;
    for(const QString& id:sourceBodyIds){
        const Part* source=find(id);
        if(id.isEmpty()||unique.contains(id)||!source){setError(error,"Drawing source-data IDs must be unique and refer to existing bodies.");return {};}
        unique.insert(id);sources.append(source);
    }
    return drawingSourceDataFingerprint(sources);
}

bool SceneDocument::refreshDrawingSheetSource(const QString& partId,QString* error) {
    Part* master=nullptr;for(Part& part:parts_)if(part.id==partId){master=&part;break;}
    if(!master||!master->hasDrawingSettings){setError(error,"Select a body with saved drawing-sheet settings.");return false;}
    QStringList sourceIds=master->drawingSettings.sourceBodyIds;if(sourceIds.isEmpty())sourceIds.append(partId);
    if(sourceIds.size()>50){setError(error,"A linked drawing source set cannot exceed 50 bodies.");return false;}
    QVector<const Part*> sources;sources.reserve(sourceIds.size());QSet<QString> unique;
    for(const QString& id:sourceIds){
        if(id.isEmpty()||unique.contains(id)){setError(error,"The saved drawing source set is invalid or contains duplicate body IDs.");return false;}
        unique.insert(id);const Part* source=find(id);
        if(!source){setError(error,QStringLiteral("Drawing source body '%1' is missing; restore it before refreshing the sheet link.").arg(id));return false;}
        sources.append(source);
    }
    const QString current=drawingSourceFingerprint(sources);
    const QString currentData=drawingSourceDataFingerprint(sources);
    if(master->drawingSettings.sourceGeometryToken==current&&master->drawingSettings.sourceDataToken==currentData)return true;
    checkpoint();master->drawingSettings.sourceGeometryToken=current;master->drawingSettings.sourceDataToken=currentData;return true;
}

namespace {
// One authoritative kernel-based reference review, shared by one-at-a-time
// editing and multi-sheet authoring. Failures never change caller's output.
bool prepareReviewedDrawingEdge(const Part& source,
        const DrawingEdgeDimensionAnnotation& original,
        DrawingEdgeDimensionAnnotation* result,QString* error){
    if(!result||source.shape.IsNull()||source.geometryToken.isEmpty()){
        if(error)*error="Drawing edge source geometry or output is missing.";return false;
    }
    const bool alreadyCurrent=original.sourceGeometryToken==source.geometryToken;
    if(!alreadyCurrent&&original.edgeAnchor.isEmpty()){
        if(error)*error="A legacy index-only edge changed revision; capture a new geometric witness.";
        return false;
    }
    DrawingEdgeDimensionAnnotation reviewed=original;
    if(!alreadyCurrent){
        int resolved=0;
        QString witnessError;
        if(!resolveStableEdgeAnchor(source.shape,original.edgeAnchor,&resolved,&witnessError)&&
           (original.kind==QStringLiteral("LENGTH")||
            !resolveResizedFullCircleForDrawing(source.shape,original.edgeAnchor,
                                                &resolved,&witnessError))){
            if(error)*error=QStringLiteral("Cannot approve drawing edge ancestry: %1").arg(witnessError);
            return false;
        }
        reviewed.edgeIndex=resolved;
        reviewed.sourceGeometryToken=source.geometryToken;
    }
    QString measurementError;
    if(!validateDrawingEdgeDimensionPreview(source,reviewed,&measurementError)){
        if(error)*error=QStringLiteral("Cannot approve drawing measurement: %1").arg(measurementError);
        return false;
    }
    *result=std::move(reviewed);
    if(error)error->clear();return true;
}
} // namespace

bool SceneDocument::reviewDrawingEdgeDimension(const QString& sheetOwnerId,
        const QString& annotationId,QString* error){
    const Part* owner=find(sheetOwnerId);
    if(!owner||!owner->hasDrawingSettings||annotationId.trimmed().isEmpty()){
        setError(error,"Select a saved drawing and a nonempty selected-edge annotation ID.");return false;
    }
    const auto& saved=owner->drawingSettings.edgeDimensionAnnotations;
    int annotationIndex=-1;
    for(int i=0;i<saved.size();++i)if(saved[i].id==annotationId){annotationIndex=i;break;}
    if(annotationIndex<0){setError(error,"The selected drawing edge annotation no longer exists.");return false;}
    const DrawingEdgeDimensionAnnotation& original=saved[annotationIndex];
    const Part* source=find(original.sourceBodyId);
    if(!source){setError(error,"The drawing edge source is missing or invalid.");return false;}
    DrawingEdgeDimensionAnnotation reviewed;
    if(!prepareReviewedDrawingEdge(*source,original,&reviewed,error))return false;
    if(reviewed==original){if(error)error->clear();return true;}
    QVector<Part> staged=parts_;
    auto at=std::find_if(staged.begin(),staged.end(),[&](const Part& p){return p.id==sheetOwnerId;});
    if(at==staged.end()){setError(error,"The drawing sheet disappeared during review.");return false;}
    at->drawingSettings.edgeDimensionAnnotations[annotationIndex]=std::move(reviewed);
    checkpoint();
    parts_=std::move(staged);
    if(error)error->clear();return true;
}

// Sprint 14: preview and commit share the same witnessed-edge kernel verifier
// as single-annotation review. One detached scene copy is used for reporting;
// the caller receives all selected revisions in one undo checkpoint or none.
bool SceneDocument::stageDrawingEdgeReviewBatch(
        const QMap<QString,QStringList>& selection,
        QMap<QString,DrawingSheetSettings>* updated,
        QString* report,QString* error) const{
    if(!updated||selection.isEmpty()||selection.size()>40){
        setError(error,"Select 1-40 saved drawing sheets and a separate staging buffer.");return false;
    }
    int total=0;
    QMap<QString,DrawingSheetSettings> proposed;
    for(auto it=selection.cbegin();it!=selection.cend();++it){
        if(it.key().trimmed().isEmpty()||it.value().isEmpty()){
            setError(error,"Every selected drawing must have at least one edge-annotation ID.");return false;
        }
        total+=it.value().size();
        if(total>200){setError(error,"A drawing review transaction supports at most 200 annotations.");return false;}
        const Part* owner=find(it.key());
        if(!owner||!owner->hasDrawingSettings){
            setError(error,QStringLiteral("Missing saved drawing sheet: %1").arg(it.key()));return false;
        }
        QSet<QString> unique;
        DrawingSheetSettings sheet=owner->drawingSettings;
        for(const QString& id:it.value()){
            if(id.trimmed().isEmpty()||unique.contains(id)){
                setError(error,"Edge annotation IDs must be nonempty and unique within each sheet.");return false;
            }
            unique.insert(id);
            auto& saved=sheet.edgeDimensionAnnotations;
            int index=-1;
            for(int j=0;j<saved.size();++j)if(saved[j].id==id){index=j;break;}
            if(index<0){setError(error,QStringLiteral("Drawing annotation %1 is missing from sheet %2.")
                              .arg(id,it.key()));return false;}
            const Part* source=find(saved[index].sourceBodyId);
            if(!source){setError(error,QStringLiteral("Drawing annotation %1 has a missing source.")
                                  .arg(id));return false;}
            DrawingEdgeDimensionAnnotation reviewed;
            QString issue;
            if(!prepareReviewedDrawingEdge(*source,saved[index],&reviewed,&issue)){
                setError(error,QStringLiteral("Sheet %1 / annotation %2: %3")
                    .arg(it.key(),id,issue));return false;
            }
            saved[index]=std::move(reviewed);
        }
        if(!(sheet==owner->drawingSettings))proposed.insert(it.key(),std::move(sheet));
    }
    // One copy of the scene for the entire operation, rather than copying all
    // B-rep-bearing parts for EACH selected annotation. Only the altered sheet
    // settings are replaced; no geometry or GD&T revision is acknowledged.
    SceneDocument staged;staged.parts_=parts_;
    for(Part& part:staged.parts_){
        const auto it=proposed.constFind(part.id);
        if(it!=proposed.cend())part.drawingSettings=*it;
    }
    QStringList lines{QStringLiteral("DRAWING EDGE REVISION REVIEW (geometry checked; no GD&T approval)"),
                      QStringLiteral("requested_annotations=%1").arg(total)};
    for(auto it=selection.cbegin();it!=selection.cend();++it){
        const auto status=staged.drawingSheetSourceStatus(it.key());
        lines.append(QStringLiteral("sheet=%1; selected=%2; pending_stale_annotations=%3; sheet_current=%4")
            .arg(it.key()).arg(it.value().size()).arg(status.staleAnnotationIds.size())
            .arg(status.state==DrawingSourceState::Current?QStringLiteral("YES"):QStringLiteral("NO")));
    }
    lines.append(QStringLiteral("changed_sheets=%1; commit=ONE_UNDO; approval=NOT_ISSUED").arg(proposed.size()));
    *updated=std::move(proposed);
    if(report)*report=lines.join('\n');
    if(error)error->clear();return true;
}

bool SceneDocument::previewDrawingEdgeReviewBatch(
        const QMap<QString,QStringList>& selection,QString* report,QString* error) const{
    if(!report){setError(error,"Provide a destination for the read-only drawing review.");return false;}
    QMap<QString,DrawingSheetSettings> staged;
    return stageDrawingEdgeReviewBatch(selection,&staged,report,error);
}

bool SceneDocument::reviewDrawingEdgeReviewBatch(
        const QMap<QString,QStringList>& selection,QString* report,QString* error){
    QMap<QString,DrawingSheetSettings> staged;
    QString stagedReport;
    if(!stageDrawingEdgeReviewBatch(selection,&staged,&stagedReport,error))return false;
    if(!staged.isEmpty()){
        checkpoint();
        for(Part& part:parts_){
            auto it=staged.constFind(part.id);
            if(it!=staged.cend())part.drawingSettings=*it;
        }
    }
    if(report)*report=std::move(stagedReport);
    if(error)error->clear();return true;
}

// Sprint 16 project-level review joins the geometry, drawing, sheet-metal and
// joint-track validators already implemented in the core. It deliberately
// reports discrete motion-rate limits rather than making a false swept-solid
// collision guarantee. Failing a single body yields a BLOCK row, not a partial
// export failure, so the entire actionable register is available to the user.
bool SceneDocument::buildIntegratedEngineeringReadinessCsv(QByteArray* output,
        QString* error) const{
    if(!output||parts_.isEmpty()||parts_.size()>200){
        setError(error,"A nonempty, bounded CAD document and output buffer are required.");return false;
    }
    const auto escapeCsv=[](QString value)->QString{
        value.replace('\r',QStringLiteral(" "));
        value.replace('\n',QStringLiteral(" "));
        if(!value.trimmed().isEmpty()&&
           QStringLiteral("=+-@\t").contains(value.trimmed().front()))
            value.prepend(QChar(0x27));
        value.replace('"',QStringLiteral("\"\""));
        return QStringLiteral("\"")+value+QStringLiteral("\"");
    };
    QString content=QStringLiteral(
        "CADWorkbench integrated engineering readiness,NOT_ISSUED; HUMAN_SIGNOFF_REQUIRED; MOTION_NOT_CERTIFIED\r\n"
        "Category,Body ID,Status,Finding,Current revision\r\n");
    std::size_t blockers=0,reviews=0,checked=0;
    const auto add=[&](const QString& category,const Part& part,const QString& severity,
                       const QString& finding){
        content+=escapeCsv(category)+','+escapeCsv(part.id)+','+escapeCsv(severity)+','+
                 escapeCsv(finding)+','+escapeCsv(part.geometryToken)+QStringLiteral("\r\n");
        if(severity==QStringLiteral("BLOCK"))++blockers;
        else if(severity==QStringLiteral("REVIEW"))++reviews;
        ++checked;
    };
    QVector<const Part*> ordered;
    ordered.reserve(parts_.size());
    for(const Part& item:parts_)ordered.append(&item);
    std::sort(ordered.begin(),ordered.end(),[](const Part* a,const Part* b){return a->id<b->id;});
    std::size_t sheetCount=0,refoldCount=0,trackCount=0;
    for(const Part* pointer:ordered){
        const Part& part=*pointer;
        QString issue;
        if(part.shape.IsNull()||!validShape(part.shape,&issue))
            add(QStringLiteral("GEOMETRY"),part,QStringLiteral("BLOCK"),
                QStringLiteral("Invalid or missing OCCT geometry: ")+issue);
        else
            add(QStringLiteral("GEOMETRY"),part,QStringLiteral("CHECKED"),
                QStringLiteral("OCCT nominal geometry passes shape validation; real-world accuracy not certified"));
        if(part.hasDrawingSettings){
            ++sheetCount;
            const auto state=drawingSheetSourceStatus(part.id);
            if(state.state!=DrawingSourceState::Current){
                add(QStringLiteral("DRAWING"),part,QStringLiteral("BLOCK"),
                    QStringLiteral("Saved drawing source or annotation is stale; missing sources=%1; stale annotations=%2")
                        .arg(state.missingSourceBodyIds.size()).arg(state.staleAnnotationIds.size()));
            }else{
                QMap<QString,QByteArray> drawings;
                QMap<QString,DrawingSheetSettings> metadata;
                QString details;
                if(!buildSavedDrawingSvgBatch({part.id},&drawings,&metadata,&details,&issue))
                    add(QStringLiteral("DRAWING"),part,QStringLiteral("BLOCK"),
                        QStringLiteral("Strict OCCT drawing regeneration failed: ")+issue);
                else
                    add(QStringLiteral("DRAWING"),part,QStringLiteral("REVIEW"),
                        QStringLiteral("Strict drawing regenerated read-only; annotations and exported SVG require human sign-off"));
            }
        }
        if(part.hasFoldedSheetMetalDefinition){
            ++refoldCount;
            QByteArray foldedAudit;
            if(!auditFoldedSheetRefoldGeometry(part.id,&foldedAudit,&issue))
                add(QStringLiteral("SHEET_METAL"),part,QStringLiteral("BLOCK"),
                    QStringLiteral("Independent folded OCCT refold/volume check failed: ")+issue);
            else
                add(QStringLiteral("SHEET_METAL"),part,QStringLiteral("REVIEW"),
                    QStringLiteral("Stored/refolded OCCT volume and developed mid-surface checked; tooling and springback unverified"));
        }
        if(!part.jointMotionKeyframes.isEmpty()){
            ++trackCount;
            const bool revolute=part.assemblyJointType==QStringLiteral("Revolute");
            const bool prismatic=part.assemblyJointType==QStringLiteral("Prismatic");
            if(!revolute&&!prismatic){
                add(QStringLiteral("JOINT_MOTION"),part,QStringLiteral("BLOCK"),
                    QStringLiteral("Saved motion track is not attached to a supported revolute/prismatic joint"));
            }else{
                std::vector<cad::motion::RateKeyframe> frames;
                frames.reserve(static_cast<std::size_t>(part.jointMotionKeyframes.size()));
                for(const auto& key:part.jointMotionKeyframes)
                    frames.push_back({key.timeSeconds,key.value});
                cad::motion::RateAudit rates;
                std::string reason;
                const double low=revolute?part.jointMinimumDegrees:part.jointMinimumMm;
                const double high=revolute?part.jointMaximumDegrees:part.jointMaximumMm;
                if(!cad::motion::auditKeyframeRates(frames,low,high,&rates,&reason))
                    add(QStringLiteral("JOINT_MOTION"),part,QStringLiteral("BLOCK"),
                        QStringLiteral("Invalid saved joint schedule: ")+QString::fromStdString(reason));
                else
                    add(QStringLiteral("JOINT_MOTION"),part,QStringLiteral("REVIEW"),
                        QStringLiteral("Peak piecewise-linear coordinate rate=%1 %2/s; direction reversals=%3; velocity steps=%4; solid-clearance proof not established")
                            .arg(rates.maximumAbsoluteRatePerSecond,0,'g',16)
                            .arg(revolute?QStringLiteral("deg"):QStringLiteral("mm"))
                            .arg(static_cast<qulonglong>(rates.directionReversals))
                            .arg(static_cast<qulonglong>(rates.velocityDiscontinuities)));
            }
        }
    }
    // Reuse the existing mature project audit rather than leaving parametric
    // history, broken sketch dependencies, feature inspection and mate parents
    // outside the integrated issue register. Informational findings are not
    // release blockers; revision-stale histories ARE blockers in this stricter
    // manufacturing-readiness view, even where the source audit uses Warning.
    const DocumentAuditReport sourceAudit=runDocumentAudit(*this);
    for(const auto& finding:sourceAudit.findings){
        if(finding.severity==AuditSeverity::Information)continue;
        const Part* body=find(finding.bodyId);
        if(!body)continue; // project-wide metadata is checked in sourceAudit
        const bool staleHistory=finding.code==QStringLiteral("PARAMETRIC_STALE")||
            finding.code==QStringLiteral("FEATURE_TREE_STALE")||
            finding.code==QStringLiteral("LINKED_SKETCH_STALE")||
            finding.code==QStringLiteral("SKETCH_STALE")||
            finding.code==QStringLiteral("FOLDED_DEFINITION_STALE");
        add(QStringLiteral("MODEL_AUDIT"),*body,
            finding.severity==AuditSeverity::Error||staleHistory?
                QStringLiteral("BLOCK"):QStringLiteral("REVIEW"),
            finding.code+QStringLiteral(": ")+finding.message+
            (finding.suggestedAction.isEmpty()?QString():
                 QStringLiteral(" Next: ")+finding.suggestedAction));
    }
    if(hasLiveArticulatedNetwork()){
        const Part* fixed=find(liveArticulatedFixed_);
        if(fixed)add(QStringLiteral("ARTICULATED_ASSEMBLY"),*fixed,QStringLiteral("REVIEW"),
            QStringLiteral("Saved connected graph present with %1 mates; verify full motion and physical clearance independently")
                .arg(liveArticulatedJoints_.size()));
    }
    content+=QStringLiteral("SUMMARY,PROJECT,BLOCKED,blockers=%1; human_reviews=%2; records=%3; saved_sheets=%4; folded_parts=%5; motion_tracks=%6; engineering_approval=NOT_ISSUED,\r\n")
        .arg(static_cast<qulonglong>(blockers))
        .arg(static_cast<qulonglong>(reviews)).arg(static_cast<qulonglong>(checked))
        .arg(static_cast<qulonglong>(sheetCount)).arg(static_cast<qulonglong>(refoldCount))
        .arg(static_cast<qulonglong>(trackCount));
    const QByteArray complete=content.toUtf8();
    if(complete.size()>2*1024*1024){
        setError(error,"Integrated engineering preflight exceeded the 2 MiB report limit.");return false;
    }
    *output=complete;
    if(error)error->clear();
    return true;
}

bool SceneDocument::exportIntegratedEngineeringReadinessCsv(const QString& path,
        QString* error) const {
    if(path.trimmed().isEmpty()){
        setError(error,"Choose a destination for the unissued engineering readiness CSV.");return false;
    }
    QByteArray report;
    if(!buildIntegratedEngineeringReadinessCsv(&report,error))return false;
    QSaveFile saved(path);
    if(!saved.open(QIODevice::WriteOnly)||saved.write(report)!=report.size()||!saved.commit()){
        saved.cancelWriting();
        setError(error,QStringLiteral("Failed to atomically save engineering readiness CSV: %1").arg(saved.errorString()));
        return false;
    }
    if(error)error->clear();return true;
}

bool SceneDocument::buildDrawingRecoveryCsv(QByteArray* output,QString* error) const{
    if(!output){setError(error,"Provide a drawing recovery CSV output buffer.");return false;}
    const auto quote=[](QString value){
        if(!value.trimmed().isEmpty()&&QStringLiteral("=+-@\t\r").contains(value.trimmed().front()))
            value.prepend(QChar(0x27));
        value.replace('"',QStringLiteral("\"\""));
        return QStringLiteral("\"")+value+QStringLiteral("\"");
    };
    QString text=QStringLiteral("CADWorkbench cross-sheet recovery register,NOT ISSUED - ENGINEERING REVIEW REQUIRED\r\n");
    text+=QStringLiteral("Sheet body ID,Sheet ID,Drawing number,Reference kind,Reference ID,Source body ID,Current geometry revision,Captured geometry revision,Review status\r\n");
    int total=0;
    for(const Part& part:parts_){
        if(!part.hasDrawingSettings)continue;
        const DrawingSheetSettings& sheet=part.drawingSettings;
        const auto status=drawingSheetSourceStatus(part.id);
        const auto append=[&](const QString& kind,const QString& id,const QString& body,
                              const QString& revision,const QString& state){
            const Part* source=find(body);
            text+=quote(part.id)+','+quote(sheet.sheetId)+','+quote(sheet.drawingNumber)+','+
                quote(kind)+','+quote(id)+','+quote(body)+','+
                quote(source?source->geometryToken:QStringLiteral("MISSING"))+','+
                quote(revision)+','+quote(state)+QStringLiteral("\r\n");
        };
        const QString aggregate=status.state==DrawingSourceState::Current?
            QStringLiteral("CURRENT"):QStringLiteral("REVIEW_REQUIRED");
        append(QStringLiteral("SHEET"),sheet.sheetId,part.id,sheet.sourceGeometryToken,aggregate);
        ++total;
        for(const auto& edge:sheet.edgeDimensionAnnotations){
            const Part* source=find(edge.sourceBodyId);
            const bool stale=!source||source->geometryToken!=edge.sourceGeometryToken;
            append(QStringLiteral("EDGE_DIMENSION"),edge.id,edge.sourceBodyId,edge.sourceGeometryToken,
                   stale?QStringLiteral("REVIEW_REQUIRED"):QStringLiteral("TOKEN_CURRENT_GEOMETRY_NOT_RECHECKED"));
            ++total;
        }
        for(const auto& frame:sheet.gdtFrames){
            const Part* source=find(frame.sourceBodyId);
            const bool stale=!source||source->geometryToken!=frame.sourceGeometryToken;
            append(QStringLiteral("GD_AND_T"),frame.id,frame.sourceBodyId,frame.sourceGeometryToken,
                   stale?QStringLiteral("REVIEW_REQUIRED"):QStringLiteral("TOKEN_CURRENT_HUMAN_REVIEW_REQUIRED"));
            ++total;
        }
        if(total>20000){setError(error,"Drawing recovery register exceeds 20000 reference records.");return false;}
    }
    *output=text.toUtf8();if(error)error->clear();return true;
}

bool SceneDocument::exportDrawingRecoveryCsv(const QString& path,QString* error) const{
    if(path.trimmed().isEmpty()){setError(error,"Select a drawing recovery CSV filename.");return false;}
    QByteArray content;
    if(!buildDrawingRecoveryCsv(&content,error))return false;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)||file.write(content)!=content.size()||!file.commit()){
        setError(error,QStringLiteral("Could not atomically export drawing recovery CSV: %1").arg(file.errorString()));return false;
    }
    if(error)error->clear();return true;
}

bool SceneDocument::buildSavedDrawingSvgBatch(
        const QStringList& sheetOwnerIds,QMap<QString,QByteArray>* svgByOwner,
        QMap<QString,DrawingSheetSettings>* preparedByOwner,
        QString* report,QString* error) const{
    if(!svgByOwner||!preparedByOwner||svgByOwner==nullptr||preparedByOwner==nullptr||
       sheetOwnerIds.isEmpty()||sheetOwnerIds.size()>12){
        setError(error,"Select 1-12 saved drawing owners and distinct output maps.");return false;
    }
    QStringList owners=sheetOwnerIds;
    owners.sort();
    if(std::adjacent_find(owners.cbegin(),owners.cend())!=owners.cend()){
        setError(error,"Drawing regeneration batch has duplicate sheet-owner IDs.");return false;
    }
    QMap<QString,QByteArray> generated;
    QMap<QString,DrawingSheetSettings> staged;
    QStringList lines{QStringLiteral("READ-ONLY LINKED DRAWING REGENERATION BATCH"),
                      QStringLiteral("human_review=REQUIRED; drawing_issue=NOT_ISSUED")};
    qsizetype totalBytes=0;
    for(const QString& ownerId:owners){
        const Part* owner=find(ownerId);
        if(!owner||!owner->hasDrawingSettings){
            setError(error,QStringLiteral("No saved drawing sheet for owner '%1'.").arg(ownerId));return false;
        }
        DrawingSheetSettings next=owner->drawingSettings;
        QStringList ids=next.sourceBodyIds;
        if(ids.isEmpty())ids<<ownerId;
        ids.sort();
        if(ids.size()>50||std::adjacent_find(ids.cbegin(),ids.cend())!=ids.cend()){
            setError(error,"A drawing batch source set exceeds 50 bodies or has duplicate IDs.");return false;
        }
        QVector<Part> sources;sources.reserve(ids.size());
        for(const QString& id:ids){
            const Part* body=find(id);
            if(!body){setError(error,QStringLiteral("Missing drawing source: %1").arg(id));return false;}
            sources.append(*body);
        }
        // Neither a full sheet refresh nor a fresh SVG should silently
        // approve stale individually witnessed edges or datum annotations.
        for(const auto& edge:next.edgeDimensionAnnotations){
            const Part* source=find(edge.sourceBodyId);
            if(!source||source->geometryToken!=edge.sourceGeometryToken){
                setError(error,QStringLiteral("Sheet %1: edge %2 is stale. Review its geometric witness first.")
                    .arg(ownerId,edge.id));return false;
            }
            QString issue;
            if(!validateDrawingEdgeDimensionPreview(*source,edge,&issue)){
                setError(error,QStringLiteral("Sheet %1: invalid edge %2: %3")
                    .arg(ownerId,edge.id,issue));return false;
            }
        }
        for(const auto& gdt:next.gdtFrames){
            const Part* source=find(gdt.sourceBodyId);
            if(!source||source->geometryToken!=gdt.sourceGeometryToken){
                setError(error,QStringLiteral("Sheet %1: GD&T %2 is stale and requires independent review.")
                    .arg(ownerId,gdt.id));return false;
            }
        }
        Part composite;
        QString issue;
        if(!buildDrawingAssemblyPart(sources,&composite,&issue)){
            setError(error,issue);return false;
        }
        next.sourceBodyIds=ids;
        next.sourceGeometryToken=composite.geometryToken;
        next.sourceDataToken=drawingSourceDataToken(ids,&issue);
        if(!issue.isEmpty()){setError(error,issue);return false;}
        DrawingSheetSettings prepared;
        if(!prepareDrawingSheetSettings(ownerId,next,&prepared,&issue)){
            setError(error,issue);return false;
        }
        const auto projection=prepared.projection==QStringLiteral("First-angle")?
            OrthographicProjection::FirstAngle:OrthographicProjection::ThirdAngle;
        const auto size=prepared.sheetSize==QStringLiteral("A3")?
            DrawingSheetSize::A3Landscape:DrawingSheetSize::A4Landscape;
        const auto auxiliary=prepared.auxiliaryView==QStringLiteral("Section A-A")?
            DrawingAuxiliaryView::SectionA:DrawingAuxiliaryView::Isometric;
        const DrawingTitleBlock title{prepared.drawingNumber,prepared.drawnBy,
           prepared.checkedBy,prepared.issueDate,prepared.revision};
        const auto bom=billOfMaterials(ids,&issue);
        if(!issue.isEmpty()){setError(error,issue);return false;}
        QByteArray svg;
        if(!buildOrthographicDrawingSvg(composite,&svg,&issue,nullptr,nullptr,nullptr,
            projection,size,auxiliary,prepared.sectionDepthFraction,
            prepared.generalNotes,title,prepared.viewCenters,prepared.viewIds,
            prepared.sheetId,prepared.noteAnnotations,prepared.dimensionAnnotations,
            prepared.partsListAnnotation,bom,ids,prepared.balloonAnnotations,
            prepared.detailAnnotations,prepared.sectionAnnotations,
            prepared.edgeDimensionAnnotations,sources,prepared.gdtFrames)){
            setError(error,QStringLiteral("Sheet %1 failed to regenerate: %2")
                .arg(ownerId,issue));return false;
        }
        totalBytes+=svg.size();
        if(svg.isEmpty()||totalBytes>32*1024*1024){
            setError(error,"Linked drawing SVG batch is empty or exceeds the 32 MiB budget.");return false;
        }
        lines<<QStringLiteral("owner=%1; sheet=%2; source_bodies=%3; svg_bytes=%4; GD&T=NOT_APPROVED")
            .arg(ownerId,prepared.sheetId).arg(ids.size()).arg(svg.size());
        generated.insert(ownerId,std::move(svg));
        staged.insert(ownerId,std::move(prepared));
    }
    *svgByOwner=std::move(generated);
    *preparedByOwner=std::move(staged);
    if(report)*report=lines.join('\n');
    if(error)error->clear();
    return true;
}

bool SceneDocument::exportSavedDrawingSvgBatch(const QStringList& sheetOwnerIds,
        const QString& newDirectory,QString* report,QString* error) const{
    if(newDirectory.trimmed().isEmpty()){
        setError(error,"Provide a new, nonexisting drawing export directory.");return false;
    }
    const QFileInfo destination(newDirectory);
    if(destination.exists()||!QDir(destination.absolutePath()).exists()){
        setError(error,"The drawing destination must not exist and its parent must exist.");return false;
    }
    QMap<QString,QByteArray> generated;
    QMap<QString,DrawingSheetSettings> metadata;
    QString nextReport;
    if(!buildSavedDrawingSvgBatch(sheetOwnerIds,&generated,&metadata,&nextReport,error))return false;
    QTemporaryDir staging(destination.absolutePath()+QStringLiteral("/.cad_drawings_staging_XXXXXX"));
    if(!staging.isValid()){
        setError(error,"Cannot create a same-filesystem drawing staging directory.");return false;
    }
    QByteArray index("Sheet,Owner,Source revision,SHA256,Status\r\n");
    int count=0;
    for(auto it=generated.cbegin();it!=generated.cend();++it){
        const QString filename=QStringLiteral("sheet-%1.svg").arg(++count,3,10,QChar('0'));
        QSaveFile file(staging.filePath(filename));
        if(!file.open(QIODevice::WriteOnly)||file.write(it.value())!=it.value().size()||!file.commit()){
            setError(error,"Could not write the full regenerated drawing set.");return false;
        }
        const auto digest=QCryptographicHash::hash(it.value(),QCryptographicHash::Sha256).toHex();
        // Use safe numeric filenames: user-authored labels never enter paths.
        const QString row=QStringLiteral("%1,%2,%3,%4,NOT_ISSUED\r\n")
            .arg(filename,it.key(),metadata.value(it.key()).sourceGeometryToken,
                 QString::fromLatin1(digest));
        // Source tokens and UUIDs are system-generated, but CSV-formula
        // safety is still required for arbitrary imported owner identities.
        if(it.key().contains(',')||it.key().contains('\n')||it.key().contains('\r')||
           QStringLiteral("=+-@").contains(it.key().left(1))){
            setError(error,"An unsafe drawing-owner identity cannot be exported in CSV.");return false;
        }
        index+=row.toUtf8();
    }
    QSaveFile manifest(staging.filePath(QStringLiteral("DRAWING_BATCH_INDEX.csv")));
    if(!manifest.open(QIODevice::WriteOnly)||manifest.write(index)!=index.size()||!manifest.commit()){
        setError(error,"Could not finalize drawing batch index.");return false;
    }
    // Directory rename publishes every exported sheet together. A partial
    // set is never placed in the final destination on failed rendering/write.
    if(QFileInfo::exists(newDirectory)||!QDir().rename(staging.path(),destination.absoluteFilePath())){
        setError(error,"Could not atomically publish the new drawing directory.");return false;
    }
    if(report)*report=std::move(nextReport);
    if(error)error->clear();
    return true;
}

bool SceneDocument::setInterleavedFeatureHistory(const QString& id,
                  const QVector<InterleavedHistoryStep>& steps,QString* error){
    const Part* original=find(id);
    if(!original||original->isSurfaceBody||original->hasConstrainedPolygonDefinition||
       original->hasLinkedBoolean||original->hasLinkedSketchExtrusion||
       !original->linkedFoldedFlatSourceId.isEmpty()||
       original->hasInPlaceSketchFeature||original->hasShapeFeatureTree||
       original->hasParametricDefinition||original->hasFoldedSheetMetalDefinition||
       !original->componentDefinitionId.isEmpty()||
       !original->assemblyJointType.isEmpty()||steps.isEmpty()||steps.size()>40){
        setError(error,"Use 1-40 interleaved operations on an ordinary solid without a competing feature definition.");
        return false;
    }
    // A shape-only sequence belongs in the ordinary feature tree.  The
    // interleaved mode must actually depend on an external sketch: otherwise
    // it would have no dependency-graph edge and would not be regenerated.
    if(std::none_of(steps.cbegin(),steps.cend(),[](const InterleavedHistoryStep& step){
            return step.kind==QStringLiteral("SketchFuse")||
                   step.kind==QStringLiteral("SketchCut");})){
        setError(error,"An interleaved history requires at least one external sketch stage. Use the ordinary feature tree for shape-only histories.");
        return false;
    }
    // A fresh history stores the unmodified body; an edited history replays
    // from its original frozen base, never from the previous final output.
    QVector<Part> candidate=parts_;
    for(Part& part:candidate)if(part.id==id){
        if(!part.hasInterleavedHistory){
            part.hasInterleavedHistory=true;part.interleavedBase=part.shape;
        }
        part.interleavedSteps=steps;
        part.interleavedSourceTokens.clear();
        part.interleavedGeometryToken.clear();
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::previewInterleavedHistoryWithDependents(const QString& id,
        const QVector<InterleavedHistoryStep>& steps,QString* report,QString* error) const{
    if(!report){setError(error,"Provide an output report for the dependent-history preflight.");return false;}
    // The normal commit path has the authoritative dependency planner and
    // geometry validator. Replay it in a disposable document rather than
    // maintaining a second, inevitably divergent preview algorithm.
    SceneDocument staged;
    staged.parts_=parts_;
    QString issue;
    if(!staged.setInterleavedFeatureHistory(id,steps,&issue)){
        setError(error,QString("Full linked history rebuild rejected: %1").arg(issue));
        return false; // The caller's previous report and this document are untouched.
    }
    QStringList lines{
        "READ-ONLY FULL PARAMETRIC DEPENDENCY PREFLIGHT",
        QString("Target: %1; staged steps: %2").arg(id).arg(steps.size()),
        "The complete linked-body dependency graph was rebuilt in a detached scene."};
    int changed=0;
    for(const Part& after:staged.parts_){
        const Part* before=find(after.id);
        if(!before){setError(error,"A staged dependency unexpectedly introduced a body.");return false;}
        if(before->geometryToken==after.geometryToken)continue;
        ++changed;
        GProp_GProps properties;
        if(after.shape.IsNull()||!BRepCheck_Analyzer(after.shape).IsValid()){
            setError(error,QString("An invalid staged descendant was found: %1").arg(after.id));return false;
        }
        BRepGProp::VolumeProperties(after.shape,properties);
        if(!std::isfinite(properties.Mass())||properties.Mass()<=0){
            setError(error,QString("Staged descendant has invalid volume: %1").arg(after.id));return false;
        }
        lines.append(QString("REBUILT %1: valid solid, volume %2 mm^3")
            .arg(after.id).arg(properties.Mass(),0,'g',14));
    }
    lines.append(QString("Changed solid bodies: %1; no live document changes committed.").arg(changed));
    *report=lines.join('\n');
    if(error)error->clear();
    return true;
}

bool SceneDocument::interleavedFeatureHistoryCurrent(const QString& id) const{
    const Part* part=find(id);
    if(!part||!part->hasInterleavedHistory||part->interleavedGeometryToken!=part->geometryToken||
       part->interleavedSourceTokens.size()!=part->interleavedSteps.size())return false;
    for(int i=0;i<part->interleavedSteps.size();++i){
        const auto& step=part->interleavedSteps[i];
        if(step.kind==QStringLiteral("Shape"))continue;
        const Part* source=find(step.sketchSourceId);
        if(!source||!source->hasConstrainedPolygonDefinition||
           source->geometryToken!=part->interleavedSourceTokens[i]||
           source->constrainedPolygonGeometryToken!=source->geometryToken)return false;
    }
    return true;
}

bool SceneDocument::shapeFeatureTreeCurrent(const QString& partId) const {
    const Part* part=find(partId);
    return part&&part->hasShapeFeatureTree&&part->shapeFeatureGeometryToken==part->geometryToken;
}

bool SceneDocument::setShapeFeatureTree(const QString& partId,const QVector<ShapeFeatureNode>& nodes,QString* error){
    int index=-1;
    for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Select an existing body.");return false;}
    const Part& part=parts_[index];
    if(part.hasLinkedBoolean||part.hasLinkedSketchExtrusion||!part.linkedFoldedFlatSourceId.isEmpty()){
        setError(error,"Linked derived bodies are read-only; edit their source bodies instead.");return false;
    }
    // Component occurrence geometry is managed by placement and joint machinery.
    // Editing it here would desynchronize assembly placement frames.
    if(!part.componentDefinitionId.isEmpty()||!part.assemblyJointType.isEmpty()){
        setError(error,"Feature stacks cannot directly modify component occurrences. Edit the source part before making an assembly occurrence.");return false;
    }
    if(part.hasShapeFeatureTree&&!shapeFeatureTreeCurrent(partId)){
        setError(error,"This feature tree is stale after a direct model edit. Rebase explicitly before editing its nodes.");return false;
    }
    if(!validateShapeFeatureNodes(nodes,error))return false;
    // Same nodes are a true no-op: no costly geometry build, no undo entry.
    if(part.hasShapeFeatureTree&&nodes.size()==part.shapeFeatureNodes.size()){
        bool identical=true;
        for(int i=0;i<nodes.size();++i){
            const auto& a=nodes[i];const auto& b=part.shapeFeatureNodes[i];
            if(a.id!=b.id||a.type!=b.type||a.parameters!=b.parameters||
               a.suppressed!=b.suppressed||a.inputs!=b.inputs){identical=false;break;}
        }
        if(identical)return true;
    }
    const TopoDS_Shape source=part.hasShapeFeatureTree?part.shapeFeatureBase:part.shape;
    // The last committed feature stack is the authority for cache identity.
    // Reuse the contiguous unchanged prefix directly; the DAG planner below
    // may reuse additional independent branches beyond the first edit.
    // A failed rebuild must not alter the document or its saved cache.
    QVector<TopoDS_Shape> prefix;
    if(part.hasShapeFeatureTree&&shapeFeatureTreeCurrent(partId)&&
       part.shapeFeatureCachedOutputs.size()==part.shapeFeatureNodes.size()){
        for(int i=0;i<std::min(nodes.size(),part.shapeFeatureNodes.size());++i){
            const auto& a=nodes[i];const auto& b=part.shapeFeatureNodes[i];
            if(a.id!=b.id||a.type!=b.type||a.parameters!=b.parameters||
               a.suppressed!=b.suppressed||a.inputs!=b.inputs)break;
            prefix.append(part.shapeFeatureCachedOutputs[i]);
        }
    }
    // The cache belongs to the current committed geometry and has one output
    // per old feature. Beyond the unchanged prefix, reusable branches are
    // identified by stable IDs and their actual dependency chains. A changed
    // independent branch does not invalidate an unrelated parallel branch.
    QMap<QString,TopoDS_Shape> reusable;
    if(part.hasShapeFeatureTree&&shapeFeatureTreeCurrent(partId)&&
       part.shapeFeatureCachedOutputs.size()==part.shapeFeatureNodes.size()){
        QVector<bool> available;
        available.reserve(part.shapeFeatureCachedOutputs.size());
        for(const auto& shape:part.shapeFeatureCachedOutputs)available.append(!shape.IsNull());
        QVector<int> oldPositions;
        if(!planShapeFeatureReuse(part.shapeFeatureNodes,nodes,available,&oldPositions,error))return false;
        for(int i=prefix.size();i<nodes.size();++i){
            const int old=oldPositions[i];
            if(old>=0)reusable.insert(nodes[i].id,part.shapeFeatureCachedOutputs[old]);
        }
    }
    TopoDS_Shape rebuilt;
    QVector<TopoDS_Shape> intermediate;
    if(!rebuildShapeFeatureTree(source,part.isSurfaceBody,nodes,&rebuilt,error,
                                 &prefix,&intermediate,&reusable))return false;
    QVector<Part> candidate=parts_;
    Part& target=candidate[index];
    if(!target.hasShapeFeatureTree){
        target.shapeFeatureBase=source;
        target.hasShapeFeatureTree=true;
    }
    target.shape=rebuilt;
    target.shapeFeatureNodes=nodes;
    target.shapeFeatureCachedOutputs=std::move(intermediate);
    // A general tree may now consume an ordered external-sketch stack.
    // Unsupported direct geometry edits still invalidate upstream history.
    target.geometryToken=generatedId();
    target.shapeFeatureGeometryToken=target.geometryToken;
    if(target.hasConstrainedPolygonDefinition&&part.constrainedPolygonGeometryToken==part.geometryToken)
        target.constrainedPolygonGeometryToken=target.geometryToken;
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);
    return true;
}

bool SceneDocument::stageShapeFeatureTreesBatch(
        const QMap<QString,QVector<ShapeFeatureNode>>& proposals,
        SceneDocument* staged,bool* changed,QStringList* changedBodyIds,QString* error) const{
    if(!staged||!changed||!changedBodyIds||proposals.size()<2||proposals.size()>16){
        setError(error,"Batch feature staging requires 2-16 roots and valid destinations.");return false;
    }
    // Snapshot EVERYTHING needed by parametric and assembly dependency rebuild.
    // Do not copy undo/redo history or save a checkpoint into the live scene.
    staged->parts_=parts_;
    staged->liveRigidMateFixed_=liveRigidMateFixed_;
    staged->liveRigidMates_=liveRigidMates_;
    staged->liveArticulatedFixed_=liveArticulatedFixed_;
    staged->liveArticulatedJoints_=liveArticulatedJoints_;
    staged->liveArticulatedReferencePoses_=liveArticulatedReferencePoses_;
    *changed=false;
    changedBodyIds->clear();
    for(auto it=proposals.cbegin();it!=proposals.cend();++it){
        const Part* original=find(it.key());
        if(!original){setError(error,QStringLiteral("Batch feature source is missing: %1").arg(it.key()));return false;}
        QString issue;
        if(!staged->setShapeFeatureTree(it.key(),it.value(),&issue)){
            setError(error,QStringLiteral("Batch feature edit failed on %1: %2").arg(it.key(),issue));return false;
        }
        const Part* result=staged->find(it.key());
        if(!result){setError(error,"Batch feature staging lost a target body.");return false;}
        *changed=*changed||(result->geometryToken!=original->geometryToken);
    }
    // Postcondition: independently validate the staged revision journal.
    // No persisted scene member or undo checkpoint is changed on failure.
    std::vector<transaction::BodyRevisionEvidence> previous,proposed;
    previous.reserve(parts_.size());proposed.reserve(staged->parts_.size());
    std::vector<std::string> rootIds;
    for(auto it=proposals.cbegin();it!=proposals.cend();++it)
        rootIds.push_back(it.key().toStdString());
    for(const Part& part:parts_)
        previous.push_back({part.id.toStdString(),part.geometryToken.toStdString(),true});
    for(const Part& part:staged->parts_){
        const Part* original=find(part.id);
        proposed.push_back({part.id.toStdString(),part.geometryToken.toStdString(),
                            original&&!original->shape.IsNull()&&
                            !part.shape.IsNull()&&original->shape.IsSame(part.shape)});
    }
    std::vector<std::string> changedStamps;
    std::string invariantError;
    if(!transaction::validateRebuildCommit(previous,proposed,rootIds,
                                            &changedStamps,&invariantError)){
        setError(error,QStringLiteral("Atomic feature rebuild invariant: %1")
                            .arg(QString::fromStdString(invariantError)));
        return false;
    }
    for(const auto& id:changedStamps)
        changedBodyIds->append(QString::fromStdString(id));
    if(error)error->clear();
    return true;
}

bool SceneDocument::previewShapeFeatureTreesBatch(
        const QMap<QString,QVector<ShapeFeatureNode>>& proposals,
        QString* report,QString* error) const{
    if(!report){setError(error,"A destination is required for the detached feature preview.");return false;}
    SceneDocument staged;
    bool changed=false;QStringList changedIds;
    if(!stageShapeFeatureTreesBatch(proposals,&staged,&changed,&changedIds,error))return false;
    // This review is computed from the DETACHED document's real saved sheet
    // links. It detects downstream annotations made stale by the proposed
    // geometry revision, without acknowledging or changing any live drawing.
    QStringList newlyStaleSheets;
    QStringList newlyStaleAnnotations;
    for(const Part& item:staged.parts_){
        if(!item.hasDrawingSettings)continue;
        const DrawingSheetSourceStatus before=drawingSheetSourceStatus(item.id);
        const DrawingSheetSourceStatus after=staged.drawingSheetSourceStatus(item.id);
        if(before.state==DrawingSourceState::Current &&
           after.state!=DrawingSourceState::Current){
            newlyStaleSheets.append(item.id);
        }
        for(const QString& annotation:after.staleAnnotationIds){
            if(!before.staleAnnotationIds.contains(annotation))
                newlyStaleAnnotations.append(item.id+QStringLiteral(":")+annotation);
        }
    }
    newlyStaleSheets.sort();newlyStaleAnnotations.sort();
    const QString preview=QStringLiteral(
        "ATOMIC_FEATURE_PREVIEW\nroots=%1\nchanged=%2\nbody_ids=%3"
        "\nnewly_stale_sheets=%4\nnewly_stale_annotations=%5"
        "\nreview_sheet_ids=%6\nreview_annotation_ids=%7\nstatus=READ_ONLY")
        .arg(proposals.size()).arg(changedIds.size()).arg(changedIds.join(','))
        .arg(newlyStaleSheets.size()).arg(newlyStaleAnnotations.size())
        .arg(newlyStaleSheets.join(',')).arg(newlyStaleAnnotations.join(','));
    *report=preview;
    if(error)error->clear();
    return true;
}

bool SceneDocument::setShapeFeatureTreesBatch(
        const QMap<QString,QVector<ShapeFeatureNode>>& proposals,QString* error){
    SceneDocument staged;
    bool changed=false;QStringList changedIds;
    if(!stageShapeFeatureTreesBatch(proposals,&staged,&changed,&changedIds,error))return false;
    if(!changed){if(error)error->clear();return true;}
    checkpoint(); // Exactly one undo record after every root and descendant succeeds.
    parts_=std::move(staged.parts_);
    if(error)error->clear();
    return true;
}

namespace {
// Recreate a dependent extrusion from the CURRENT saved source sketch. Never
// use the source body's finished B-rep as a proxy for its sketch geometry.
bool rebuildLinkedSketchSolid(const Part& source,double depth,double offset,
                              TopoDS_Shape* output,QString* error){
    if(!output){if(error)*error="Missing linked-sketch output.";return false;}
    if(!source.hasConstrainedPolygonDefinition||source.isSurfaceBody||
       source.constrainedPolygonGeometryToken!=source.geometryToken){
        if(error)*error="Linked sketch source is missing or its stored sketch is stale.";
        return false;
    }
    if(!std::isfinite(depth)||depth<=1e-5||depth>1000000||
       !std::isfinite(offset)||std::abs(offset)>1000000){
        if(error)*error="Linked extrusion depth/offset must be finite, with positive depth and bounded offset.";
        return false;
    }
    SketchPlane plane=source.constrainedPolygonPlane;
    if(!validSketchPlane(plane,error))return false;
    const double normalLength=std::hypot(plane.normalX,std::hypot(plane.normalY,plane.normalZ));
    // Offset is a physical distance in mm, even for a non-unit saved normal.
    plane.originX+=(plane.normalX/normalLength)*offset;
    plane.originY+=(plane.normalY/normalLength)*offset;
    plane.originZ+=(plane.normalZ/normalLength)*offset;
    if(!validSketchPlane(plane,error))return false;
    return buildSketchExtrusion(source.constrainedPolygonSketch,
                                source.constrainedPolygonBoundary,depth,plane,output,error);
}
}

namespace {
// This helper is called from a non-member replay function. Mirror the
// SceneDocument solid validation without accessing its private static API.
bool validReplayShape(const TopoDS_Shape& shape,QString* error){
    if(shape.IsNull()){if(error)*error="The kernel returned a null shape.";return false;}
    if(!hasSolid(shape)){if(error)*error="A modeling operation must produce at least one solid.";return false;}
    if(!BRepCheck_Analyzer(shape).IsValid()){
        if(error)*error="Geometry failed OCCT topology validation.";return false;
    }
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape,props);
    if(!std::isfinite(props.Mass())||props.Mass()<=1e-12){
        if(error)*error="Solid has zero or invalid volume.";return false;
    }
    return true;
}
bool replayInterleavedHistory(const Part& part,const QVector<Part>& scene,
                              const QMap<QString,int>& indices,TopoDS_Shape* finalShape,
                              QStringList* sourceTokens,QString* error,
                              QMap<QString,TopoDS_Shape>* stageOutputs=nullptr){
    if(!finalShape||!part.hasInterleavedHistory||part.interleavedBase.IsNull()||
       part.isSurfaceBody||part.interleavedSteps.isEmpty()||part.interleavedSteps.size()>40){
        if(error)*error="Invalid interleaved history base or node count.";return false;
    }
    TopoDS_Shape previous=part.interleavedBase;
    if(!validReplayShape(previous,error))return false;
    QStringList tokens;
    QMap<QString,TopoDS_Shape> evaluated;
    evaluated.insert(QStringLiteral("@base"),previous);
    // Kernel builders are retained only for this rebuild. Their provenance
    // maps are invalid as persistent identifiers across source geometry edits.
    QMap<QString,std::shared_ptr<BRepAlgoAPI_BooleanOperation>> booleanHistory;
    QMap<QString,std::shared_ptr<BRepBuilderAPI_MakeShape>> modifierHistory;
    QMap<QString,QStringList> modifierSources;
    QMap<QString,QStringList> booleanSources;
    QMap<QString,QStringList> rigidSources;
    QMap<QString,ShapeFeatureNode> rigidOperations;
    QMap<QString,QStringList> patternSources;
    QMap<QString,ShapeFeatureNode> patternOperations;
    QMap<QString,QStringList> survivingModifierSources;
    QString previousStepId=QStringLiteral("@base");
    for(const auto& step:part.interleavedSteps){
        if(step.id.isEmpty()||step.id.size()>64||step.id==QStringLiteral("@base")||
           evaluated.contains(step.id)){
            if(error)*error="History step IDs must be nonempty, unique, and not @base.";return false;
        }
        const bool shape=step.kind==QStringLiteral("Shape");
        const bool combine=shape&&(step.feature.type==QStringLiteral("Fuse branches")||
                                   step.feature.type==QStringLiteral("Subtract branches")||
                                   step.feature.type==QStringLiteral("Intersect branches"));
        if(step.inputs.size()>(combine?2:1)||(combine&&step.inputs.size()!=2)||
           (!shape&&step.inputs.size()>1)||
           (shape&&(!step.sketchSourceId.isEmpty()||!step.feature.inputs.isEmpty()||step.feature.id!=step.id))){
            if(error)*error="Invalid mixed feature dependencies or shape step definition.";return false;
        }
        QSet<QString> distinct;
        for(const QString& inputId:step.inputs){
            if(!evaluated.contains(inputId)||distinct.contains(inputId)){
                if(error)*error="Mixed history has a missing, duplicate, or forward feature input.";return false;
            }
            distinct.insert(inputId);
        }
        const TopoDS_Shape input=step.inputs.isEmpty()?previous:evaluated.value(step.inputs.first());
        if(step.referencePatternOccurrence<0||step.referencePatternOccurrence>23||
           (step.referenceSourceId.isEmpty()&&
               (step.referencePatternOccurrence!=0||!step.referencePatternOccurrences.isEmpty()))||
           (step.referencePatternOccurrence!=0&&!step.referencePatternOccurrences.isEmpty())||
           step.referencePatternOccurrences.size()>40){
            if(error)*error="Invalid legacy or per-stage pattern occurrence selections.";
            return false;
        }
        for(auto it=step.referencePatternOccurrences.cbegin();
            it!=step.referencePatternOccurrences.cend();++it){
            if(it.key().isEmpty()||it.key().size()>64||it.value()<1||it.value()>23||
               !patternOperations.contains(it.key())){
                if(error)*error="Pattern occurrence must name an earlier pattern and a non-seed index [1,23].";
                return false;
            }
        }
        if(!shape&&!step.referenceSourceId.isEmpty()){
            if(error)*error="Only shape modifiers may carry a tracked reference source.";return false;
        }
        TopoDS_Shape next;
        if(shape){
            if(combine){
                const QString op=step.feature.type==QStringLiteral("Fuse branches")?QStringLiteral("Fuse"):
                    step.feature.type==QStringLiteral("Subtract branches")?QStringLiteral("Cut"):
                    QStringLiteral("Common");
                // Two-input combiners have no numeric parameters or suppression.
                if(!step.feature.parameters.isEmpty()||step.feature.suppressed){
                    if(error)*error="Mixed Boolean combines cannot have parameters or suppression.";
                    return false;
                }
                if(!step.referenceSourceId.isEmpty()){
                    if(error)*error="Boolean combines cannot carry feature-selection ancestry.";return false;
                }
                std::shared_ptr<BRepAlgoAPI_BooleanOperation> history;
                if(!linkedBooleanResult(input,evaluated.value(step.inputs[1]),op,&next,error,&history))return false;
                if(op==QStringLiteral("Cut")&&!linkedSketchCutEffective(input,next,error))return false;
                booleanHistory.insert(step.id,std::move(history));
                booleanSources.insert(step.id,step.inputs);
            }else{
                ShapeFeatureNode resolved=step.feature;
                if(!step.referenceSourceId.isEmpty()){
                    if(step.inputs.size()!=1||!evaluated.contains(step.referenceSourceId)){
                        if(error)*error="A referenced modifier requires an existing source and an explicit tracked parent.";
                        return false;
                    }
                    // Follow every supported ancestry route using current
                    // kernel Booleans, rigid transforms, selected pattern copies,
                    // or exact unchanged subshape identity through edge finishing.
                    // Unsupported affine/procedural topology replacements remain boundaries;
                    // a merge is accepted only when all routes agree.
                    std::map<std::string,std::vector<std::string>> ancestry;
                    const auto addParents=[&](const QMap<QString,QStringList>& paths){
                        for(auto it=paths.cbegin();it!=paths.cend();++it){
                            std::vector<std::string> sources;
                            for(const QString& parentId:it.value())sources.push_back(parentId.toStdString());
                            ancestry.emplace(it.key().toStdString(),std::move(sources));
                        }
                    };
                    addParents(booleanSources);
                    addParents(rigidSources);
                    addParents(patternSources);
                    addParents(survivingModifierSources);
                    addParents(modifierSources);
                    std::vector<std::vector<std::string>> routes;
                    std::string why;
                    const bool directStage=step.referenceSourceId==step.inputs.first();
                    if(directStage){
                        // A reference captured on an intermediate result is
                        // witnessed against that result on EVERY fresh rebuild.
                        // There is no earlier source and no invented ancestry.
                        routes.emplace_back();
                    }else if(!lineage::allTrackedPaths(ancestry,
                        step.referenceSourceId.toStdString(),step.inputs.first().toStdString(),
                        &routes,&why)){
                        if(error)*error=QString::fromStdString(why);return false;
                    }
                    // A Boolean merge may turn two DIFFERENT design points into
                    // the same final edge descriptor. Keep the original point
                    // independently of every re-captured topology anchor.
                    gp_Pnt originalWitness;
                    const auto& saved=step.feature.parameters;
                    const auto getWitness=[&](const QString& prefix){
                        if(!saved.contains(prefix+"x")||!saved.contains(prefix+"y")||
                           !saved.contains(prefix+"z"))return false;
                        originalWitness=gp_Pnt(saved.value(prefix+"x"),
                                               saved.value(prefix+"y"),
                                               saved.value(prefix+"z"));
                        return std::isfinite(originalWitness.X())&&
                               std::isfinite(originalWitness.Y())&&
                               std::isfinite(originalWitness.Z());
                    };
                    const bool hasWitness=getWitness(QStringLiteral("edge_witness_"))||
                                          getWitness(QStringLiteral("face_anchor_witness_"))||
                                          getWitness(QStringLiteral("curved_witness_"));
                    if(routes.size()>1&&!hasWitness){
                        if(error)*error="Merged ancestry requires a saved physical design witness on every route.";
                        return false;
                    }
                    ShapeFeatureNode referenceResult;
                    bool haveResult=false;
                    std::vector<lineage::ReferenceRouteEvidence> routeEvidence;
                    routeEvidence.reserve(routes.size());
                    int routeNumber=0;
                    for(const auto& chain:routes){
                        ++routeNumber;
                        gp_Pnt pathWitness=originalWitness;
                        QString ancestor=step.referenceSourceId;
                        ShapeFeatureNode candidate=resolved;
                        if(directStage){
                            ShapeFeatureNode refreshed;
                            if(!rebaseFeatureOnCurrentStage(evaluated.value(ancestor),
                                candidate,&refreshed,error))return false;
                            candidate=std::move(refreshed);
                        }
                        int patternHops=0;
                        QSet<QString> selectedPatternStages;
                        for(const std::string& stage:chain){
                            const QString descendant=QString::fromStdString(stage);
                            if(!evaluated.contains(ancestor)||!evaluated.contains(descendant)){
                                if(error)*error="Reference lineage contains an unavailable rebuild stage.";
                                return false;
                            }
                            ShapeFeatureNode rebound;
                            if(booleanHistory.contains(descendant)){
                                if(!rebaseFeatureThroughBoolean(evaluated.value(ancestor),evaluated.value(descendant),
                                    *booleanHistory.value(descendant),candidate,&rebound,error))return false;
                            }else if(rigidOperations.contains(descendant)){
                                if(hasWitness&&!transportDesignPointThroughSimilarity(
                                    rigidOperations.value(descendant),&pathWitness,error))return false;
                                if(!rebaseFeatureThroughSimilarity(evaluated.value(ancestor),evaluated.value(descendant),
                                    rigidOperations.value(descendant),candidate,&rebound,error))return false;
                            }else if(patternOperations.contains(descendant)){
                                ++patternHops;
                                selectedPatternStages.insert(descendant);
                                if(step.referencePatternOccurrence!=0&&patternHops>1){
                                    if(error)*error="Legacy v4 occurrence cannot cross nested patterns; edit and save using v5 per-stage selections.";
                                    return false;
                                }
                                const int occurrence=step.referencePatternOccurrences.isEmpty()?
                                    step.referencePatternOccurrence:
                                    step.referencePatternOccurrences.value(descendant,0);
                                if(hasWitness&&!transportDesignPointThroughPattern(
                                    patternOperations.value(descendant),occurrence,
                                    &pathWitness,error))return false;
                                if(!rebaseFeatureThroughPattern(evaluated.value(ancestor),evaluated.value(descendant),
                                    patternOperations.value(descendant),occurrence,
                                    candidate,&rebound,error))return false;
                            }else if(modifierHistory.contains(descendant)){
                                // Fillets/chamfers can retain an EXACT original
                                // subshape even when the OCCT builder's history
                                // reports IsDeleted() or non-unique Modified().
                                // Prefer that proven identity before consulting
                                // generated/modified successors. Both paths
                                // enforce the saved geometric witness; neither
                                // guesses a new reference by edge number.
                                QString exactError;
                                const bool exactSurvivor=
                                    survivingModifierSources.contains(descendant)&&
                                    rebaseFeatureThroughSurvivingModifier(
                                        evaluated.value(ancestor),evaluated.value(descendant),
                                        candidate,&rebound,&exactError);
                                if(!exactSurvivor){
                                    QString kernelError;
                                    if(!rebaseFeatureThroughBoolean(evaluated.value(ancestor),
                                        evaluated.value(descendant),*modifierHistory.value(descendant),
                                        candidate,&rebound,&kernelError)){
                                        if(error)*error=exactError.isEmpty()?kernelError:
                                            QStringLiteral("Exact modifier lineage: %1; kernel lineage: %2")
                                                .arg(exactError,kernelError);
                                        return false;
                                    }
                                }
                            }else if(survivingModifierSources.contains(descendant)){
                                if(!rebaseFeatureThroughSurvivingModifier(evaluated.value(ancestor),
                                    evaluated.value(descendant),candidate,&rebound,error))return false;
                            }else{
                                if(error)*error="Reference path passes through an unsupported feature operation.";
                                return false;
                            }
                            candidate=std::move(rebound);
                            ancestor=descendant;
                        }
                        if(step.referencePatternOccurrence!=0&&patternHops!=1){
                            if(error)*error="Legacy selected occurrence requires exactly one tracked pattern stage.";
                            return false;
                        }
                        for(auto it=step.referencePatternOccurrences.cbegin();
                            it!=step.referencePatternOccurrences.cend();++it){
                            if(!selectedPatternStages.contains(it.key())){
                                if(error)*error="A selected pattern occurrence is absent from an ancestry route.";
                                return false;
                            }
                        }
                        lineage::ReferenceRouteEvidence evidence;
                        evidence.witness={pathWitness.X(),pathWitness.Y(),pathWitness.Z()};
                        for(auto it=candidate.parameters.cbegin();it!=candidate.parameters.cend();++it)
                            evidence.descriptor.emplace(it.key().toStdString(),it.value());
                        routeEvidence.push_back(std::move(evidence));
                        // Fail on the first disagreement, BEFORE any shape
                        // operation uses the unresolved edge or face. The
                        // complete saved design witness remains immutable.
                        std::string disagreement;
                        if(!lineage::agreeReferenceRoutes(routeEvidence,&disagreement)){
                            if(error)*error=QStringLiteral("Persistent Boolean ancestry conflict at '%1' (route %2): %3")
                                .arg(step.inputs.first()).arg(routeNumber)
                                .arg(QString::fromStdString(disagreement));
                            return false;
                        }
                        if(!haveResult){
                            referenceResult=candidate;
                            haveResult=true;
                        }
                    }
                    resolved=std::move(referenceResult);
                }
                // `resolved` is a temporary, CURRENT-rebuild descriptor. A tracked
                // edge has already crossed every ancestry route, with its source
                // witness checked and its successor converted to a unique edge
                // index in `input`. Do not run a SECOND geometric witness search
                // inside the one-node feature builder: a fillet may change the
                // body's mass centre and cause anchor compensation to reject an
                // otherwise exact, current TShape (or select a different edge).
                //
                // Drop the witness only from the ephemeral execution copy. Keep
                // the original witnessed feature in `part.interleavedSteps` for
                // persistence, source edits, and subsequent full rebuilds.
                ShapeFeatureNode executable=resolved;
                const bool trackedEdge=!step.referenceSourceId.isEmpty()&&
                    (resolved.type==QStringLiteral("Fillet selected edge")||
                     resolved.type==QStringLiteral("Chamfer selected edge"));
                if(trackedEdge){
                    if(!resolved.parameters.contains(QStringLiteral("edge_anchor_version"))||
                       !resolved.parameters.contains(QStringLiteral("edge_index"))){
                        if(error)*error="Tracked edge is missing its verified current-stage identity.";
                        return false;
                    }
                    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> inputEdges;
                    TopExp::MapShapes(input,TopAbs_EDGE,inputEdges);
                    const double indexValue=resolved.parameters.value(QStringLiteral("edge_index"));
                    if(!std::isfinite(indexValue)||std::floor(indexValue)!=indexValue||
                       indexValue<1||indexValue>inputEdges.Extent()){
                        if(error)*error="Tracked edge index is outside the current stage.";
                        return false;
                    }
                    for(auto it=executable.parameters.begin();it!=executable.parameters.end();){
                        if(it.key().startsWith(QStringLiteral("edge_"))&&
                           it.key()!=QStringLiteral("edge_index"))
                            it=executable.parameters.erase(it);
                        else ++it;
                    }
                }
                std::shared_ptr<BRepBuilderAPI_MakeShape> recordedModifier;
                if(!rebuildShapeFeatureTree(input,false,{executable},&next,error,
                    nullptr,nullptr,nullptr,&recordedModifier)){
                    if(error)*error=QStringLiteral("Interleaved operation '%1' (%2) failed: %3")
                        .arg(step.id).arg(step.feature.type).arg(*error);
                    return false;
                }
                if(supportsRecordedModifierLineage(step.feature)&&recordedModifier){
                    modifierHistory.insert(step.id,std::move(recordedModifier));
                    modifierSources.insert(step.id,
                        {step.inputs.isEmpty()?previousStepId:step.inputs.first()});
                }
                // Rigid and positive uniform scale stages preserve a one-to-one image of all original
                // subshapes; save the exact operation and its actual source for
                // later feature reference walks. Modifiers and patterns cannot
                // be treated as implicit rigid ancestry.
                if(step.referenceSourceId.isEmpty()&&supportsSimilarityReferenceLineage(step.feature)){
                    rigidOperations.insert(step.id,step.feature);
                    rigidSources.insert(step.id,{step.inputs.isEmpty()?previousStepId:step.inputs.first()});
                }
                if(step.referenceSourceId.isEmpty()&&supportsPatternReferenceLineage(step.feature)){
                    patternOperations.insert(step.id,step.feature);
                    patternSources.insert(step.id,{step.inputs.isEmpty()?previousStepId:step.inputs.first()});
                }
                // Register exact-identity ancestry for *all* fillets/chamfers,
                // including ones with recorded kernel history. The two
                // resolvers are complementary: exact survivors first, then
                // valid kernel-confirmed descendants where appropriate.
                if(supportsSurvivingModifierLineage(step.feature)){
                    survivingModifierSources.insert(step.id,
                        {step.inputs.isEmpty()?previousStepId:step.inputs.first()});
                }
            }
            tokens.append(QString());
        }else if(step.kind==QStringLiteral("SketchFuse")||step.kind==QStringLiteral("SketchCut")){
            const int sourceIndex=indices.value(step.sketchSourceId,-1);
            if(sourceIndex<0||scene[sourceIndex].id==part.id||
               !std::isfinite(step.depthMm)||step.depthMm<=1e-5||step.depthMm>1e6||
               !std::isfinite(step.offsetMm)||std::abs(step.offsetMm)>1e6){
                if(error)*error="Interleaved sketch source or its dimensions are invalid.";return false;
            }
            const Part& source=scene[sourceIndex];
            TopoDS_Shape cutter;
            const QString op=step.kind==QStringLiteral("SketchFuse")?
                QStringLiteral("Fuse"):QStringLiteral("Cut");
            std::shared_ptr<BRepAlgoAPI_BooleanOperation> history;
            if(!rebuildLinkedSketchSolid(source,step.depthMm,step.offsetMm,&cutter,error)||
               !linkedBooleanResult(input,cutter,op,&next,error,&history))return false;
            const QString parent=step.inputs.isEmpty()?previousStepId:step.inputs.first();
            booleanSources.insert(step.id,{parent});
            booleanHistory.insert(step.id,std::move(history));
            if(op==QStringLiteral("Fuse")&&!linkedSketchFuseEffective(input,next,error))return false;
            if(op==QStringLiteral("Cut")&&!linkedSketchCutEffective(input,next,error))return false;
            tokens.append(source.geometryToken);
        }else{
            if(error)*error="Unknown interleaved feature history step.";return false;
        }
        if(!validReplayShape(next,error))return false;
        evaluated.insert(step.id,next);
        previous=next;
        previousStepId=step.id;
    }
    *finalShape=previous;
    if(sourceTokens)*sourceTokens=std::move(tokens);
    if(stageOutputs)*stageOutputs=std::move(evaluated);
    return true;
}
}


// Evaluate a staged prefix for authoring. This is intentionally read-only:
// it neither checkpoints nor mutates the displayed geometry or source tokens.
bool SceneDocument::previewInterleavedFeatureHistory(const QString& id,
        const QVector<InterleavedHistoryStep>& steps,
        QMap<QString,TopoDS_Shape>* stageShapes,QString* error) const{
    const Part* original=find(id);
    if(!stageShapes||!original||original->isSurfaceBody||steps.isEmpty()||steps.size()>40||
       (original->hasInterleavedHistory&&original->interleavedBase.IsNull())){
        setError(error,"Preview requires a solid with a valid frozen base and 1-40 staged steps.");
        return false;
    }
    Part staged=*original;
    if(!staged.hasInterleavedHistory)staged.interleavedBase=original->shape;
    staged.hasInterleavedHistory=true;
    staged.interleavedSteps=steps;
    QMap<QString,int> indices;
    for(int i=0;i<parts_.size();++i)indices.insert(parts_[i].id,i);
    QMap<QString,TopoDS_Shape> preview;
    TopoDS_Shape result;
    if(!replayInterleavedHistory(staged,parts_,indices,&result,nullptr,error,&preview))return false;
    *stageShapes=std::move(preview);
    return true;
}

// Apply a persisted sketch-derived modifier to the SAME target body's base B-rep.
// First stage of a bounded, ordered in-place sketch stack. Later stages are
// persisted and regenerated through the same multi-source dependency DAG.
bool SceneDocument::setInPlaceSketchFeature(const QString& targetId,const QString& sourceId,
                                            BooleanOp op,double depth,double offset,QString* error){
    const Part* original=find(targetId);
    const Part* source=find(sourceId);
    if(!original||!source||targetId==sourceId||original->isSurfaceBody||
       original->hasLinkedBoolean||original->hasLinkedSketchExtrusion||
       !original->linkedFoldedFlatSourceId.isEmpty()||
       (original->hasShapeFeatureTree&&!original->hasInPlaceSketchFeature)||
       original->hasConstrainedPolygonDefinition||
       !original->componentDefinitionId.isEmpty()||!original->assemblyJointType.isEmpty()||
       !source->hasConstrainedPolygonDefinition||source->constrainedPolygonGeometryToken!=source->geometryToken){
        setError(error,"Select a separate current sketch source and an ordinary, solid target without an existing feature stack.");return false;
    }
    if((original->hasInPlaceSketchFollowUp&&original->inPlaceFollowSourceId==sourceId)||
       std::any_of(original->extraInPlaceSketchStages.begin(),original->extraInPlaceSketchStages.end(),
                   [&](const auto& stage){return stage.sourceId==sourceId;})){
        setError(error,"A sketch source cannot appear twice in the same in-place stack.");return false;
    }
    if(op!=BooleanOp::Fuse&&op!=BooleanOp::Cut){
        setError(error,"In-place sketch operation must be additive or subtractive.");return false;
    }
    if(!std::isfinite(depth)||depth<=1e-5||depth>1e6||!std::isfinite(offset)||std::abs(offset)>1e6){
        setError(error,"In-place sketch depth/offset is invalid.");return false;
    }
    QVector<Part> candidate=parts_;
    for(Part& part:candidate)if(part.id==targetId){
        if(!part.hasInPlaceSketchFeature){part.inPlaceSketchBase=part.shape;part.hasInPlaceSketchFeature=true;}
        part.inPlaceSketchSourceId=sourceId;
        part.inPlaceSketchSourceToken.clear(); // Force rebuild even on parameter-only edits.
        part.inPlaceSketchOperation=op==BooleanOp::Fuse?QStringLiteral("Fuse"):QStringLiteral("Cut");
        part.inPlaceSketchDepthMm=depth;part.inPlaceSketchOffsetMm=offset;
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::appendInPlaceSketchFeature(const QString& targetId,const QString& sourceId,
                                               BooleanOp op,double depth,double offset,QString* error){
    const Part* previous=find(targetId);
    const Part* source=find(sourceId);
    if(!previous||!previous->hasInPlaceSketchFeature||previous->hasInPlaceSketchFollowUp||
       !source||sourceId==targetId||sourceId==previous->inPlaceSketchSourceId||
       !source->hasConstrainedPolygonDefinition||
       source->constrainedPolygonGeometryToken!=source->geometryToken){
        setError(error,"Select an existing one-stage in-place target and a different current sketch source.");return false;
    }
    if((op!=BooleanOp::Fuse&&op!=BooleanOp::Cut)||
       !std::isfinite(depth)||depth<=1e-5||depth>1e6||
       !std::isfinite(offset)||std::abs(offset)>1e6){
        setError(error,"Invalid follow-up in-place sketch operation or dimensions.");return false;
    }
    QVector<Part> candidate=parts_;
    for(Part& part:candidate)if(part.id==targetId){
        part.hasInPlaceSketchFollowUp=true;
        part.inPlaceFollowSourceId=sourceId;
        part.inPlaceFollowSourceToken.clear();
        part.inPlaceFollowOperation=op==BooleanOp::Fuse?QStringLiteral("Fuse"):QStringLiteral("Cut");
        part.inPlaceFollowDepthMm=depth;
        part.inPlaceFollowOffsetMm=offset;
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::updateInPlaceSketchFollowUp(const QString& targetId,const QString& sourceId,
                                                BooleanOp op,double depth,double offset,QString* error){
    const Part* previous=find(targetId);
    const Part* source=find(sourceId);
    if(!previous||!previous->hasInPlaceSketchFollowUp||!source||
       sourceId==targetId||sourceId==previous->inPlaceSketchSourceId||
       std::any_of(previous->extraInPlaceSketchStages.begin(),previous->extraInPlaceSketchStages.end(),
                   [&](const auto& stage){return stage.sourceId==sourceId;})||
       !source->hasConstrainedPolygonDefinition||
       source->constrainedPolygonGeometryToken!=source->geometryToken){
        setError(error,"Select a two-stage target and a distinct current sketch source.");return false;
    }
    if((op!=BooleanOp::Fuse&&op!=BooleanOp::Cut)||
       !std::isfinite(depth)||depth<=1e-5||depth>1e6||
       !std::isfinite(offset)||std::abs(offset)>1e6){
        setError(error,"Invalid second-stage operation or dimensions.");return false;
    }
    QVector<Part> candidate=parts_;
    for(Part& part:candidate)if(part.id==targetId){
        part.inPlaceFollowSourceId=sourceId;
        part.inPlaceFollowSourceToken.clear();
        part.inPlaceFollowOperation=op==BooleanOp::Fuse?QStringLiteral("Fuse"):QStringLiteral("Cut");
        part.inPlaceFollowDepthMm=depth;part.inPlaceFollowOffsetMm=offset;
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::removeInPlaceSketchFollowUp(const QString& targetId,QString* error){
    const Part* existing=find(targetId);
    if(!existing||!existing->hasInPlaceSketchFollowUp||!existing->extraInPlaceSketchStages.isEmpty()){
        setError(error,"Remove additional stages in reverse order before deleting stage two.");return false;
    }
    QVector<Part> candidate=parts_;
    for(Part& part:candidate)if(part.id==targetId){
        part.hasInPlaceSketchFollowUp=false;
        part.inPlaceFollowSourceId.clear();part.inPlaceFollowSourceToken.clear();
        part.inPlaceFollowOperation.clear();
        part.inPlaceFollowDepthMm=0;part.inPlaceFollowOffsetMm=0;
        part.inPlaceSketchSourceToken.clear(); // Rebuild first stage, then descendants.
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::appendInPlaceSketchStage(const QString& targetId,const QString& sourceId,
                                               BooleanOp op,double depth,double offset,QString* error){
    const Part* previous=find(targetId);
    const Part* source=find(sourceId);
    if(!previous||!previous->hasInPlaceSketchFollowUp||
       previous->extraInPlaceSketchStages.size()>=kMaxInPlaceSketchStages-2||
       !source||!source->hasConstrainedPolygonDefinition||
       source->constrainedPolygonGeometryToken!=source->geometryToken||
       sourceId==targetId||sourceId==previous->inPlaceSketchSourceId||
       sourceId==previous->inPlaceFollowSourceId||
       std::any_of(previous->extraInPlaceSketchStages.begin(),previous->extraInPlaceSketchStages.end(),
                   [&](const auto& stage){return stage.sourceId==sourceId;})){
        setError(error,"Requires two stages, a new current sketch source, and a maximum of eight stages.");return false;
    }
    if((op!=BooleanOp::Fuse&&op!=BooleanOp::Cut)||!std::isfinite(depth)||
       depth<=1e-5||depth>1e6||!std::isfinite(offset)||std::abs(offset)>1e6){
        setError(error,"Invalid additional in-place sketch operation or dimensions.");return false;
    }
    QVector<Part> candidate=parts_;
    for(Part& part:candidate)if(part.id==targetId){
        Part::AdditionalInPlaceSketchStage stage;
        stage.sourceId=sourceId;
        stage.operation=op==BooleanOp::Fuse?QStringLiteral("Fuse"):QStringLiteral("Cut");
        stage.depthMm=depth;stage.offsetMm=offset;
        part.extraInPlaceSketchStages.append(std::move(stage));
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::updateInPlaceSketchStage(const QString& targetId,int additionalIndex,
                                               const QString& sourceId,BooleanOp op,
                                               double depth,double offset,QString* error){
    const Part* previous=find(targetId);
    const Part* source=find(sourceId);
    if(!previous||additionalIndex<0||additionalIndex>=previous->extraInPlaceSketchStages.size()||
       !source||sourceId==targetId||!source->hasConstrainedPolygonDefinition||
       source->constrainedPolygonGeometryToken!=source->geometryToken||
       sourceId==previous->inPlaceSketchSourceId||sourceId==previous->inPlaceFollowSourceId){
        setError(error,"Invalid additional modifier index or sketch source.");return false;
    }
    for(int i=0;i<previous->extraInPlaceSketchStages.size();++i)
        if(i!=additionalIndex&&previous->extraInPlaceSketchStages[i].sourceId==sourceId){
            setError(error,"Duplicate sketch source in ordered stack.");return false;
        }
    if((op!=BooleanOp::Fuse&&op!=BooleanOp::Cut)||!std::isfinite(depth)||
       depth<=1e-5||depth>1e6||!std::isfinite(offset)||std::abs(offset)>1e6){
        setError(error,"Invalid edited modifier dimensions.");return false;
    }
    QVector<Part> candidate=parts_;
    for(Part& part:candidate)if(part.id==targetId){
        auto& stage=part.extraInPlaceSketchStages[additionalIndex];
        stage.sourceId=sourceId;stage.sourceToken.clear();
        stage.operation=op==BooleanOp::Fuse?QStringLiteral("Fuse"):QStringLiteral("Cut");
        stage.depthMm=depth;stage.offsetMm=offset;
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::removeLastInPlaceSketchStage(const QString& targetId,QString* error){
    const Part* previous=find(targetId);
    if(!previous||previous->extraInPlaceSketchStages.isEmpty()){
        setError(error,"No additional sketch modifier remains to remove.");return false;
    }
    QVector<Part> candidate=parts_;
    for(Part& part:candidate)if(part.id==targetId){
        part.extraInPlaceSketchStages.removeLast();
        part.inPlaceSketchSourceToken.clear(); // Force recomputation from frozen base.
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::removeInPlaceSketchFeature(const QString& targetId,QString* error){
    const Part* previous=find(targetId);
    if(!previous||!previous->hasInPlaceSketchFeature){
        setError(error,"Selected body has no in-place sketch feature.");return false;
    }
    QVector<Part> candidate=parts_;
    for(Part& part:candidate)if(part.id==targetId){
        const TopoDS_Shape original=part.inPlaceSketchBase;
        if(part.hasShapeFeatureTree){
            TopoDS_Shape rebuilt;QVector<TopoDS_Shape> outputs;
            if(!rebuildShapeFeatureTree(original,part.isSurfaceBody,
                part.shapeFeatureNodes,&rebuilt,error,nullptr,&outputs))return false;
            part.shapeFeatureBase=original;
            part.shapeFeatureCachedOutputs=std::move(outputs);
            part.shape=rebuilt;
        }else part.shape=original;
        part.geometryToken=generatedId();
        if(part.hasShapeFeatureTree)part.shapeFeatureGeometryToken=part.geometryToken;
        part.hasInPlaceSketchFeature=false;
        part.inPlaceSketchSourceId.clear();part.inPlaceSketchSourceToken.clear();
        part.inPlaceSketchOperation.clear();part.inPlaceSketchBase.Nullify();
        part.hasInPlaceSketchFollowUp=false;
        part.inPlaceFollowSourceId.clear();part.inPlaceFollowSourceToken.clear();
        part.inPlaceFollowOperation.clear();
        part.inPlaceFollowDepthMm=0;part.inPlaceFollowOffsetMm=0;
        part.inPlaceSketchDepthMm=0.0;part.inPlaceSketchOffsetMm=0.0;
        part.extraInPlaceSketchStages.clear();
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::inPlaceSketchFeatureCurrent(const QString& targetId) const{
    const Part* body=find(targetId);
    if(!body||!body->hasInPlaceSketchFeature)return false;
    const Part* source=find(body->inPlaceSketchSourceId);
    return source&&source->hasConstrainedPolygonDefinition&&
           source->constrainedPolygonGeometryToken==source->geometryToken&&
           body->inPlaceSketchSourceToken==source->geometryToken&&
           (!body->hasInPlaceSketchFollowUp||([&]{
               const Part* follow=find(body->inPlaceFollowSourceId);
               return follow&&follow->hasConstrainedPolygonDefinition&&
                      follow->constrainedPolygonGeometryToken==follow->geometryToken&&
                      body->inPlaceFollowSourceToken==follow->geometryToken;
           })())&&
           std::all_of(body->extraInPlaceSketchStages.begin(),body->extraInPlaceSketchStages.end(),
                       [&](const auto& stage){
                           const Part* sourcePart=find(stage.sourceId);
                           return sourcePart&&sourcePart->hasConstrainedPolygonDefinition&&
                                  sourcePart->constrainedPolygonGeometryToken==sourcePart->geometryToken&&
                                  stage.sourceToken==sourcePart->geometryToken;
                       });
}

bool SceneDocument::addLinkedSketchExtrusion(const QString& sourceId,double depth,double offset,QString* error){
    const Part* source=find(sourceId);
    if(!source){setError(error,"Select a source body with an editable constrained sketch.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    TopoDS_Shape result;
    if(!rebuildLinkedSketchSolid(*source,depth,offset,&result,error))return false;
    if(!validShape(result,error))return false;
    QVector<Part> candidate=parts_;
    Part dependent{generatedId(),uniqueName(QStringLiteral("Linked Sketch Extrusion")),result};
    dependent.hasLinkedSketchExtrusion=true;
    dependent.linkedSketchSourceId=sourceId;
    dependent.linkedSketchSourceToken=source->geometryToken;
    dependent.linkedSketchDepthMm=depth;
    dependent.linkedSketchOffsetMm=offset;
    candidate.append(std::move(dependent));
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}

bool SceneDocument::updateLinkedSketchExtrusion(const QString& id,double depth,double offset,QString* error){
    const Part* linked=find(id);
    if(!linked||!linked->hasLinkedSketchExtrusion){
        setError(error,"Select a linked sketch extrusion result.");return false;
    }
    return retargetLinkedSketchExtrusion(id,linked->linkedSketchSourceId,depth,offset,error);
}

bool SceneDocument::retargetLinkedSketchExtrusion(const QString& id,const QString& sourceId,
                                                   double depth,double offset,QString* error){
    int index=-1;
    for(int i=0;i<parts_.size();++i)if(parts_[i].id==id){index=i;break;}
    if(index<0||!parts_[index].hasLinkedSketchExtrusion){
        setError(error,"Select an existing sketch-derived solid to retarget.");return false;
    }
    const Part* source=find(sourceId);
    if(!source||source->isSurfaceBody||!source->hasConstrainedPolygonDefinition||
       source->constrainedPolygonGeometryToken!=source->geometryToken){
        setError(error,"New sketch source must be an existing body with a current constrained sketch.");
        return false;
    }
    // Reject bad parameters even on edits where only the source is changing.
    if(!std::isfinite(depth)||depth<=1e-5||depth>1000000||
       !std::isfinite(offset)||std::abs(offset)>1000000){
        setError(error,"Linked extrusion depth/offset is out of range.");return false;
    }
    const Part& current=parts_[index];
    if(current.linkedSketchSourceId==sourceId&&current.linkedSketchDepthMm==depth&&
       current.linkedSketchOffsetMm==offset)return true;
    QVector<Part> candidate=parts_;
    Part& changed=candidate[index];
    changed.linkedSketchSourceId=sourceId;
    changed.linkedSketchDepthMm=depth;
    changed.linkedSketchOffsetMm=offset;
    changed.linkedSketchSourceToken.clear(); // forces a new B-rep and descendant rebuild
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}

bool SceneDocument::addLinkedSketchPocket(const QString& sketchId,const QString& targetId,
                                           double depth,double offset,QString* error){
    if(parts_.size()>kMaxObjects-2){
        setError(error,"Two free scene-object slots are required for a linked pocket.");return false;
    }
    const Part* source=find(sketchId);
    const Part* target=find(targetId);
    if(!source||!target||target->isSurfaceBody){
        setError(error,"Choose an existing constrained-sketch source and a solid target body.");return false;
    }
    if((target->hasLinkedBoolean&&!linkedBooleanCurrent(targetId))||
       (target->hasLinkedSketchExtrusion&&!linkedSketchExtrusionCurrent(targetId))){
        setError(error,"Refresh the target body's stale dependencies before making a pocket.");return false;
    }
    TopoDS_Shape cutterShape;
    if(!rebuildLinkedSketchSolid(*source,depth,offset,&cutterShape,error)||
       !validShape(cutterShape,error))return false;
    TopoDS_Shape pocketShape;
    if(!linkedBooleanResult(target->shape,cutterShape,QStringLiteral("Cut"),&pocketShape,error))
        return false;
    if(!linkedSketchCutEffective(target->shape,pocketShape,error))return false;
    QVector<Part> candidate=parts_;
    Part cutter{generatedId(),uniqueName(QStringLiteral("Linked Sketch Pocket Tool")),cutterShape};
    cutter.visible=false;
    cutter.hasLinkedSketchExtrusion=true;
    cutter.linkedSketchSourceId=sketchId;
    cutter.linkedSketchSourceToken=source->geometryToken;
    cutter.linkedSketchDepthMm=depth;
    cutter.linkedSketchOffsetMm=offset;
    const QString cutterId=cutter.id;
    candidate.append(std::move(cutter));
    Part result{generatedId(),uniqueName(QStringLiteral("Linked Sketch Pocket")),
                pocketShape,true,target->color};
    result.hasLinkedBoolean=true;
    result.linkedOperation=QStringLiteral("Cut");
    result.linkedPrimaryId=targetId;
    result.linkedToolId=cutterId;
    result.linkedPrimaryToken=target->geometryToken;
    result.linkedToolToken=candidate.last().geometryToken;
    candidate.append(std::move(result));
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}

bool SceneDocument::addLinkedSketchBoss(const QString& sketchId,const QString& targetId,
                                         double depth,double offset,QString* error){
    if(parts_.size()>kMaxObjects-2){
        setError(error,"Two free scene-object slots are required for an associative boss.");return false;
    }
    const Part* source=find(sketchId);
    const Part* target=find(targetId);
    if(!source||!target||target->isSurfaceBody){
        setError(error,"Choose an existing constrained-sketch source and a solid target body.");return false;
    }
    if((target->hasLinkedBoolean&&!linkedBooleanCurrent(targetId))||
       (target->hasLinkedSketchExtrusion&&!linkedSketchExtrusionCurrent(targetId))){
        setError(error,"Refresh the target body's stale dependencies before making a boss.");return false;
    }
    TopoDS_Shape toolShape;
    if(!rebuildLinkedSketchSolid(*source,depth,offset,&toolShape,error)||
       !validShape(toolShape,error))return false;
    TopoDS_Shape fusedShape;
    if(!linkedBooleanResult(target->shape,toolShape,QStringLiteral("Fuse"),&fusedShape,error)||
       !linkedSketchFuseEffective(target->shape,fusedShape,error))return false;
    QVector<Part> candidate=parts_;
    Part tool{generatedId(),uniqueName(QStringLiteral("Linked Sketch Boss Tool")),toolShape};
    tool.visible=false;
    tool.hasLinkedSketchExtrusion=true;
    tool.linkedSketchSourceId=sketchId;
    tool.linkedSketchSourceToken=source->geometryToken;
    tool.linkedSketchDepthMm=depth;
    tool.linkedSketchOffsetMm=offset;
    const QString toolId=tool.id;
    candidate.append(std::move(tool));
    Part result{generatedId(),uniqueName(QStringLiteral("Linked Sketch Boss")),
                fusedShape,true,target->color};
    result.hasLinkedBoolean=true;
    result.linkedOperation=QStringLiteral("Fuse");
    result.linkedPrimaryId=targetId;
    result.linkedToolId=toolId;
    result.linkedPrimaryToken=target->geometryToken;
    result.linkedToolToken=candidate.last().geometryToken;
    candidate.append(std::move(result));
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);return true;
}

bool SceneDocument::linkedSketchExtrusionCurrent(const QString& id) const{
    const Part* part=find(id);
    if(!part||!part->hasLinkedSketchExtrusion)return false;
    const Part* source=find(part->linkedSketchSourceId);
    return source&&source->hasConstrainedPolygonDefinition&&
           source->constrainedPolygonGeometryToken==source->geometryToken&&
           source->geometryToken==part->linkedSketchSourceToken;
}

bool SceneDocument::propagateLinkedBodies(QVector<Part>* staged,QString* error) const {
    if(!staged){setError(error,"Missing staged linked scene.");return false;}
    QVector<int> order;
    if(!planLinkedBodyOrder(*staged,&order,error))return false;
    QMap<QString,int> indices;
    for(int i=0;i<staged->size();++i)indices.insert(staged->at(i).id,i);
    // Resolve in dependency order, not browser/creation order. This allows an
    // existing result to be retargeted to a newer independent body, while
    // cycle checks prevent an impossible update from entering the document.
    for(int i:order){
        Part& linked=(*staged)[i];
        if(!linked.linkedFoldedFlatSourceId.isEmpty()){
            const int sourceIndex=indices.value(linked.linkedFoldedFlatSourceId,-1);
            if(sourceIndex<0||sourceIndex==i){
                setError(error,"Linked flat blank references a missing or cyclic folded source.");return false;
            }
            const Part& source=(*staged)[sourceIndex];
            if(linked.linkedFoldedFlatSourceToken==source.geometryToken)continue;
            TopoDS_Shape rebuilt;
            if(!createLinkedFlatBlank(source,linked.linkedFoldedFlatKFactors,linked.linkedFoldedFlatRelief,&rebuilt,error))return false;
            linked.shape=rebuilt;
            linked.geometryToken=generatedId();
            linked.linkedFoldedFlatSourceToken=source.geometryToken;
            continue;
        }
        if(linked.hasInterleavedHistory){
            bool upToDate=linked.interleavedGeometryToken==linked.geometryToken&&
                linked.interleavedSourceTokens.size()==linked.interleavedSteps.size();
            if(upToDate){
                for(int j=0;j<linked.interleavedSteps.size();++j){
                    const auto& step=linked.interleavedSteps[j];
                    if(step.kind==QStringLiteral("Shape"))continue;
                    const int sourceIndex=indices.value(step.sketchSourceId,-1);
                    if(sourceIndex<0||linked.interleavedSourceTokens[j]!=staged->at(sourceIndex).geometryToken){
                        upToDate=false;break;
                    }
                }
            }
            if(upToDate)continue;
            TopoDS_Shape built;QStringList sourceTokens;
            if(!replayInterleavedHistory(linked,*staged,indices,&built,&sourceTokens,error))return false;
            linked.shape=built;linked.geometryToken=generatedId();
            linked.interleavedSourceTokens=std::move(sourceTokens);
            linked.interleavedGeometryToken=linked.geometryToken;
            continue;
        }
        if(linked.hasInPlaceSketchFeature){
            const int sourceIndex=indices.value(linked.inPlaceSketchSourceId,-1);
            if(sourceIndex<0||sourceIndex==i||linked.inPlaceSketchBase.IsNull()){
                setError(error,"In-place sketch source or frozen target body is missing.");return false;
            }
            const Part& source=staged->at(sourceIndex);
            if(!source.hasConstrainedPolygonDefinition||
               source.constrainedPolygonGeometryToken!=source.geometryToken){
                setError(error,"In-place sketch source is stale or no longer editable.");return false;
            }
            const Part* followSource=nullptr;
            if(linked.hasInPlaceSketchFollowUp){
                const int followIndex=indices.value(linked.inPlaceFollowSourceId,-1);
                if(followIndex<0||followIndex==i||followIndex==sourceIndex){
                    setError(error,"Second in-place sketch has a missing, self or duplicate source.");return false;
                }
                followSource=&staged->at(followIndex);
                if(!followSource->hasConstrainedPolygonDefinition||
                   followSource->constrainedPolygonGeometryToken!=followSource->geometryToken){
                    setError(error,"Second in-place sketch source is stale.");return false;
                }
            }
            QVector<const Part*> additionalSources;
            additionalSources.reserve(linked.extraInPlaceSketchStages.size());
            bool additionalUnchanged=true;
            for(const auto& stage:linked.extraInPlaceSketchStages){
                const int extraIndex=indices.value(stage.sourceId,-1);
                if(extraIndex<0||extraIndex==i){
                    setError(error,"Additional in-place sketch source is missing or circular.");return false;
                }
                const Part& extra=staged->at(extraIndex);
                if(!extra.hasConstrainedPolygonDefinition||
                   extra.constrainedPolygonGeometryToken!=extra.geometryToken){
                    setError(error,"Additional in-place sketch source is stale.");return false;
                }
                additionalSources.append(&extra);
                if(stage.sourceToken!=extra.geometryToken)additionalUnchanged=false;
            }
            if(linked.inPlaceSketchSourceToken==source.geometryToken&&
               (!followSource||linked.inPlaceFollowSourceToken==followSource->geometryToken)&&
               additionalUnchanged)continue;
            auto applyModifier=[&](const TopoDS_Shape& input,const Part& sketchPart,
                                   const QString& operation,double depth,double offset,
                                   TopoDS_Shape* output)->bool{
                TopoDS_Shape tool;
                if(!rebuildLinkedSketchSolid(sketchPart,depth,offset,&tool,error)||
                   !linkedBooleanResult(input,tool,operation,output,error))return false;
                if(operation==QStringLiteral("Fuse"))return linkedSketchFuseEffective(input,*output,error);
                if(operation==QStringLiteral("Cut"))return linkedSketchCutEffective(input,*output,error);
                setError(error,"Invalid in-place sketch operation.");return false;
            };
            TopoDS_Shape first,modified;
            if(!applyModifier(linked.inPlaceSketchBase,source,linked.inPlaceSketchOperation,
                              linked.inPlaceSketchDepthMm,linked.inPlaceSketchOffsetMm,&first))return false;
            if(followSource){
                if(!applyModifier(first,*followSource,linked.inPlaceFollowOperation,
                                  linked.inPlaceFollowDepthMm,linked.inPlaceFollowOffsetMm,&modified))return false;
            }else modified=first;
            for(int stageIndex=0;stageIndex<additionalSources.size();++stageIndex){
                const auto& stage=linked.extraInPlaceSketchStages[stageIndex];
                TopoDS_Shape next;
                if(!applyModifier(modified,*additionalSources[stageIndex],stage.operation,
                                  stage.depthMm,stage.offsetMm,&next))return false;
                modified=next;
            }
            // A general, shape-independent feature tree can now follow the
            // ordered external-sketch stages on the SAME body. Rebuild its
            // actual committed operations from the new upstream solid before
            // staging any geometry or token changes. No stale B-rep cache is
            // allowed to cross this upstream dependency boundary.
            TopoDS_Shape finalShape=modified;
            QVector<TopoDS_Shape> featureOutputs;
            if(linked.hasShapeFeatureTree){
                if(!rebuildShapeFeatureTree(modified,linked.isSurfaceBody,
                     linked.shapeFeatureNodes,&finalShape,error,nullptr,&featureOutputs))return false;
                linked.shapeFeatureBase=modified;
                linked.shapeFeatureCachedOutputs=std::move(featureOutputs);
            }
            linked.shape=finalShape;linked.geometryToken=generatedId();
            if(linked.hasShapeFeatureTree)
                linked.shapeFeatureGeometryToken=linked.geometryToken;
            linked.inPlaceSketchSourceToken=source.geometryToken;
            if(followSource)linked.inPlaceFollowSourceToken=followSource->geometryToken;
            for(int stageIndex=0;stageIndex<additionalSources.size();++stageIndex)
                linked.extraInPlaceSketchStages[stageIndex].sourceToken=additionalSources[stageIndex]->geometryToken;
            continue;
        }
        if(linked.hasLinkedSketchExtrusion){
            const int sourceIndex=indices.value(linked.linkedSketchSourceId,-1);
            if(sourceIndex<0||sourceIndex==i){
                setError(error,"Linked sketch source is missing or circular.");return false;
            }
            const Part& source=staged->at(sourceIndex);
            if(source.geometryToken==linked.linkedSketchSourceToken)continue;
            TopoDS_Shape newShape;
            if(!rebuildLinkedSketchSolid(source,linked.linkedSketchDepthMm,
                                         linked.linkedSketchOffsetMm,&newShape,error)||
               !validShape(newShape,error))return false;
            linked.shape=newShape;
            linked.geometryToken=generatedId();
            linked.linkedSketchSourceToken=source.geometryToken;
            continue;
        }
        if(!linked.hasLinkedBoolean)continue;
        const int first=indices.value(linked.linkedPrimaryId,-1);
        const int second=indices.value(linked.linkedToolId,-1);
        if(first<0||second<0||first==second){
            setError(error,"Linked Boolean sources are missing or duplicate.");return false;
        }
        // Copy input handles before writing the result. These sources are
        // distinct bodies but may themselves have just been rebuilt.
        const Part& primary=staged->at(first);
        const Part& tool=staged->at(second);
        if(primary.isSurfaceBody||tool.isSurfaceBody){
            setError(error,"Linked Boolean dependencies must be solid bodies.");return false;
        }
        if(linked.linkedPrimaryToken==primary.geometryToken&&linked.linkedToolToken==tool.geometryToken)continue;
        TopoDS_Shape result;
        if(!linkedBooleanResult(primary.shape,tool.shape,linked.linkedOperation,&result,error))return false;
        if(linked.linkedOperation==QStringLiteral("Cut")&&tool.hasLinkedSketchExtrusion&&
           !linkedSketchCutEffective(primary.shape,result,error))return false;
        if(linked.linkedOperation==QStringLiteral("Fuse")&&tool.hasLinkedSketchExtrusion&&
           !linkedSketchFuseEffective(primary.shape,result,error))return false;
        linked.shape=result;
        linked.geometryToken=generatedId();
        linked.linkedPrimaryToken=primary.geometryToken;
        linked.linkedToolToken=tool.geometryToken;
    }
    return true;
}
bool SceneDocument::linkedBooleanCurrent(const QString& id) const{
    const Part* p=find(id);
    if(!p||!p->hasLinkedBoolean)return false;
    const Part* a=find(p->linkedPrimaryId);
    const Part* b=find(p->linkedToolId);
    return a&&b&&p->linkedPrimaryToken==a->geometryToken&&p->linkedToolToken==b->geometryToken;
}
bool SceneDocument::linkedBodyImpact(const QString& sourceId,QStringList* bodyIds,
                                      QString* error,QStringList* explanationPaths) const {
    if(!bodyIds){setError(error,"Missing impact preview output.");return false;}
    const auto nodes=linkedBodyGraphNodes(parts_);
    std::vector<std::size_t> impacted;
    std::string reason;
    if(!linkedgraph::affected(nodes,sourceId.toStdString(),&impacted,&reason)){
        setError(error,QStringLiteral("Linked-body impact: %1").arg(QString::fromStdString(reason)));
        return false;
    }
    QStringList result;
    for(std::size_t index:impacted)result.append(parts_[static_cast<qsizetype>(index)].id);
    QStringList explanations;
    if(explanationPaths){
        std::vector<linkedgraph::ImpactCause> causes;
        if(!linkedgraph::explainAffectedMany(nodes,{sourceId.toStdString()},&causes,&reason)){
            setError(error,QStringLiteral("Linked-body impact explanation: %1").arg(QString::fromStdString(reason)));
            return false;
        }
        if(causes.size()!=impacted.size()){
            setError(error,"Linked-body impact explanation differs from the dependency plan.");return false;
        }
        for(std::size_t i=0;i<causes.size();++i){
            if(causes[i].nodeIndex!=impacted[i]){
                setError(error,"Linked-body impact explanation order differs from the dependency plan.");return false;
            }
            QStringList path;
            for(const auto& id:causes[i].pathIds)path<<QString::fromStdString(id);
            explanations<<path.join(QStringLiteral(" -> "));
        }
    }
    *bodyIds=std::move(result);
    if(explanationPaths)*explanationPaths=std::move(explanations);
    return true;
}

bool SceneDocument::refreshLinkedBooleanBodies(QString* error){
    QVector<Part> candidate=parts_;
    if(!propagateLinkedBodies(&candidate,error))return false;
    bool changed=false;
    for(int i=0;i<candidate.size();++i)
        if(candidate[i].geometryToken!=parts_[i].geometryToken){changed=true;break;}
    if(!changed)return true;
    checkpoint();parts_=std::move(candidate);return true;
}
bool SceneDocument::addLinkedBooleanBody(BooleanOp operation,const QString& primaryId,
                                          const QString& toolId,QString* error){
    if(primaryId==toolId||parts_.size()>=kMaxObjects){setError(error,"Select two different bodies; scene limit must not be exceeded.");return false;}
    const Part* primary=find(primaryId);
    const Part* tool=find(toolId);
    if(!primary||!tool||primary->isSurfaceBody||tool->isSurfaceBody){
        setError(error,"Linked Boolean requires two existing solid bodies.");return false;
    }
    if((primary->hasLinkedBoolean&&!linkedBooleanCurrent(primaryId))||
       (tool->hasLinkedBoolean&&!linkedBooleanCurrent(toolId))||
       (primary->hasLinkedSketchExtrusion&&!linkedSketchExtrusionCurrent(primaryId))||
       (tool->hasLinkedSketchExtrusion&&!linkedSketchExtrusionCurrent(toolId))){
        setError(error,"Refresh stale linked source bodies before creating another dependency.");return false;
    }
    const QString label=operation==BooleanOp::Fuse?"Fuse":operation==BooleanOp::Cut?"Cut":"Common";
    TopoDS_Shape result;
    if(!linkedBooleanResult(primary->shape,tool->shape,label,&result,error))return false;
    if(operation==BooleanOp::Cut&&tool->hasLinkedSketchExtrusion&&
       !linkedSketchCutEffective(primary->shape,result,error))return false;
    if(operation==BooleanOp::Fuse&&tool->hasLinkedSketchExtrusion&&
       !linkedSketchFuseEffective(primary->shape,result,error))return false;
    Part derived{generatedId(),uniqueName("Linked "+label),result,true,primary->color};
    derived.hasLinkedBoolean=true;
    derived.linkedOperation=label;
    derived.linkedPrimaryId=primaryId;derived.linkedToolId=toolId;
    derived.linkedPrimaryToken=primary->geometryToken;derived.linkedToolToken=tool->geometryToken;
    checkpoint();parts_.append(std::move(derived));return true;
}

namespace {
QString labelForLinkedOperation(BooleanOp operation) {
    switch(operation){
        case BooleanOp::Fuse:return QStringLiteral("Fuse");
        case BooleanOp::Cut:return QStringLiteral("Cut");
        case BooleanOp::Common:return QStringLiteral("Common");
    }
    return {};
}

// Only stages dependency metadata. Geometry is evaluated by the normal
// transactional topological rebuild after all changes have been collected.
bool stageLinkedEdits(QVector<Part>* candidate,
                      const QVector<LinkedBooleanEdit>& edits,
                      QStringList* changedIds,QString* error) {
    if(!candidate||!changedIds){if(error)*error=QStringLiteral("Missing batch staging output.");return false;}
    if(edits.size()>candidate->size()){
        if(error)*error=QStringLiteral("Too many linked Boolean edits.");return false;
    }
    QMap<QString,int> byId;
    for(int i=0;i<candidate->size();++i)byId.insert(candidate->at(i).id,i);
    QSet<QString> seen;
    QStringList changed;
    for(const LinkedBooleanEdit& edit:edits){
        if(edit.resultId.isEmpty()||seen.contains(edit.resultId)){
            if(error)*error=QStringLiteral("Each batch result must be nonempty and unique.");return false;
        }
        seen.insert(edit.resultId);
        const QString operation=labelForLinkedOperation(edit.operation);
        const int at=byId.value(edit.resultId,-1);
        const int a=byId.value(edit.primaryId,-1);
        const int b=byId.value(edit.toolId,-1);
        if(operation.isEmpty()||at<0||!candidate->at(at).hasLinkedBoolean||
           a<0||b<0||at==a||at==b||a==b||
           candidate->at(a).isSurfaceBody||candidate->at(b).isSurfaceBody){
            if(error)*error=QStringLiteral("Batch edit has an invalid result, operation, or solid source.");return false;
        }
        Part& result=(*candidate)[at];
        if(result.linkedOperation==operation&&result.linkedPrimaryId==edit.primaryId&&
           result.linkedToolId==edit.toolId)continue;
        result.linkedOperation=operation;
        result.linkedPrimaryId=edit.primaryId;
        result.linkedToolId=edit.toolId;
        result.linkedPrimaryToken.clear();
        result.linkedToolToken.clear();
        changed.append(edit.resultId);
    }
    QVector<int> graphOrder;
    if(!planLinkedBodyOrder(*candidate,&graphOrder,error))return false;
    *changedIds=std::move(changed);
    return true;
}
} // namespace

bool SceneDocument::previewLinkedBooleanBatch(const QVector<LinkedBooleanEdit>& edits,
                                               QStringList* affectedIds,QString* error) const {
    if(!affectedIds){setError(error,"Missing batch impact destination.");return false;}
    QVector<Part> staged=parts_;
    QStringList changed;
    if(!stageLinkedEdits(&staged,edits,&changed,error))return false;
    const auto nodes=linkedBodyGraphNodes(staged);
    std::vector<std::string> sources;
    sources.reserve(static_cast<std::size_t>(changed.size()));
    for(const QString& id:changed)sources.push_back(id.toStdString());
    std::vector<std::size_t> affected;
    std::string reason;
    if(!linkedgraph::affectedMany(nodes,sources,&affected,&reason)){
        setError(error,QStringLiteral("Batch impact: %1").arg(QString::fromStdString(reason)));
        return false;
    }
    QStringList result;
    for(std::size_t index:affected)result.append(staged[static_cast<qsizetype>(index)].id);
    *affectedIds=std::move(result);
    return true;
}

bool SceneDocument::updateLinkedBooleanBatch(const QVector<LinkedBooleanEdit>& edits,
                                               QString* error) {
    QVector<Part> candidate=parts_;
    QStringList changed;
    if(!stageLinkedEdits(&candidate,edits,&changed,error))return false;
    if(changed.isEmpty())return true;  // No checkpoint for a no-op.
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();
    parts_=std::move(candidate);
    return true;
}

bool SceneDocument::updateLinkedBooleanBody(const QString& resultId,BooleanOp operation,
                                             const QString& primaryId,const QString& toolId,
                                             QString* error) {
    return updateLinkedBooleanBatch({LinkedBooleanEdit{resultId,operation,primaryId,toolId}},error);
}

bool SceneDocument::rebaseShapeFeatureTree(const QString& partId,QString* error){
    Part* target=nullptr;for(Part& part:parts_)if(part.id==partId){target=&part;break;}
    if(!target||!target->hasShapeFeatureTree){setError(error,"Select a body with an existing feature stack.");return false;}
    // Explicitly freeze the current direct-modelled result, dropping the old
    // modifier sequence instead of attempting to infer an unsupported operation.
    if(target->shapeFeatureNodes.isEmpty()&&shapeFeatureTreeCurrent(partId))return true;
    checkpoint();target->shapeFeatureBase=target->shape;
    target->shapeFeatureNodes.clear();target->shapeFeatureCachedOutputs.clear();
    target->shapeFeatureGeometryToken=target->geometryToken;
    return true;
}

bool SceneDocument::addParametricRectangularExtrusion(double width,double height,double depth,QString* error) {
    if(!finitePositive(width)||!finitePositive(height)||!finitePositive(depth)){
        setError(error,"Sketch width, height, and extrusion depth must each be finite and between 0.00001 and 1,000,000 mm.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=BRepPrimAPI_MakeBox(width,height,depth).Shape();
        if(!validShape(shape,error))return false;
        Part part{generatedId(),uniqueName(QStringLiteral("Parametric Extrusion")),shape,true,"#6c9dc2"};
        part.hasParametricDefinition=true;
        part.parametricGeometryToken=part.geometryToken;
        part.namedParameters={{QStringLiteral("width"),width},{QStringLiteral("height"),height},{QStringLiteral("depth"),depth}};
        part.sketchConstraints={QStringLiteral("Coincident: rectangle origin at sketch origin"),
            QStringLiteral("Horizontal: lower rectangle edge"),QStringLiteral("Vertical: left rectangle edge"),
            QStringLiteral("Dimension: width = width parameter"),QStringLiteral("Dimension: height = height parameter")};
        part.featureHistory={QStringLiteral("Sketch: XY rectangle (fully constrained)"),
            QStringLiteral("Extrude: Base solid (depth parameter)")};
        checkpoint();parts_.append(std::move(part));return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Parametric sketch kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addConstrainedPolygonExtrusion(const SketchModel& sketch,const QStringList& boundaryPointIds,
                                                    double depth,QString* error,const SketchPlane& plane){
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    TopoDS_Shape shape;
    if(!buildSketchExtrusion(sketch,boundaryPointIds,depth,plane,&shape,error))return false;
    Part part{generatedId(),uniqueName(QStringLiteral("Constrained Sketch Extrusion")),shape};
    part.hasConstrainedPolygonDefinition=true;
    part.constrainedPolygonSketch=sketch;
    part.constrainedPolygonBoundary=boundaryPointIds;
    part.constrainedPolygonDepthMm=depth;
    part.constrainedPolygonPlane=plane;
    part.constrainedPolygonGeometryToken=part.geometryToken;
    checkpoint();parts_.append(std::move(part));return true;
}

bool SceneDocument::updateConstrainedPolygonExtrusion(const QString& partId,const SketchModel& sketch,
                                                       const QStringList& boundaryPointIds,double depth,
                                                       QString* error,const SketchPlane& plane){
    int index=-1;
    for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0||!parts_[index].hasConstrainedPolygonDefinition){
        setError(error,"Select an extrusion with a persisted editable sketch.");return false;
    }
    const Part& old=parts_[index];
    if(old.geometryToken!=old.constrainedPolygonGeometryToken||
       (old.hasShapeFeatureTree&&!shapeFeatureTreeCurrent(partId))){
        setError(error,"The sketch dependency is stale after an unsupported direct geometry edit. The body remains unchanged.");return false;
    }
    if(depth==old.constrainedPolygonDepthMm&&boundaryPointIds==old.constrainedPolygonBoundary&&
       sketchModelToJson(sketch)==sketchModelToJson(old.constrainedPolygonSketch)&&
       sketchPlaneToJson(plane)==sketchPlaneToJson(old.constrainedPolygonPlane))return true;
    // Build sketch first, and replay the complete downstream dependency graph.
    // A single invalid downstream operation aborts before the undo checkpoint.
    TopoDS_Shape base,finished;
    QVector<TopoDS_Shape> intermediate;
    if(!buildSketchExtrusion(sketch,boundaryPointIds,depth,plane,&base,error))return false;
    if(old.hasShapeFeatureTree){
        if(!rebuildShapeFeatureTree(base,old.isSurfaceBody,old.shapeFeatureNodes,&finished,error,
                                    nullptr,&intermediate)){
            if(error)*error="Downstream feature regeneration failed: "+*error;
            return false;
        }
    }else finished=base;
    QVector<Part> candidate=parts_;
    Part& part=candidate[index];
    part.shape=finished;
    if(part.hasShapeFeatureTree){
        part.shapeFeatureBase=base;
        part.shapeFeatureCachedOutputs=std::move(intermediate);
    }
    part.geometryToken=generatedId();
    part.constrainedPolygonGeometryToken=part.geometryToken;
    if(part.hasShapeFeatureTree)part.shapeFeatureGeometryToken=part.geometryToken;
    part.constrainedPolygonSketch=sketch;
    part.constrainedPolygonBoundary=boundaryPointIds;
    part.constrainedPolygonDepthMm=depth;
    part.constrainedPolygonPlane=plane;
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);
    return true;
}

bool SceneDocument::addParametricRectangularExtrusionAt(double originX,double originY,double width,double height,double depth,QString* error) {
    if(!finiteOffset(originX)||!finiteOffset(originY)||!finitePositive(width)||!finitePositive(height)||!finitePositive(depth)){
        setError(error,"Sketch origin coordinates must be finite and within ±1,000,000 mm; width, height, and depth must be positive dimensions up to 1,000,000 mm.");return false;
    }
    if(!finiteOffset(originX+width)||!finiteOffset(originY+height)){setError(error,"Sketch rectangle coordinates exceed the supported range.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=BRepPrimAPI_MakeBox(gp_Pnt(originX,originY,0),gp_Pnt(originX+width,originY+height,depth)).Shape();
        if(!validShape(shape,error))return false;
        Part part{generatedId(),uniqueName(QStringLiteral("Parametric Offset Extrusion")),shape,true,"#6c9dc2"};
        part.hasParametricDefinition=true;part.parametricGeometryToken=part.geometryToken;
        part.namedParameters={{QStringLiteral("origin_x"),originX},{QStringLiteral("origin_y"),originY},
            {QStringLiteral("width"),width},{QStringLiteral("height"),height},{QStringLiteral("depth"),depth}};
        part.sketchConstraints={QStringLiteral("Dimension: origin X = origin_x parameter"),
            QStringLiteral("Dimension: origin Y = origin_y parameter"),QStringLiteral("Horizontal: lower rectangle edge"),
            QStringLiteral("Vertical: left rectangle edge"),QStringLiteral("Dimension: width = width parameter"),
            QStringLiteral("Dimension: height = height parameter")};
        part.featureHistory={QStringLiteral("Sketch: XY rectangle with editable origin (fully constrained)"),
            QStringLiteral("Extrude: Base solid (depth parameter)")};
        checkpoint();parts_.append(std::move(part));return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Parametric offset sketch kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addParametricThroughHole(const QString& partId,double diameter,double centerX,double centerY,QString* error) {
    int index=-1;
    for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Select an existing parametric base extrusion.");return false;}
    const Part& current=parts_[index];
    const bool offsetProfile=current.namedParameters.contains(QStringLiteral("origin_x"));
    const QStringList expectedBaseHistory=offsetProfile?
        QStringList({QStringLiteral("Sketch: XY rectangle with editable origin (fully constrained)"),QStringLiteral("Extrude: Base solid (depth parameter)")}):
        QStringList({QStringLiteral("Sketch: XY rectangle (fully constrained)"),QStringLiteral("Extrude: Base solid (depth parameter)")});
    if(!current.hasParametricDefinition||!parametricDefinitionCurrent(partId)||current.featureHistory!=expectedBaseHistory){
        setError(error,"A parametric through-hole can be added once to a current rectangle-and-extrusion history.");return false;
    }
    if(!finitePositive(diameter)||!finiteOffset(centerX)||!finiteOffset(centerY)){
        setError(error,"Hole diameter must be positive and its center coordinates must be finite and within the supported range.");return false;
    }
    const double width=current.namedParameters.value(QStringLiteral("width"));
    const double height=current.namedParameters.value(QStringLiteral("height"));
    const double originX=current.namedParameters.value(QStringLiteral("origin_x"));
    const double originY=current.namedParameters.value(QStringLiteral("origin_y"));
    const double radius=diameter/2.0;
    if(centerX-radius<=originX||centerY-radius<=originY||centerX+radius>=originX+width||centerY+radius>=originY+height){
        setError(error,"The through-hole circle must remain fully inside the rectangular sketch profile.");return false;
    }
    try{
        const double depth=current.namedParameters.value(QStringLiteral("depth"));
        const TopoDS_Shape cutter=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(centerX,centerY,-0.01),gp_Dir(0,0,1)),radius,depth+0.02).Shape();
        BRepAlgoAPI_Cut cut(current.shape,cutter);cut.Build();
        if(!cut.IsDone()||cut.HasErrors()){setError(error,"The parametric through-hole cut failed.");return false;}
        const TopoDS_Shape result=cut.Shape();if(!validShape(result,error))return false;
        checkpoint();Part& updated=parts_[index];updated.shape=result;
        updated.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);updated.parametricGeometryToken=updated.geometryToken;
        updated.namedParameters.insert(QStringLiteral("hole_diameter"),diameter);
        updated.namedParameters.insert(QStringLiteral("hole_x"),centerX);updated.namedParameters.insert(QStringLiteral("hole_y"),centerY);
        updated.sketchConstraints.append(QStringLiteral("Coincident: circle center = named hole X/Y parameters"));
        updated.sketchConstraints.append(QStringLiteral("Dimension: circle diameter = hole_diameter parameter"));
        updated.sketchConstraints.append(QStringLiteral("Containment: circle remains inside the rectangular profile"));
        updated.featureHistory.append(QStringLiteral("Sketch: XY circle (fully constrained by center and diameter)"));
        updated.featureHistory.append(QStringLiteral("Cut: Parametric through-hole"));
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Parametric through-hole kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addParametricBlindHole(const QString& partId,double diameter,double centerX,double centerY,
                                           double holeDepthMm,QString* error) {
    int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Select an existing parametric base extrusion.");return false;}
    const Part& current=parts_[index];
    const bool offsetProfile=current.namedParameters.contains(QStringLiteral("origin_x"));
    const QStringList expectedBaseHistory=offsetProfile?
        QStringList({QStringLiteral("Sketch: XY rectangle with editable origin (fully constrained)"),QStringLiteral("Extrude: Base solid (depth parameter)")}):
        QStringList({QStringLiteral("Sketch: XY rectangle (fully constrained)"),QStringLiteral("Extrude: Base solid (depth parameter)")});
    if(!current.hasParametricDefinition||!parametricDefinitionCurrent(partId)||current.featureHistory!=expectedBaseHistory){
        setError(error,"A parametric blind hole can be added once to a current rectangle-and-extrusion history.");return false;
    }
    const double baseDepth=current.namedParameters.value(QStringLiteral("depth"));
    if(!finitePositive(diameter)||!finiteOffset(centerX)||!finiteOffset(centerY)||
       !finitePositive(holeDepthMm)||holeDepthMm>=baseDepth-std::max(0.01,baseDepth*1e-5)){
        setError(error,"Hole diameter and depth must be positive; blind-hole depth must remain below the extrusion depth, and the center coordinates must be finite.");return false;
    }
    const double width=current.namedParameters.value(QStringLiteral("width"));
    const double height=current.namedParameters.value(QStringLiteral("height"));
    const double originX=current.namedParameters.value(QStringLiteral("origin_x"));
    const double originY=current.namedParameters.value(QStringLiteral("origin_y"));
    const double radius=diameter/2.0;
    if(centerX-radius<=originX||centerY-radius<=originY||centerX+radius>=originX+width||centerY+radius>=originY+height){
        setError(error,"The blind-hole circle must remain fully inside the rectangular sketch profile.");return false;
    }
    try{
        const TopoDS_Shape cutter=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(centerX,centerY,baseDepth-holeDepthMm),gp_Dir(0,0,1)),radius,holeDepthMm+0.01).Shape();
        BRepAlgoAPI_Cut cut(current.shape,cutter);cut.Build();
        if(!cut.IsDone()||cut.HasErrors()){setError(error,"The parametric blind-hole cut failed.");return false;}
        const TopoDS_Shape result=cut.Shape();if(!validShape(result,error))return false;
        checkpoint();Part& updated=parts_[index];updated.shape=result;
        updated.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);updated.parametricGeometryToken=updated.geometryToken;
        updated.namedParameters.insert(QStringLiteral("hole_diameter"),diameter);
        updated.namedParameters.insert(QStringLiteral("hole_x"),centerX);updated.namedParameters.insert(QStringLiteral("hole_y"),centerY);
        updated.namedParameters.insert(QStringLiteral("hole_depth"),holeDepthMm);
        updated.sketchConstraints.append(QStringLiteral("Coincident: circle center = named hole X/Y parameters"));
        updated.sketchConstraints.append(QStringLiteral("Dimension: circle diameter = hole_diameter parameter"));
        updated.sketchConstraints.append(QStringLiteral("Dimension: blind cut depth = hole_depth parameter"));
        updated.sketchConstraints.append(QStringLiteral("Containment: circle remains inside the rectangular profile"));
        updated.featureHistory.append(QStringLiteral("Sketch: XY circle (fully constrained by center and diameter)"));
        updated.featureHistory.append(QStringLiteral("Cut: Parametric blind hole"));
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Parametric blind-hole kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addParametricLinearHolePattern(const QString& partId,int holeCount,double spacingMm,QString* error) {
    return addParametricLinearHolePattern(partId,holeCount,spacingMm,QStringLiteral("X"),error);
}

bool SceneDocument::addParametricLinearHolePattern(const QString& partId,int holeCount,double spacingMm,
                                                   const QString& axis,QString* error) {
    int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Select a parametric body with a through-hole or blind hole.");return false;}
    const Part& current=parts_[index];
    const bool blind=current.featureHistory.contains(QStringLiteral("Cut: Parametric blind hole"));
    if(!current.hasParametricDefinition||!parametricDefinitionCurrent(partId)||
       (!blind&&!current.featureHistory.contains(QStringLiteral("Cut: Parametric through-hole")))||
       (current.featureHistory.contains(QStringLiteral("Pattern: Linear through-hole array along X"))||
        current.featureHistory.contains(QStringLiteral("Pattern: Linear through-hole array along Y"))||
        current.featureHistory.contains(QStringLiteral("Pattern: Linear blind-hole array along X"))||
        current.featureHistory.contains(QStringLiteral("Pattern: Linear blind-hole array along Y")))||
       current.featureHistory.contains(QStringLiteral("Chamfer: All edges"))||
       current.featureHistory.contains(QStringLiteral("Fillet: All edges"))){
        setError(error,"Add one parametric through-hole or blind hole to a current rectangular extrusion before creating its linear pattern.");return false;
    }
    if(holeCount<2||holeCount>100||!finitePositive(spacingMm)||(axis!="X"&&axis!="Y")){
        setError(error,"A linear hole pattern needs 2 to 100 instances and positive spacing up to 1,000,000 mm.");return false;
    }
    const auto& p=current.namedParameters;
    const double diameter=p.value(QStringLiteral("hole_diameter"));
    const double firstX=p.value(QStringLiteral("hole_x")),centerY=p.value(QStringLiteral("hole_y"));
    const double originX=p.value(QStringLiteral("origin_x")),originY=p.value(QStringLiteral("origin_y"));
    const double width=p.value(QStringLiteral("width")),height=p.value(QStringLiteral("height"));
    const bool alongX=axis=="X";
    const double lastCoordinate=(alongX?firstX:centerY)+spacingMm*(holeCount-1),radius=diameter/2.0;
    if(spacingMm<diameter||!std::isfinite(lastCoordinate)||
       (alongX?(firstX-radius<=originX||lastCoordinate+radius>=originX+width):
               (centerY-radius<=originY||lastCoordinate+radius>=originY+height))||
       (alongX?(centerY-radius<=originY||centerY+radius>=originY+height):
               (firstX-radius<=originX||firstX+radius>=originX+width))){
        setError(error,"Pattern instances must be separated by at least the hole diameter and remain fully inside the rectangular profile.");return false;
    }
    try{
        // Subtract all additional pattern holes in one Boolean operation,
        // rather than repeatedly reclassifying a growing result for each cut.
        BRep_Builder builder;TopoDS_Compound cutters;builder.MakeCompound(cutters);
        const double baseDepth=p.value(QStringLiteral("depth"));
        const double cutDepth=blind?p.value(QStringLiteral("hole_depth")):baseDepth;
        const double startZ=blind?baseDepth-cutDepth:-0.01;
        for(int i=1;i<holeCount;++i){
            const double x=firstX+(alongX?spacingMm*i:0.0),y=centerY+(alongX?0.0:spacingMm*i);
            builder.Add(cutters,BRepPrimAPI_MakeCylinder(
                gp_Ax2(gp_Pnt(x,y,startZ),gp_Dir(0,0,1)),radius,cutDepth+(blind?0.01:0.02)).Shape());
        }
        BRepAlgoAPI_Cut cut(current.shape,cutters);cut.Build();
        if(!cut.IsDone()||cut.HasErrors()){setError(error,"A linear-pattern through-hole cut failed.");return false;}
        const TopoDS_Shape result=cut.Shape();
        if(!validShape(result,error))return false;
        checkpoint();Part& updated=parts_[index];updated.shape=result;
        updated.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);updated.parametricGeometryToken=updated.geometryToken;
        updated.namedParameters.insert(QStringLiteral("hole_pattern_count"),holeCount);
        updated.namedParameters.insert(QStringLiteral("hole_pattern_spacing"),spacingMm);
        updated.holePatternAxis=axis;
        updated.sketchConstraints.append(QStringLiteral("Pattern: hole centers spaced by hole_pattern_spacing along ")+axis);
        updated.sketchConstraints.append(QStringLiteral("Dimension: hole_pattern_count is an integer from 2 to 100"));
        updated.featureHistory.append(blind?QStringLiteral("Pattern: Linear blind-hole array along ")+axis:
                                              QStringLiteral("Pattern: Linear through-hole array along ")+axis);
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Parametric hole-pattern kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addParametricAllEdgeChamfer(const QString& partId,double distanceMm,QString* error) {
    int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Select an existing parametric solid.");return false;}
    const Part& current=parts_[index];
    if(!current.hasParametricDefinition||!parametricDefinitionCurrent(partId)||
       !current.featureHistory.contains(QStringLiteral("Extrude: Base solid (depth parameter)"))){
        setError(error,"A current rectangle-and-extrusion feature history is required for a parametric chamfer.");return false;
    }
    if(current.featureHistory.contains(QStringLiteral("Chamfer: All edges"))){
        setError(error,"This parametric history already contains an all-edge chamfer.");return false;
    }
    if(current.featureHistory.contains(QStringLiteral("Fillet: All edges"))){
        setError(error,"This bounded parametric history supports either an all-edge fillet or chamfer, not both.");return false;
    }
    if(!finitePositive(distanceMm)||distanceMm>=0.5*std::min({current.namedParameters.value(QStringLiteral("width")),
        current.namedParameters.value(QStringLiteral("height")),current.namedParameters.value(QStringLiteral("depth"))})){
        setError(error,"Chamfer distance must be positive and less than half the smallest base dimension.");return false;
    }
    try{
        Part proposed=current;
        proposed.namedParameters.insert(QStringLiteral("chamfer_distance"),distanceMm);
        proposed.sketchConstraints.append(QStringLiteral("Dimension: all-edge chamfer = chamfer_distance parameter"));
        proposed.featureHistory.append(QStringLiteral("Chamfer: All edges"));
        TopoDS_Shape rebuilt;
        if(!rebuildParametricSolid(proposed,proposed.namedParameters,proposed.suppressedParametricFeatures,&rebuilt,error))return false;
        checkpoint();
        Part& updated=parts_[index];updated.shape=rebuilt;
        updated.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        updated.parametricGeometryToken=updated.geometryToken;
        updated.namedParameters=std::move(proposed.namedParameters);
        updated.sketchConstraints=std::move(proposed.sketchConstraints);
        updated.featureHistory=std::move(proposed.featureHistory);
        return true;
    }catch(const Standard_Failure& failure){setError(error,QStringLiteral("Parametric chamfer kernel error: %1").arg(failure.what()));}
     catch(const std::exception& failure){setError(error,QStringLiteral("Parametric chamfer failed: %1").arg(QString::fromUtf8(failure.what())));}
    return false;
}

bool SceneDocument::addParametricAllEdgeFillet(const QString& partId,double radiusMm,QString* error) {
    int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Select an existing parametric solid.");return false;}
    const Part& current=parts_[index];
    if(!current.hasParametricDefinition||!parametricDefinitionCurrent(partId)||
       !current.featureHistory.contains(QStringLiteral("Extrude: Base solid (depth parameter)"))){
        setError(error,"A current rectangle-and-extrusion feature history is required for a parametric fillet.");return false;
    }
    if(current.featureHistory.contains(QStringLiteral("Fillet: All edges"))){
        setError(error,"This parametric history already contains an all-edge fillet.");return false;
    }
    if(!finitePositive(radiusMm)||radiusMm>=0.5*std::min({current.namedParameters.value(QStringLiteral("width")),
        current.namedParameters.value(QStringLiteral("height")),current.namedParameters.value(QStringLiteral("depth"))})){
        setError(error,"Fillet radius must be positive and less than half the smallest base dimension.");return false;
    }
    try{
        Part proposed=current;
        proposed.namedParameters.insert(QStringLiteral("fillet_radius"),radiusMm);
        proposed.sketchConstraints.append(QStringLiteral("Dimension: all-edge fillet = fillet_radius parameter"));
        proposed.featureHistory.append(QStringLiteral("Fillet: All edges"));
        TopoDS_Shape rebuilt;
        if(!rebuildParametricSolid(proposed,proposed.namedParameters,proposed.suppressedParametricFeatures,&rebuilt,error))return false;
        checkpoint();
        Part& updated=parts_[index];updated.shape=rebuilt;
        updated.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        updated.parametricGeometryToken=updated.geometryToken;
        updated.namedParameters=std::move(proposed.namedParameters);
        updated.sketchConstraints=std::move(proposed.sketchConstraints);
        updated.featureHistory=std::move(proposed.featureHistory);
        return true;
    }catch(const Standard_Failure& failure){setError(error,QStringLiteral("Parametric fillet kernel error: %1").arg(failure.what()));}
     catch(const std::exception& failure){setError(error,QStringLiteral("Parametric fillet failed: %1").arg(QString::fromUtf8(failure.what())));}
    return false;
}

bool SceneDocument::setParameterLink(const QString& partId,const QString& target,const QString& source,
                                     double scale,QString* error) {
    return setParameterLink(partId,target,source,scale,0.0,error);
}

bool SceneDocument::setParameterLink(const QString& partId,const QString& target,const QString& source,
                                     double scale,double offset,QString* error) {
    int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Object not found.");return false;}
    const Part& current=parts_[index];
    if(!current.hasParametricDefinition||!parametricDefinitionCurrent(partId)){
        setError(error,"An editable parametric history is required for parameter dependencies.");return false;
    }
    QMap<QString,ParameterLink> links=current.parameterLinks;
    if(links.value(target).source==source&&links.value(target).scale==scale&&links.value(target).offset==offset)return true;
    // Replacing a link is one transaction. Validate the proposed complete
    // graph before touching geometry so a cycle or downstream constraint
    // failure leaves both the old dependency and its driven value intact.
    links.insert(target,ParameterLink{source,scale,offset});
    QMap<QString,double> resolved=current.namedParameters;
    if(!resolveParameterLinks(links,resolved,error))return false;
    const bool geometryChanges=resolved!=current.namedParameters;
    if(geometryChanges){
        // Reuse the ordinary geometry transaction with the proposed graph so
        // downstream constraints are checked before either link or solid changes.
        if(!setNamedParametersWithLinks(partId,{},links,true,error))return false;
    }else checkpoint();
    parts_[index].parameterLinks=std::move(links);
    parts_[index].namedParameterFormulas.remove(target);
    return true;
}

bool SceneDocument::removeParameterLink(const QString& partId,const QString& target,QString* error) {
    int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Object not found.");return false;}
    if(!parts_[index].hasParametricDefinition||!parametricDefinitionCurrent(partId)||
       !parts_[index].parameterLinks.contains(target)){
        setError(error,"No editable parameter dependency exists for this target.");return false;
    }
    checkpoint();parts_[index].parameterLinks.remove(target);return true;
}

QMap<QString,ParameterLink> SceneDocument::parameterLinks(const QString& partId) const {
    const Part* part=find(partId);
    return part&&part->hasParametricDefinition?part->parameterLinks:QMap<QString,ParameterLink>{};
}


namespace {
// Evaluate saved formula DAGs in base engineering units before touching B-reps.
// Explicit dimension edits are able to override their own saved formulas; other
// formulas are reevaluated on every source change. No stale numeric snapshot is
// accepted when a formula or its referenced source changes.
bool evaluatePersistentDimensionFormulas(const QMap<QString,double>& values,
      const QMap<QString,QString>& formulas,QMap<QString,double>* result,QString* error){
    const auto reject=[&](const QString& message){if(error)*error=message;return false;};
    if(!result)return reject("Missing dimension-formula output.");
    if(formulas.size()>64)return reject("Too many persisted dimension formulas.");
    std::map<std::string,study::Quantity> baseline;
    std::map<std::string,study::Dimension> dimensions;
    std::map<std::string,std::string> edits;
    for(auto it=values.cbegin();it!=values.cend();++it){
        const bool count=it.key()==QStringLiteral("hole_pattern_count");
        const auto unit=count?study::scalar:study::length;
        baseline[it.key().toStdString()]={it.value(),unit};
        dimensions[it.key().toStdString()]=unit;
    }
    for(auto it=formulas.cbegin();it!=formulas.cend();++it){
        if(!values.contains(it.key())||it.value().trimmed().isEmpty()||it.value().size()>256||
           it.value().contains(QLatin1Char('\n'))||it.value().contains(QLatin1Char('\r'))){
            return reject("Dimension formulas must refer to existing parameters and be 1-256 printable characters.");
        }
        edits.emplace(it.key().toStdString(),it.value().toStdString());
    }
    std::map<std::string,study::Quantity> resolved;
    std::string problem;
    if(!study::evaluateParameterExpressions(baseline,edits,dimensions,&resolved,&problem)){
        return reject(QString::fromStdString(problem));
    }
    QMap<QString,double> numeric=values;
    for(const auto& [name,value]:resolved)numeric[QString::fromStdString(name)]=value.value;
    *result=std::move(numeric);return true;
}
}

// Feature-level notes deliberately use the stable feature node identity,
// never an OpenCascade face traversal index. Missing nodes stay visible as
// ORPHANED after edits so an engineer can recover or remove the note.
static QStringList featureInspectionIds(const Part& part){
    QStringList result;
    if(part.hasInterleavedHistory){
        for(const InterleavedHistoryStep& step:part.interleavedSteps)
            if(!step.id.isEmpty()&&!result.contains(step.id))result.append(step.id);
    }else if(part.hasShapeFeatureTree){
        for(const ShapeFeatureNode& node:part.shapeFeatureNodes)
            if(!node.id.isEmpty()&&!result.contains(node.id))result.append(node.id);
    }
    return result;
}

static bool validFeatureNotes(const QVector<FeatureInspectionNote>& notes,QString* error){
    if(notes.size()>100){
        if(error)*error="At most 100 feature inspection notes per body.";
        return false;
    }
    QSet<QString> seen;
    for(const auto& note:notes){
        if(note.id.isEmpty()||note.id.size()>64||seen.contains(note.id)||
           note.featureId.isEmpty()||note.featureId.size()>100||
           note.reviewedGeometryToken.isEmpty()||note.reviewedGeometryToken.size()>64||
           (note.category!="DESIGN"&&note.category!="INSPECTION"&&note.category!="PROCESS")||
           note.text.trimmed().isEmpty()||note.text.size()>240||
           note.text.contains('\n')||note.text.contains('\r')||
           note.featureId.contains('\n')||note.featureId.contains('\r')){
            if(error)*error="Malformed, duplicated or unsupported feature-level inspection annotation.";
            return false;
        }
        seen.insert(note.id);
    }
    return true;
}

QStringList SceneDocument::inspectionFeatureIds(const QString& id) const{
    const Part* part=find(id);return part?featureInspectionIds(*part):QStringList{};
}
QVector<FeatureInspectionNote> SceneDocument::featureInspectionNotes(const QString& id) const{
    const Part* part=find(id);return part?part->featureInspectionNotes:QVector<FeatureInspectionNote>{};
}
bool SceneDocument::setFeatureInspectionNotes(const QString& id,
                          const QVector<FeatureInspectionNote>& notes,QString* error){
    int index=-1;
    for(int i=0;i<parts_.size();++i)if(parts_[i].id==id){index=i;break;}
    if(index<0||parts_[index].isSurfaceBody){
        setError(error,"Choose an existing solid with a feature-history tree.");return false;
    }
    if(!validFeatureNotes(notes,error))return false;
    const QStringList allowed=featureInspectionIds(parts_[index]);
    const auto& old=parts_[index].featureInspectionNotes;
    for(const auto& note:notes){
        const bool previouslyPresent=std::any_of(old.cbegin(),old.cend(),
            [&](const auto& previous){return previous.id==note.id&&
                      previous.featureId==note.featureId;});
        if(!allowed.contains(note.featureId)&&!previouslyPresent){
            setError(error,"A new inspection annotation must reference an existing stable feature ID.");return false;
        }
        if(note.reviewedGeometryToken!=parts_[index].geometryToken){
            const bool existingStale=std::any_of(old.cbegin(),old.cend(),
                [&](const auto& previous){return previous.id==note.id&&
                    previous.featureId==note.featureId&&
                    previous.reviewedGeometryToken==note.reviewedGeometryToken;});
            if(!existingStale){
                setError(error,"New notes cannot claim review of an old geometry revision.");return false;
            }
        }
    }
    if(old==notes)return true;
    checkpoint();parts_[index].featureInspectionNotes=notes;return true;
}

bool SceneDocument::buildFeatureInspectionCsv(const QString& id,QByteArray* csv,QString* error) const{
    const Part* part=find(id);
    if(!part||!csv){setError(error,"Select a body and provide a report output buffer.");return false;}
    if(!validFeatureNotes(part->featureInspectionNotes,error))return false;
    const QStringList available=featureInspectionIds(*part);
    auto csvValue=[](QString value){
        if(!value.trimmed().isEmpty()&&QStringLiteral("=+-@\t\r").contains(value.trimmed().front()))
            value.prepend(QChar(0x27));
        value.replace('"',QStringLiteral("\"\""));
        return QStringLiteral("\"")+value+QStringLiteral("\"");
    };
    QString report=QStringLiteral("CADWorkbench feature-linked engineering notes,DESIGN REVIEW ONLY - NOT CERTIFIED INSPECTION\r\n");
    report+=QStringLiteral("Source body,")+csvValue(id)+QStringLiteral("\r\n");
    report+=QStringLiteral("Current geometry revision,")+csvValue(part->geometryToken)+QStringLiteral("\r\n");
    report+=QStringLiteral("Note ID,Feature ID,Category,Note,Reviewed geometry revision,Status\r\n");
    for(const auto& note:part->featureInspectionNotes){
        const QString status=!available.contains(note.featureId)?QStringLiteral("MISSING_FEATURE"):
            note.reviewedGeometryToken==part->geometryToken?QStringLiteral("CURRENT_REVISION"):
                                                       QStringLiteral("REVIEW_REQUIRED");
        report+=csvValue(note.id)+','+csvValue(note.featureId)+','+csvValue(note.category)+','+
                csvValue(note.text)+','+csvValue(note.reviewedGeometryToken)+','+status+QStringLiteral("\r\n");
    }
    *csv=report.toUtf8();if(error)error->clear();return true;
}
bool SceneDocument::exportFeatureInspectionCsv(const QString& id,const QString& fileName,QString* error) const{
    if(fileName.trimmed().isEmpty()){
        setError(error,"Provide a report filename.");return false;
    }
    QByteArray contents;
    if(!buildFeatureInspectionCsv(id,&contents,error))return false;
    QSaveFile output(fileName);
    if(!output.open(QIODevice::WriteOnly)){
        setError(error,output.errorString());return false;
    }
    if(output.write(contents)!=contents.size()){
        setError(error,output.errorString());output.cancelWriting();return false;
    }
    if(!output.commit()){
        setError(error,output.errorString());return false;
    }
    if(error)error->clear();return true;
}

// A stored configuration is engineering intent, not a cached result. Validate
// the entire catalog before save/load or mutation, and re-evaluate against the
// *current* dimensional baseline every time a preset is applied.
static bool validateDesignConfigurations(const Part& part,
                                         const QVector<SavedDesignConfiguration>& definitions,
                                         QString* error){
    if(!part.hasParametricDefinition){
        if(error)*error="Design configurations require an editable parametric body.";
        return false;
    }
    if(definitions.size()>32){
        if(error)*error="A body may contain at most 32 stored design configurations.";
        return false;
    }
    std::map<std::string,study::Quantity> baseline;
    std::map<std::string,study::Dimension> dimensions;
    for(auto it=part.namedParameters.cbegin();it!=part.namedParameters.cend();++it){
        const study::Dimension unit=it.key()==QStringLiteral("hole_pattern_count")?
            study::scalar:study::length;
        baseline.emplace(it.key().toStdString(),study::Quantity{it.value(),unit});
        dimensions.emplace(it.key().toStdString(),unit);
    }
    QSet<QString> names;
    std::vector<study::Configuration> cases;
    for(const SavedDesignConfiguration& definition:definitions){
        const QString name=definition.name.trimmed();
        if(name.isEmpty()||name!=definition.name||name.size()>48||
           name.contains('\n')||name.contains('\r')||name.contains(':')||
           names.contains(name.toCaseFolded())||
           definition.expressions.isEmpty()||definition.expressions.size()>16){
            if(error)*error="Invalid, duplicate or empty design configuration name/parameters.";
            return false;
        }
        names.insert(name.toCaseFolded());
        study::Configuration candidate;candidate.name=name.toStdString();
        for(auto it=definition.expressions.cbegin();it!=definition.expressions.cend();++it){
            if(!part.namedParameters.contains(it.key())||part.parameterLinks.contains(it.key())||
               it.value().trimmed().isEmpty()||it.value().size()>256||
               it.value().contains('\n')||it.value().contains('\r')){
                if(error)*error="A saved configuration has an unknown, linked or invalid dimension expression.";
                return false;
            }
            candidate.parameters.emplace(it.key().toStdString(),it.value().toStdString());
        }
        cases.push_back(std::move(candidate));
    }
    const auto evaluated=study::evaluateConfigurations(baseline,dimensions,cases);
    if(evaluated.size()!=cases.size()){
        if(error)*error="Configuration evaluation returned an incomplete result.";
        return false;
    }
    for(std::size_t i=0;i<evaluated.size();++i){
        if(!evaluated[i].valid){
            if(error)*error=QStringLiteral("Configuration %1: %2").arg(definitions[int(i)].name,
                          QString::fromStdString(evaluated[i].error));
            return false;
        }
        for(const auto& [key,quantity]:evaluated[i].values){
            const QString name=QString::fromStdString(key);
            const bool coordinate=name==QStringLiteral("origin_x")||
                name==QStringLiteral("origin_y")||name==QStringLiteral("hole_x")||
                name==QStringLiteral("hole_y");
            const bool count=name==QStringLiteral("hole_pattern_count");
            if(count?(!std::isfinite(quantity.value)||quantity.value<2.0||
                      quantity.value>100.0||std::floor(quantity.value)!=quantity.value):
               coordinate?!finiteOffset(quantity.value):!finitePositive(quantity.value)){
                if(error)*error=QStringLiteral("Configuration %1 has a dimension outside the valid design range.")
                                  .arg(definitions[int(i)].name);
                return false;
            }
        }
    }
    return true;
}

QVector<SavedDesignConfiguration> SceneDocument::designConfigurations(const QString& id) const{
    const Part* part=find(id);
    return part&&part->hasParametricDefinition?part->designConfigurations:
        QVector<SavedDesignConfiguration>{};
}

bool SceneDocument::setDesignConfigurations(const QString& id,
                          const QVector<SavedDesignConfiguration>& configurations,QString* error){
    int index=-1;
    for(int i=0;i<parts_.size();++i)if(parts_[i].id==id){index=i;break;}
    if(index<0||!parts_[index].hasParametricDefinition||!parametricDefinitionCurrent(id)){
        setError(error,"Choose a body with a current editable parametric definition.");return false;
    }
    if(!validateDesignConfigurations(parts_[index],configurations,error))return false;
    QString active=parts_[index].activeDesignConfiguration;
    if(!active.isEmpty()&&std::none_of(configurations.cbegin(),configurations.cend(),
       [&](const SavedDesignConfiguration& candidate){return candidate.name==active;}))active.clear();
    if(parts_[index].designConfigurations==configurations&&
       parts_[index].activeDesignConfiguration==active)return true;
    checkpoint();parts_[index].designConfigurations=configurations;
    parts_[index].activeDesignConfiguration=active;
    return true;
}

bool SceneDocument::applyDesignConfiguration(const QString& id,const QString& name,QString* error){
    const Part* part=find(id);
    if(!part||!part->hasParametricDefinition||!parametricDefinitionCurrent(id)){
        setError(error,"Choose a body with a current editable parametric definition.");return false;
    }
    if(!validateDesignConfigurations(*part,part->designConfigurations,error))return false;
    const auto at=std::find_if(part->designConfigurations.cbegin(),part->designConfigurations.cend(),
        [&](const SavedDesignConfiguration& candidate){return candidate.name==name;});
    if(at==part->designConfigurations.cend()){
        setError(error,"Selected named configuration is not stored in this project.");return false;
    }
    std::map<std::string,study::Quantity> baseline;
    std::map<std::string,study::Dimension> dimensions;
    std::map<std::string,std::string> edits;
    for(auto it=part->namedParameters.cbegin();it!=part->namedParameters.cend();++it){
        const auto dimension=it.key()==QStringLiteral("hole_pattern_count")?study::scalar:study::length;
        baseline.emplace(it.key().toStdString(),study::Quantity{it.value(),dimension});
        dimensions.emplace(it.key().toStdString(),dimension);
    }
    for(auto it=at->expressions.cbegin();it!=at->expressions.cend();++it)
        edits.emplace(it.key().toStdString(),it.value().toStdString());
    std::map<std::string,study::Quantity> evaluated;
    std::string detail;
    if(!study::evaluateParameterExpressions(baseline,edits,dimensions,&evaluated,&detail)){
        setError(error,QString::fromStdString(detail));return false;
    }
    QMap<QString,double> edited;
    for(auto it=at->expressions.cbegin();it!=at->expressions.cend();++it)
        edited.insert(it.key(),evaluated.at(it.key().toStdString()).value);
    return setNamedParametersWithLinks(id,edited,part->parameterLinks,true,error,nullptr,&name);
}

bool SceneDocument::compareDesignConfigurations(const QString& partId,
                  QVector<ConfigurationBomComparison>* output,QString* error) const {
    const Part* source=find(partId);
    if(!output||!source||!source->hasParametricDefinition||
       !parametricDefinitionCurrent(partId)){
        setError(error,"Configuration/BOM comparison requires a current parametric body and result buffer.");
        return false;
    }
    if(!validateDesignConfigurations(*source,source->designConfigurations,error))return false;
    if(source->designConfigurations.isEmpty()){
        setError(error,"Save at least one named configuration before comparing its geometry and BOM.");
        return false;
    }
    QVector<ConfigurationBomComparison> prepared;
    prepared.reserve(source->designConfigurations.size()+1);
    const auto addRow=[&](const SceneDocument& trial,const QString& name,bool current)->bool {
        const Part* target=trial.find(partId);
        if(!target||!trial.parametricDefinitionCurrent(partId)){
            setError(error,QStringLiteral("Configuration %1 lost its parametric source.").arg(name));return false;
        }
        BodyDiagnostics body;
        QString detail;
        if(!trial.diagnoseBody(partId,&body,&detail)||!body.valid){
            setError(error,QStringLiteral("Configuration %1 has invalid target geometry: %2").arg(name,detail));
            return false;
        }
        const auto bom=trial.billOfMaterials(&detail);
        if(bom.isEmpty()){
            setError(error,QStringLiteral("Configuration %1 could not generate a valid BOM: %2").arg(name,detail));
            return false;
        }
        ConfigurationBomComparison row;
        row.name=name;row.current=current;row.targetVolumeMm3=body.volume;
        row.allMassesKnown=true;row.bomLines=bom.size();
        for(const BillOfMaterialsRow& item:bom){
            row.totalAssemblyVolumeMm3+=item.totalVolumeMm3;
            row.knownAssemblyMassKg+=item.totalMassKg;
            row.componentQuantity+=item.quantity;
            if(item.quantity<=0||!std::isfinite(item.totalVolumeMm3)||
               !std::isfinite(item.totalMassKg)||item.totalVolumeMm3<0||item.totalMassKg<0){
                setError(error,QStringLiteral("Configuration %1 produced an invalid BOM quantity or measurement.")
                            .arg(name));return false;
            }
            // Zero mass for a nonempty material-bearing solid means density is
            // unknown; never present a partial mass as a complete assembly mass.
            if(item.totalVolumeMm3>0&&item.unitMassKg<=0)row.allMassesKnown=false;
        }
        if(!std::isfinite(row.targetVolumeMm3)||row.targetVolumeMm3<0||
           !std::isfinite(row.totalAssemblyVolumeMm3)||
           !std::isfinite(row.knownAssemblyMassKg)){
            setError(error,"Nonfinite configuration/BOM totals.");return false;
        }
        prepared.append(std::move(row));return true;
    };
    if(!addRow(*this,QStringLiteral("CURRENT"),true))return false;
    for(const auto& variant:source->designConfigurations){
        SceneDocument trial;
        // Lightweight read-only snapshot. No open document, file, undo history
        // or B-rep cache is changed; an internal checkpoint only affects trial.
        trial.parts_=parts_;
        trial.liveRigidMateFixed_=liveRigidMateFixed_;
        trial.liveRigidMates_=liveRigidMates_;
        trial.liveArticulatedFixed_=liveArticulatedFixed_;
        trial.liveArticulatedJoints_=liveArticulatedJoints_;
        trial.liveArticulatedReferencePoses_=liveArticulatedReferencePoses_;
        trial.drawingReadinessPolicy_=drawingReadinessPolicy_;
        QString issue;
        if(!trial.applyDesignConfiguration(partId,variant.name,&issue)){
            setError(error,QStringLiteral("Configuration %1 could not rebuild: %2").arg(variant.name,issue));
            return false;
        }
        if(!addRow(trial,variant.name,false))return false;
    }
    *output=std::move(prepared);
    if(error)error->clear();
    return true;
}

bool SceneDocument::buildConfigurationComparisonCsv(const QString& partId,
                                                      QByteArray* output,QString* error) const{
    if(!output){setError(error,"Provide a configuration comparison CSV destination.");return false;}
    QVector<ConfigurationBomComparison> rows;
    if(!compareDesignConfigurations(partId,&rows,error))return false;
    auto csv=[](QString value){
        if(!value.trimmed().isEmpty()&&QStringLiteral("=+-@\t\r").contains(value.trimmed().front()))
            value.prepend(QChar(0x27));
        value.replace('"',QStringLiteral("\"\""));
        return QStringLiteral("\"")+value+QStringLiteral("\"");
    };
    const auto real=[](double value){return QString::number(value,'g',15);};
    QString contents=QStringLiteral("CADWorkbench configuration/BOM comparison,GEOMETRY DESIGN REVIEW - NOT PRODUCTION APPROVAL\r\n");
    contents+=QStringLiteral("Source body,")+csv(partId)+QStringLiteral("\r\n");
    contents+=QStringLiteral("Source geometry revision,")+csv(find(partId)->geometryToken)+QStringLiteral("\r\n");
    QVector<const Part*> assembly;
    assembly.reserve(parts_.size());
    for(const Part& part:parts_)assembly.append(&part);
    contents+=QStringLiteral("Assembly geometry fingerprint,")+csv(drawingSourceFingerprint(assembly))+
              QStringLiteral("\r\n");
    contents+=QStringLiteral("BOM/material fingerprint,")+csv(drawingSourceDataFingerprint(assembly))+
              QStringLiteral("\r\n");
    contents+=QStringLiteral("Configuration,Is current,Target volume (mm3),Assembly volume (mm3),Known mass (kg),Complete mass data,BOM lines,Total quantity\r\n");
    for(const auto& row:rows){
        contents+=csv(row.name)+','+(row.current?"YES":"NO")+','+real(row.targetVolumeMm3)+','+
            real(row.totalAssemblyVolumeMm3)+','+real(row.knownAssemblyMassKg)+','+
            (row.allMassesKnown?"YES":"NO")+','+QString::number(row.bomLines)+','+
            QString::number(row.componentQuantity)+QStringLiteral("\r\n");
    }
    *output=contents.toUtf8();if(error)error->clear();return true;
}

bool SceneDocument::exportConfigurationComparisonCsv(const QString& partId,
                                                    const QString& path,QString* error) const{
    if(path.trimmed().isEmpty()){setError(error,"Choose a report filename.");return false;}
    QByteArray bytes;
    if(!buildConfigurationComparisonCsv(partId,&bytes,error))return false;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){
        setError(error,QStringLiteral("Could not atomically write configuration comparison: %1").arg(file.errorString()));
        return false;
    }
    if(error)error->clear();return true;
}

bool SceneDocument::setNamedParameter(const QString& partId,const QString& parameterName,double value,QString* error) {
    return setNamedParameters(partId,QMap<QString,double>{{parameterName,value}},error);
}

bool SceneDocument::setNamedParameters(const QString& partId,const QMap<QString,double>& values,QString* error) {
    return setNamedParametersWithLinks(partId,values,parameterLinks(partId),false,error);
}

bool SceneDocument::setNamedParametersWithLinks(const QString& partId,const QMap<QString,double>& values,
        const QMap<QString,ParameterLink>& links,bool allowEmpty,QString* error,
        const QMap<QString,QString>* replacementFormulas,
        const QString* selectedConfiguration) {
    int index=-1;
    for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Object not found.");return false;}
    const Part& current=parts_[index];
    if(!current.hasParametricDefinition){setError(error,"The selected body has no editable parametric feature history.");return false;}
    if(current.geometryToken!=current.parametricGeometryToken){setError(error,"This body has geometry edits after its parametric feature history. The old parameters are read-only; rebuild a new parametric feature before editing dimensions.");return false;}
    if(values.isEmpty()&&!allowEmpty){setError(error,"Provide at least one named parameter value to edit.");return false;}
    QMap<QString,double> updated=current.namedParameters;
    QMap<QString,QString> formulas=replacementFormulas?*replacementFormulas:current.namedParameterFormulas;
    if(replacementFormulas){
        for(auto it=formulas.cbegin();it!=formulas.cend();++it){
            if(links.contains(it.key())){
                setError(error,"A driven parameter cannot also have an editable dimension expression.");return false;
            }
        }
    }else{
        for(auto it=values.cbegin();it!=values.cend();++it)formulas.remove(it.key());
        for(auto it=links.cbegin();it!=links.cend();++it)formulas.remove(it.key());
    }
    for(auto it=values.cbegin();it!=values.cend();++it){
        if(!updated.contains(it.key())){setError(error,"The named parameter does not exist on this feature history.");return false;}
        if(links.contains(it.key())){
            setError(error,"A driven named parameter must be edited through its source or its dependency removed.");return false;
        }
        const bool coordinate=it.key()==QStringLiteral("origin_x")||it.key()==QStringLiteral("origin_y")||
            it.key()==QStringLiteral("hole_x")||it.key()==QStringLiteral("hole_y");
        const bool patternCount=it.key()==QStringLiteral("hole_pattern_count");
        if(patternCount?(!std::isfinite(it.value())||it.value()<2||it.value()>100||std::floor(it.value())!=it.value()):
           (coordinate?!finiteOffset(it.value()):!finitePositive(it.value()))){setError(error,"Named dimensions must be finite and positive; origins and hole centers may range from -1,000,000 to 1,000,000 mm, and pattern count must be an integer from 2 to 100.");return false;}
        updated[it.key()]=it.value();
    }
    if(!resolveParameterLinks(links,updated,error))return false;
    QMap<QString,double> computed;
    if(!evaluatePersistentDimensionFormulas(updated,formulas,&computed,error))return false;
    updated=std::move(computed);
    if(!resolveParameterLinks(links,updated,error))return false;
    for(auto it=updated.cbegin();it!=updated.cend();++it){
        const bool coordinate=it.key()==QStringLiteral("origin_x")||it.key()==QStringLiteral("origin_y")||
            it.key()==QStringLiteral("hole_x")||it.key()==QStringLiteral("hole_y");
        const bool count=it.key()==QStringLiteral("hole_pattern_count");
        if(count?(!std::isfinite(it.value())||it.value()<2||it.value()>100||std::floor(it.value())!=it.value()):
           coordinate?!finiteOffset(it.value()):!finitePositive(it.value())){
            setError(error,"Reevaluated dimension expressions produced an invalid physical parameter.");return false;
        }
    }
    const QString selected=selectedConfiguration?*selectedConfiguration:QString{};
    const bool geometryChanges=current.namedParameters!=updated;
    const bool formulasChange=current.namedParameterFormulas!=formulas;
    const bool selectionChanges=current.activeDesignConfiguration!=selected;
    if(!geometryChanges&&!formulasChange&&!selectionChanges)return true;
    if(!geometryChanges){
        checkpoint();parts_[index].namedParameterFormulas=std::move(formulas);
        parts_[index].activeDesignConfiguration=selected;return true;
    }
    try{
        TopoDS_Shape rebuilt;
        if(!rebuildParametricSolid(current,updated,current.suppressedParametricFeatures,&rebuilt,error))return false;
        QVector<Part> candidate=parts_;
        candidate[index].shape=rebuilt;candidate[index].geometryToken=generatedId();
        candidate[index].parametricGeometryToken=candidate[index].geometryToken;
        candidate[index].namedParameters=std::move(updated);
        candidate[index].namedParameterFormulas=std::move(formulas);
        candidate[index].activeDesignConfiguration=selected;
        if(!propagateLinkedBodies(&candidate,error))return false;
        checkpoint();parts_=std::move(candidate);return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Parametric feature rebuild failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

QMap<QString,double> SceneDocument::namedParameters(const QString& partId) const {
    const Part* part=find(partId);return part&&part->hasParametricDefinition?part->namedParameters:QMap<QString,double>{};
}
QMap<QString,QString> SceneDocument::namedParameterFormulas(const QString& partId) const {
    const Part* part=find(partId);return part&&part->hasParametricDefinition?
        part->namedParameterFormulas:QMap<QString,QString>{};
}
bool SceneDocument::previewNamedParameterFormulas(const QString& partId,
        const QMap<QString,QString>& formulas,QString* report,QString* error) const{
    if(!report){setError(error,"Provide an output report for the formula geometry preview.");return false;}
    const Part* source=find(partId);
    if(!source||!source->hasParametricDefinition){
        setError(error,"Select a valid parametric body for formula preview.");return false;
    }
    // SceneDocument contains value snapshots, shared immutable OCCT input
    // handles, and no QObject or live UI pointers. A detached copy invokes
    // the SAME checked geometry and linked-body transaction as Save.
    SceneDocument staged=*this;
    if(!staged.setNamedParameterFormulas(partId,formulas,error))return false;
    QStringList lines{"READ-ONLY PARAMETRIC FORMULA PREFLIGHT: complete staged regeneration"};
    int changed=0;
    for(const Part& original:parts_){
        const Part* next=staged.find(original.id);
        if(!next){setError(error,"Staged formula rebuild lost a source body.");return false;}
        if(original.geometryToken==next->geometryToken)continue;
        ++changed;
        if(next->shape.IsNull()||!validShape(next->shape,error))return false;
        if(!next->isSurfaceBody){
            GProp_GProps mass;BRepGProp::VolumeProperties(next->shape,mass);
            if(!std::isfinite(mass.Mass())||mass.Mass()<=0.0){
                setError(error,"Staged parameter rebuild produced a nonpositive solid volume.");return false;
            }
            lines << QString("%1: new volume %2 mm^3")
                .arg(next->name).arg(mass.Mass(),0,'g',12);
        }else{
            lines << QString("%1: surface geometry regenerated").arg(next->name);
        }
    }
    lines.prepend(QString("Rebuilt bodies: %1; no saved geometry or revision was modified.")
                  .arg(changed));
    *report=lines.join('\n');
    if(error)error->clear();return true;
}

bool SceneDocument::setNamedParameterFormulas(const QString& partId,
        const QMap<QString,QString>& formulas,QString* error){
    // The graph and complete B-rep are validated and committed together.
    return setNamedParametersWithLinks(partId,{},parameterLinks(partId),true,error,&formulas);
}
bool SceneDocument::previewNamedParameterBatch(
        const QMap<QString,QMap<QString,double>>& changes,QString* report,QString* error) const{
    if(!report||changes.isEmpty()||changes.size()>12){
        setError(error,"Provide a report and between 1 and 12 edited parametric bodies.");return false;
    }
    SceneDocument staged=*this;
    QStringList lines{"COORDINATED PARAMETRIC CHANGE REVIEW (detached full-scene geometry)",
        QString("Requested source bodies: %1").arg(changes.size())};
    for(auto it=changes.cbegin();it!=changes.cend();++it){
        if(it.value().isEmpty()){
            setError(error,QString("No dimensions were supplied for body %1.").arg(it.key()));return false;
        }
    }
    if(changes.size()==1){
        const auto it=changes.cbegin();
        if(!staged.setNamedParameters(it.key(),it.value(),error))return false;
    }else if(!staged.setCoordinatedParametricRevision(changes,{},error))return false;
    int rebuilt=0;
    for(const Part& before:parts_){
        const Part* after=staged.find(before.id);
        if(!after){setError(error,"Detached batch lost a body.");return false;}
        if(before.geometryToken==after->geometryToken)continue;
        ++rebuilt;
        if(after->shape.IsNull()||!validShape(after->shape,error))return false;
        if(after->isSurfaceBody){
            lines<<QString("%1: surface rebuilt").arg(after->name);continue;
        }
        GProp_GProps volume;
        BRepGProp::VolumeProperties(after->shape,volume);
        if(!std::isfinite(volume.Mass())||volume.Mass()<=1e-9){
            setError(error,"A staged dependent body has nonpositive volume.");return false;
        }
        lines<<QString("%1: rebuilt solid %2 mm^3").arg(after->name).arg(volume.Mass(),0,'g',14);
    }
    lines<<QString("Total rebuilt source/dependent bodies: %1. No saved scene or undo history changed.").arg(rebuilt);
    *report=lines.join('\n');
    if(error)error->clear();return true;
}

bool SceneDocument::setNamedParameterBatch(
        const QMap<QString,QMap<QString,double>>& changes,QString* error){
    if(changes.isEmpty()||changes.size()>12){
        setError(error,"Select 1-12 parametric bodies for a single coordinated commit.");return false;
    }
    for(auto it=changes.cbegin();it!=changes.cend();++it){
        if(it.value().isEmpty()){
            setError(error,QString("No dimensions were supplied for body %1.").arg(it.key()));return false;
        }
    }
    // Before Sprint 14, this path could rebuild every dependency cone twice
    // for preview and twice for commit. An N-source edit could also fail on a
    // transient intermediate solid. Reuse the already-transactional FINAL-
    // STATE regeneration, with one dependent propagation and one undo step.
    if(changes.size()==1){
        const auto it=changes.cbegin();
        return setNamedParameters(it.key(),it.value(),error);
    }
    return setCoordinatedParametricRevision(changes,{},error);
}

bool SceneDocument::evaluateParametricDesignAlternatives(
        const QVector<ParametricDesignAlternative>& alternatives,
        QVector<ParametricDesignAlternativeResult>* results,QString* error) const{
    if(!results||alternatives.isEmpty()||alternatives.size()>8){
        setError(error,"Choose 1-8 alternatives and a separate results buffer.");return false;
    }
    QSet<QString> labels;
    for(const auto& alternative:alternatives){
        const QString label=alternative.label.trimmed();
        if(label.isEmpty()||labels.contains(label)||alternative.changes.size()>12){
            setError(error,"Alternative labels must be unique/nonempty, with up to 12 changed bodies.");return false;
        }
        labels.insert(label);
    }
    const auto measure=[&](const SceneDocument& document,
            double* volume,double* knownMass)->bool{
        double sum=0,mass=0;
        for(const Part& part:document.parts_){
            if(part.isSurfaceBody)continue;
            if(part.shape.IsNull()||!validShape(part.shape,nullptr))return false;
            GProp_GProps properties;BRepGProp::VolumeProperties(part.shape,properties);
            const double v=properties.Mass();
            if(!std::isfinite(v)||v<=1e-9)return false;
            sum+=v;
            if(part.materialDensityKgPerM3>0&&std::isfinite(part.materialDensityKgPerM3))
                mass+=v*1e-9*part.materialDensityKgPerM3;
        }
        *volume=sum;*knownMass=mass;return std::isfinite(sum)&&std::isfinite(mass);
    };
    double baselineVolume=0,baselineMass=0;
    if(!measure(*this,&baselineVolume,&baselineMass)){
        setError(error,"Current design contains an invalid or non-measurable solid.");return false;
    }
    QVector<ParametricDesignAlternativeResult> stagedResults;
    stagedResults.reserve(alternatives.size());
    for(const auto& alternative:alternatives){
        ParametricDesignAlternativeResult row;row.label=alternative.label.trimmed();
        SceneDocument candidate=*this;
        QString issue;
        if(!alternative.changes.isEmpty()&&
           !candidate.setNamedParameterBatch(alternative.changes,&issue)){
            row.failure=issue.isEmpty()?"Candidate dependent-body rebuild failed.":issue;
        }else{
            double volume=0,mass=0;
            if(!measure(candidate,&volume,&mass)){
                row.failure="Candidate geometry is invalid or has nonpositive volume.";
            }else{
                row.valid=true;row.totalSolidVolumeMm3=volume;row.knownMassKg=mass;
                row.volumeChangePercent=baselineVolume>1e-9?
                    (volume-baselineVolume)*100.0/baselineVolume:0.0;
                for(const Part& part:parts_){
                    const Part* changed=candidate.find(part.id);
                    if(!changed)continue;
                    if(changed->geometryToken!=part.geometryToken||
                       changed->namedParameters!=part.namedParameters){
                        ++row.rebuiltBodies;
                        // Always inspect actual OCCT B-reps. Geometry tokens
                        // alone cannot tell us when a dependent feature gained
                        // or lost a face/edge during regeneration.
                        if(!part.isSurfaceBody){
                            ParametricBodyImpact impact;
                            impact.bodyId=part.id;impact.bodyName=part.name;
                            GProp_GProps beforeVolume,afterVolume;
                            BRepGProp::VolumeProperties(part.shape,beforeVolume);
                            BRepGProp::VolumeProperties(changed->shape,afterVolume);
                            impact.beforeVolumeMm3=beforeVolume.Mass();
                            impact.afterVolumeMm3=afterVolume.Mass();
                            for(TopExp_Explorer e(part.shape,TopAbs_FACE);e.More();e.Next())++impact.beforeFaces;
                            for(TopExp_Explorer e(changed->shape,TopAbs_FACE);e.More();e.Next())++impact.afterFaces;
                            for(TopExp_Explorer e(part.shape,TopAbs_EDGE);e.More();e.Next())++impact.beforeEdges;
                            for(TopExp_Explorer e(changed->shape,TopAbs_EDGE);e.More();e.Next())++impact.afterEdges;
                            impact.topologyCountChanged=(impact.beforeFaces!=impact.afterFaces||
                                impact.beforeEdges!=impact.afterEdges);
                            if(impact.topologyCountChanged)++row.topologyCountChangedBodies;
                            row.bodyImpacts.append(std::move(impact));
                        }
                    }
                }
            }
        }
        stagedResults.append(std::move(row));
    }
    *results=std::move(stagedResults);
    if(error)error->clear();return true;
}

QString SceneDocument::designGeometryFingerprint() const{
    // A candidate depends on the source parameter/formula graph AS WELL AS the
    // visible B-rep. Formula-only changes can leave a geometry token equal;
    // they still invalidate an earlier engineering design-choice evaluation.
    QVector<const Part*> ordered;ordered.reserve(parts_.size());
    for(const Part& part:parts_)ordered.append(&part);
    std::sort(ordered.begin(),ordered.end(),[](const Part* a,const Part* b){return a->id<b->id;});
    QJsonArray bodies;
    for(const Part* part:ordered){
        QJsonObject record;
        record.insert(QStringLiteral("id"),part->id);
        record.insert(QStringLiteral("geometry"),part->geometryToken);
        record.insert(QStringLiteral("parametric_geometry"),part->parametricGeometryToken);
        record.insert(QStringLiteral("current_configuration"),part->activeDesignConfiguration);
        QJsonObject dimensions,formulas,links;
        for(auto it=part->namedParameters.cbegin();it!=part->namedParameters.cend();++it)
            dimensions.insert(it.key(),it.value());
        for(auto it=part->namedParameterFormulas.cbegin();it!=part->namedParameterFormulas.cend();++it)
            formulas.insert(it.key(),it.value());
        for(auto it=part->parameterLinks.cbegin();it!=part->parameterLinks.cend();++it){
            QJsonObject link;
            link.insert(QStringLiteral("source"),it.value().source);
            link.insert(QStringLiteral("scale"),it.value().scale);
            link.insert(QStringLiteral("offset"),it.value().offset);
            links.insert(it.key(),link);
        }
        QJsonArray suppressed;
        for(const QString& feature:part->suppressedParametricFeatures)suppressed.append(feature);
        record.insert(QStringLiteral("parameters"),dimensions);
        record.insert(QStringLiteral("formulas"),formulas);
        record.insert(QStringLiteral("links"),links);
        record.insert(QStringLiteral("suppressed"),suppressed);
        bodies.append(record);
    }
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(bodies).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
}

bool SceneDocument::applyParametricDesignAlternative(
        const ParametricDesignAlternative& selected,
        const QString& expectedGeometryFingerprint,QString* error){
    if(selected.label.trimmed().isEmpty()||selected.changes.isEmpty()||
       selected.changes.size()>12){
        setError(error,"Select a named design alternative affecting 1-12 parametric bodies.");return false;
    }
    if(expectedGeometryFingerprint.isEmpty()||
       expectedGeometryFingerprint!=designGeometryFingerprint()){
        setError(error,"The design changed after this alternative was evaluated. Re-evaluate before applying.");return false;
    }
    for(auto it=selected.changes.cbegin();it!=selected.changes.cend();++it){
        if(it.value().isEmpty()){
            setError(error,"Every selected body must have at least one proposed dimension.");return false;
        }
    }
    // One checked commit against the final state, not another independent
    // interpretation of the candidate preview. All linked dependents are
    // staged by the existing coordinated parametric regeneration engine.
    if(selected.changes.size()==1){
        const auto it=selected.changes.cbegin();
        return setNamedParameters(it.key(),it.value(),error);
    }
    return setCoordinatedParametricRevision(selected.changes,{},error);
}

QStringList SceneDocument::suppressedParametricFeatures(const QString& partId) const {
    const Part* part=find(partId);return part&&part->hasParametricDefinition?part->suppressedParametricFeatures:QStringList{};
}
static bool optionalParametricFeaturePresent(const Part& part,const QString& featureId){
    if(featureId==QStringLiteral("through_hole"))
        return part.featureHistory.contains(QStringLiteral("Cut: Parametric through-hole"));
    if(featureId==QStringLiteral("blind_hole"))
        return part.featureHistory.contains(QStringLiteral("Cut: Parametric blind hole"));
    if(featureId==QStringLiteral("linear_hole_pattern"))
        return part.featureHistory.contains(QStringLiteral("Pattern: Linear through-hole array along X"))||
               part.featureHistory.contains(QStringLiteral("Pattern: Linear through-hole array along Y"))||
               part.featureHistory.contains(QStringLiteral("Pattern: Linear blind-hole array along X"))||
               part.featureHistory.contains(QStringLiteral("Pattern: Linear blind-hole array along Y"));
    if(featureId==QStringLiteral("edge_chamfer"))
        return part.featureHistory.contains(QStringLiteral("Chamfer: All edges"));
    if(featureId==QStringLiteral("edge_fillet"))
        return part.featureHistory.contains(QStringLiteral("Fillet: All edges"));
    return false;
}
bool SceneDocument::setParametricFeatureSuppressed(const QString& partId,const QString& featureId,
                                                   bool suppressed,QString* error) {
    const Part* current=find(partId);
    if(!current){setError(error,"The parametric body was not found.");return false;}
    if(!current->hasParametricDefinition||!parametricDefinitionCurrent(partId)){
        setError(error,"A current editable feature history is required to change feature suppression.");return false;
    }
    if(!optionalParametricFeaturePresent(*current,featureId)){
        setError(error,"The requested optional operation is not present in this parametric feature history.");return false;
    }
    QStringList proposed=current->suppressedParametricFeatures;
    if(suppressed){if(!proposed.contains(featureId))proposed.append(featureId);}
    else proposed.removeAll(featureId);
    return setParametricFeaturesSuppressed(partId,proposed,error);
}
bool SceneDocument::setParametricFeaturesSuppressed(const QString& partId,const QStringList& featureIds,
                                                    QString* error) {
    int index=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"The parametric body was not found.");return false;}
    const Part& current=parts_[index];
    if(!current.hasParametricDefinition||!parametricDefinitionCurrent(partId)){
        setError(error,"A current editable feature history is required to change feature suppression.");return false;
    }
    QStringList proposed=featureIds;
    proposed.sort();
    for(int i=0;i<proposed.size();++i){
        if(!optionalParametricFeaturePresent(current,proposed[i])||
           (i>0&&proposed[i]==proposed[i-1])){
            setError(error,"The suppression list contains an unavailable or duplicate optional feature.");return false;
        }
    }
    if(proposed==current.suppressedParametricFeatures)return true;
    try{
        TopoDS_Shape rebuilt;
        if(!rebuildParametricSolid(current,current.namedParameters,proposed,&rebuilt,error))return false;
        QVector<Part> candidate=parts_;
        Part& updated=candidate[index];updated.shape=rebuilt;
        updated.geometryToken=generatedId();
        updated.parametricGeometryToken=updated.geometryToken;
        updated.suppressedParametricFeatures=std::move(proposed);
        if(!propagateLinkedBodies(&candidate,error))return false;
        checkpoint();parts_=std::move(candidate);
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Feature suppression rebuild failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}
bool SceneDocument::setCoordinatedParametricRevision(
        const QMap<QString,QMap<QString,double>>& dimensions,
        const QMap<QString,QStringList>& suppression,QString* error){
    QSet<QString> selected;
    for(auto it=dimensions.cbegin();it!=dimensions.cend();++it)selected.insert(it.key());
    for(auto it=suppression.cbegin();it!=suppression.cend();++it)selected.insert(it.key());
    if(selected.size()<2||selected.size()>12){
        setError(error,"Select 2-12 different parametric bodies for the coordinated revision.");return false;
    }
    QVector<Part> candidate=parts_;
    bool modified=false;
    try{
        for(const QString& id:selected){
            auto it=std::find_if(candidate.begin(),candidate.end(),
                [&](const Part& item){return item.id==id;});
            if(it==candidate.end()||!it->hasParametricDefinition||
               !parametricDefinitionCurrent(id)){
                setError(error,"Each selected body needs a current editable parametric definition.");return false;
            }
            Part& part=*it;
            QMap<QString,double> values=part.namedParameters;
            QMap<QString,QString> formulas=part.namedParameterFormulas;
            const auto requests=dimensions.value(id);
            for(auto change=requests.cbegin();change!=requests.cend();++change){
                if(!values.contains(change.key())||part.parameterLinks.contains(change.key())){
                    setError(error,QString("Unknown or linked dimension %1 in %2.").arg(change.key(),id));
                    return false;
                }
                const bool coordinate=change.key()==QStringLiteral("origin_x")||
                    change.key()==QStringLiteral("origin_y")||
                    change.key()==QStringLiteral("hole_x")||
                    change.key()==QStringLiteral("hole_y");
                const bool count=change.key()==QStringLiteral("hole_pattern_count");
                const double value=change.value();
                if(count?(!std::isfinite(value)||value<2||value>100||std::floor(value)!=value):
                    (coordinate?!finiteOffset(value):!finitePositive(value))){
                    setError(error,QString("Invalid value for %1 in %2.").arg(change.key(),id));return false;
                }
                values[change.key()]=value;
                formulas.remove(change.key());
            }
            if(!resolveParameterLinks(part.parameterLinks,values,error))return false;
            QMap<QString,double> computed;
            if(!evaluatePersistentDimensionFormulas(values,formulas,&computed,error))return false;
            values=std::move(computed);
            if(!resolveParameterLinks(part.parameterLinks,values,error))return false;
            for(auto entry=values.cbegin();entry!=values.cend();++entry){
                const bool position=entry.key()==QStringLiteral("origin_x")||
                    entry.key()==QStringLiteral("origin_y")||
                    entry.key()==QStringLiteral("hole_x")||entry.key()==QStringLiteral("hole_y");
                const bool count=entry.key()==QStringLiteral("hole_pattern_count");
                const double v=entry.value();
                if(count?(!std::isfinite(v)||v<2||v>100||std::floor(v)!=v):
                    (position?!finiteOffset(v):!finitePositive(v))){
                    setError(error,"Calculated dimension is outside the valid geometry domain.");return false;
                }
            }
            QStringList suppressed=suppression.contains(id)?suppression.value(id):part.suppressedParametricFeatures;
            suppressed.sort();
            for(int i=0;i<suppressed.size();++i){
                if((i>0&&suppressed[i-1]==suppressed[i])||
                    !optionalParametricFeaturePresent(part,suppressed[i])){
                    setError(error,QString("Duplicate or missing optional feature in %1.").arg(id));return false;
                }
            }
            if(values==part.namedParameters&&formulas==part.namedParameterFormulas&&
               suppressed==part.suppressedParametricFeatures)continue;
            // One combined rebuild: the requested final state need not pass
            // through an intermediate dimension-only or suppression-only B-rep.
            TopoDS_Shape solid;
            if(!rebuildParametricSolid(part,values,suppressed,&solid,error)||
               solid.IsNull()||!validShape(solid,error))return false;
            part.shape=solid;
            part.geometryToken=generatedId();part.parametricGeometryToken=part.geometryToken;
            part.namedParameters=std::move(values);
            part.namedParameterFormulas=std::move(formulas);
            part.suppressedParametricFeatures=std::move(suppressed);
            part.activeDesignConfiguration.clear();
            modified=true;
        }
        if(!modified){if(error)error->clear();return true;}
        if(!propagateLinkedBodies(&candidate,error))return false;
        // Nothing is changed in this document until every source AND dependent
        // has been validated against the final configuration.
        for(const Part& body:candidate){
            if(body.shape.IsNull()){setError(error,"A staged dependent body has no geometry.");return false;}
        }
        checkpoint();parts_=std::move(candidate);
        if(error)error->clear();return true;
    }catch(const Standard_Failure& failure){
        setError(error,QString("Coordinated feature revision kernel failure: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QString("Coordinated revision failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::previewCoordinatedParametricRevision(
        const QMap<QString,QMap<QString,double>>& dimensions,
        const QMap<QString,QStringList>& suppression,
        QString* report,QString* error) const{
    if(!report){setError(error,"Provide a separate review report destination.");return false;}
    SceneDocument staged=*this;
    if(!staged.setCoordinatedParametricRevision(dimensions,suppression,error))return false;
    QStringList lines{"COORDINATED DIMENSION + FEATURE-SUPPRESSION PREVIEW",
        "One final-state rebuild per source; linked descendants regenerated; live model unchanged."};
    int altered=0,topologyChanges=0,linkedChanges=0;
    for(const Part& before:parts_){
        const Part* after=staged.find(before.id);
        if(!after){setError(error,"An original scene body disappeared in the staged rebuild.");return false;}
        if(before.geometryToken==after->geometryToken)continue;
        ++altered;
        if(!validShape(after->shape,error))return false;
        const bool source=dimensions.contains(after->id)||suppression.contains(after->id);
        if(!source)++linkedChanges;
        if(after->isSurfaceBody){
            lines<<after->name+": valid rebuilt surface (physical identity not inferred)";
            continue;
        }
        if(!validShape(before.shape,error))return false;
        GProp_GProps previous,properties;
        BRepGProp::VolumeProperties(before.shape,previous);
        BRepGProp::VolumeProperties(after->shape,properties);
        if(!std::isfinite(previous.Mass())||previous.Mass()<=1e-9||
           !std::isfinite(properties.Mass())||properties.Mass()<=1e-9){
            setError(error,"A staged solid has invalid or zero volume.");return false;
        }
        int oldFaces=0,oldEdges=0,faces=0,edges=0;
        for(TopExp_Explorer f(before.shape,TopAbs_FACE);f.More();f.Next())++oldFaces;
        for(TopExp_Explorer e(before.shape,TopAbs_EDGE);e.More();e.Next())++oldEdges;
        for(TopExp_Explorer f(after->shape,TopAbs_FACE);f.More();f.Next())++faces;
        for(TopExp_Explorer e(after->shape,TopAbs_EDGE);e.More();e.Next())++edges;
        const bool countChanged=oldFaces!=faces||oldEdges!=edges;
        if(countChanged)++topologyChanges;
        const double percent=(properties.Mass()-previous.Mass())*100.0/previous.Mass();
        lines<<QString("%1 [%2]: volume %3 -> %4 mm3 (%5%), faces %6 -> %7, edges %8 -> %9 [%10]%11")
            .arg(after->name,after->id)
            .arg(previous.Mass(),0,'g',14).arg(properties.Mass(),0,'g',14)
            .arg(percent,0,'g',9).arg(oldFaces).arg(faces).arg(oldEdges).arg(edges)
            .arg(source?"source":"linked descendant")
            .arg(countChanged?"  TOPOLOGY COUNT CHANGED":"");
    }
    lines<<QString("Rebuilt bodies: %1, linked descendants: %2, topology-count changes: %3.")
        .arg(altered).arg(linkedChanges).arg(topologyChanges);
    if(topologyChanges>0)lines<<
        "Review downstream face/edge references after acceptance. Equal topology counts do not prove identity.";
    lines<<"No live geometry, revision or undo change until accepted.";
    *report=lines.join('\n');if(error)error->clear();return true;
}

bool SceneDocument::setParametricFeaturesSuppressedBatch(
        const QMap<QString,QStringList>& byBody,QString* error){
    if(byBody.size()<2||byBody.size()>12){
        setError(error,"Batch optional-feature editing requires 2-12 parametric bodies.");return false;
    }
    QVector<Part> candidate=parts_;
    bool changed=false;
    try{
        // Validate ALL source definitions before rebuilding any geometry. The
        // candidate is detached until the complete linked graph has passed.
        for(auto it=byBody.cbegin();it!=byBody.cend();++it){
            const int index=static_cast<int>(std::distance(candidate.cbegin(),
                std::find_if(candidate.cbegin(),candidate.cend(),[&](const Part& p){return p.id==it.key();})));
            if(index<0||index>=candidate.size()){
                setError(error,"A selected parametric body is missing.");return false;
            }
            Part& part=candidate[index];
            if(!part.hasParametricDefinition||!parametricDefinitionCurrent(part.id)){
                setError(error,"Every selected body needs a current editable parametric history.");return false;
            }
            QStringList proposed=it.value();proposed.sort();
            for(int i=0;i<proposed.size();++i){
                if(!optionalParametricFeaturePresent(part,proposed[i])||
                   (i>0&&proposed[i]==proposed[i-1])){
                    setError(error,"An optional feature is missing or duplicated in the selected body.");return false;
                }
            }
            if(proposed==part.suppressedParametricFeatures)continue;
            TopoDS_Shape rebuilt;
            if(!rebuildParametricSolid(part,part.namedParameters,proposed,&rebuilt,error))return false;
            if(rebuilt.IsNull()||!validShape(rebuilt,error))return false;
            part.shape=rebuilt;
            part.geometryToken=generatedId();
            part.parametricGeometryToken=part.geometryToken;
            part.suppressedParametricFeatures=std::move(proposed);
            changed=true;
        }
        if(!changed){if(error)error->clear();return true;}
        if(!propagateLinkedBodies(&candidate,error))return false;
        // Exactly one checkpoint, including every linked descendant.
        checkpoint();parts_=std::move(candidate);
        if(error)error->clear();return true;
    }catch(const Standard_Failure& e){
        setError(error,QString("Coordinated feature-history rebuild failed: %1").arg(e.what()));
    }catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::previewParametricFeaturesSuppressedBatch(
        const QMap<QString,QStringList>& byBody,QString* report,QString* error) const{
    if(!report){setError(error,"Provide a report buffer for coordinated feature preview.");return false;}
    SceneDocument trial=*this;
    if(!trial.setParametricFeaturesSuppressedBatch(byBody,error))return false;
    QStringList lines{"Coordinated optional-feature history: validated detached geometry"};
    int changedBodies=0;
    for(const Part& before:parts_){
        const Part* after=trial.find(before.id);
        if(!after){setError(error,"A linked descendant disappeared during feature replay.");return false;}
        if(before.geometryToken==after->geometryToken)continue;
        if(before.isSurfaceBody||after->isSurfaceBody||!validShape(after->shape,error)){
            setError(error,"A changed body is not a valid solid.");return false;
        }
        GProp_GProps props;
        BRepGProp::VolumeProperties(after->shape,props);
        const double volume=props.Mass();
        if(!std::isfinite(volume)||volume<=1e-9){
            setError(error,"A rebuilt feature produces a nonpositive or nonfinite volume.");return false;
        }
        lines<<QString("%1 [%2]: validated volume %3 mm^3%4")
            .arg(after->name,after->id).arg(volume,0,'g',14)
            .arg(byBody.contains(after->id)?" (edited source)":" (linked dependent)");
        ++changedBodies;
    }
    lines<<QString("%1 affected bodies. No live geometry or history was modified.").arg(changedBodies);
    *report=lines.join('\n');
    if(error)error->clear();return true;
}

QStringList SceneDocument::sketchConstraints(const QString& partId) const {
    const Part* part=find(partId);return part&&part->hasParametricDefinition?part->sketchConstraints:QStringList{};
}
QStringList SceneDocument::featureHistory(const QString& partId) const {
    const Part* part=find(partId);return part&&part->hasParametricDefinition?part->featureHistory:QStringList{};
}
bool SceneDocument::parametricDefinitionCurrent(const QString& partId) const {
    const Part* part=find(partId);return part&&part->hasParametricDefinition&&part->geometryToken==part->parametricGeometryToken;
}

bool SceneDocument::booleanOperation(BooleanOp op, const QString& firstId, const QString& secondId, QString* error) {
    if (firstId == secondId) { setError(error, "Choose two different solids."); return false; }
    for(const Part& part:parts_)if(part.hasLinkedBoolean&&
       (part.linkedPrimaryId==firstId||part.linkedToolId==firstId||
        part.linkedPrimaryId==secondId||part.linkedToolId==secondId)){
        setError(error,"A selected source has linked Boolean dependents. Use a non-destructive linked Boolean instead.");return false;
    }
    const Part* a = find(firstId);
    const Part* b = find(secondId);
    if (!a || !b) { setError(error, "Both solids must exist."); return false; }
    if(!a->linkedFoldedFlatSourceId.isEmpty()||!b->linkedFoldedFlatSourceId.isEmpty()){
        setError(error,"A linked neutral flat blank cannot be consumed by a destructive Boolean; use an independent copy.");return false;
    }
    if(rejectSurfaceOnlyInput(*a,QStringLiteral("Boolean operations"),error)||
       rejectSurfaceOnlyInput(*b,QStringLiteral("Boolean operations"),error))return false;
    try {
        TopoDS_Shape result;
        if (op == BooleanOp::Fuse) {
            BRepAlgoAPI_Fuse algo(a->shape,b->shape);
            algo.Build();
            if (!algo.IsDone() || algo.HasErrors()) { setError(error, "Boolean union failed."); return false; }
            result = algo.Shape();
        } else if (op == BooleanOp::Cut) {
            BRepAlgoAPI_Cut algo(a->shape,b->shape);
            algo.Build();
            if (!algo.IsDone() || algo.HasErrors()) { setError(error, "Boolean cut failed."); return false; }
            result = algo.Shape();
        } else {
            BRepAlgoAPI_Common algo(a->shape,b->shape);
            algo.Build();
            if (!algo.IsDone() || algo.HasErrors()) { setError(error, "Boolean intersection failed."); return false; }
            result = algo.Shape();
        }
        if (!validShape(result, error)) return false;
        const QString label = op == BooleanOp::Fuse ? "Union" : op == BooleanOp::Cut ? "Cut" : "Intersection";
        const QString name = uniqueName(label);
        const QString color = a->color;
        const QString commonMaterial = (!a->materialName.isEmpty() && a->materialName == b->materialName &&
                                        a->materialDensityKgPerM3 == b->materialDensityKgPerM3 &&
                                        a->materialEngineering.provenance == b->materialEngineering.provenance &&
                                        a->materialEngineering.youngsModulusGPa == b->materialEngineering.youngsModulusGPa)
                                        ? a->materialName : QString();
        const double commonDensity = commonMaterial.isEmpty() ? 0.0 : a->materialDensityKgPerM3;
        const MaterialEngineeringData commonEngineering = commonMaterial.isEmpty()
            ? MaterialEngineeringData{} : a->materialEngineering;
        checkpoint();
        for (int i = parts_.size()-1; i >= 0; --i) {
            if (parts_[i].id == firstId || parts_[i].id == secondId) parts_.removeAt(i);
        }
        Part fused{generatedId(), name, result, true, color};
        if (!commonMaterial.isEmpty()) {
            fused.materialName = commonMaterial;
            fused.materialDensityKgPerM3 = commonDensity;
            fused.materialEngineering = commonEngineering;
        }
        parts_.append(fused);
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::measureMinimumDistance(const QString& firstId,const QString& secondId,
                                           MinimumDistanceResult* result,QString* error) const {
    if (!result) { setError(error,"A distance result is required."); return false; }
    if (firstId==secondId) { setError(error,"Choose two different solids."); return false; }
    const Part* first=find(firstId);
    const Part* second=find(secondId);
    if (!first||!second) { setError(error,"Both solids must exist."); return false; }
    if (first->shape.IsNull()||second->shape.IsNull()) { setError(error,"Both solids must contain geometry."); return false; }
    try {
        BRepExtrema_DistShapeShape measurement(first->shape,second->shape);
        measurement.Perform();
        if (!measurement.IsDone()||measurement.NbSolution()<1) {
            setError(error,"The CAD kernel could not find a closest-point solution.");
            return false;
        }
        const gp_Pnt a=measurement.PointOnShape1(1);
        const gp_Pnt b=measurement.PointOnShape2(1);
        MinimumDistanceResult candidate;
        candidate.distance=measurement.Value();
        candidate.firstX=a.X(); candidate.firstY=a.Y(); candidate.firstZ=a.Z();
        candidate.secondX=b.X(); candidate.secondY=b.Y(); candidate.secondZ=b.Z();
        *result=candidate;
        return true;
    } catch (const Standard_Failure& e) {
        setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));
    } catch (const std::exception& e) {
        setError(error,QString::fromUtf8(e.what()));
    }
    return false;
}

bool SceneDocument::measureInterference(const QString& firstId,const QString& secondId,
                                        InterferenceResult* result,QString* error) const {
    if(!result){setError(error,"An interference result is required.");return false;}
    if(firstId==secondId){setError(error,"Choose two different solids.");return false;}
    const Part* first=find(firstId);const Part* second=find(secondId);
    if(!first||!second){setError(error,"Both solids must exist.");return false;}
    if(first->shape.IsNull()||second->shape.IsNull()){setError(error,"Both solids must contain geometry.");return false;}
    try {
        BRepAlgoAPI_Common common(first->shape,second->shape);
        common.Build();
        if(!common.IsDone()||common.Shape().IsNull()){
            setError(error,"The CAD kernel could not compute the shared volume.");return false;
        }
        GProp_GProps properties;
        BRepGProp::VolumeProperties(common.Shape(),properties);
        InterferenceResult candidate;
        candidate.volume=std::max(0.0,properties.Mass());
        const gp_Pnt center=properties.CentreOfMass();
        candidate.centroidX=center.X();candidate.centroidY=center.Y();candidate.centroidZ=center.Z();
        *result=candidate;
        return true;
    } catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
      catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::diagnoseBody(const QString& id,BodyDiagnostics* result,QString* error) const {
    if(!result){setError(error,"Body diagnostics output is null.");return false;}
    const Part* part=find(id);
    if(!part||part->shape.IsNull()){setError(error,"The selected body does not exist or has no geometry.");return false;}
    try {
        BodyDiagnostics candidate;
        candidate.valid=BRepCheck_Analyzer(part->shape).IsValid();
        const auto count=[&](TopAbs_ShapeEnum type){
            NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> uniqueShapes;
            TopExp::MapShapes(part->shape,type,uniqueShapes);
            return uniqueShapes.Extent();
        };
        candidate.solids=count(TopAbs_SOLID);candidate.shells=count(TopAbs_SHELL);
        candidate.faces=count(TopAbs_FACE);candidate.wires=count(TopAbs_WIRE);
        candidate.edges=count(TopAbs_EDGE);candidate.vertices=count(TopAbs_VERTEX);
        GProp_GProps volumeProperties,areaProperties;
        BRepGProp::VolumeProperties(part->shape,volumeProperties);
        BRepGProp::SurfaceProperties(part->shape,areaProperties);
        candidate.volume=volumeProperties.Mass();
        candidate.surfaceArea=areaProperties.Mass();
        const gp_Pnt center=volumeProperties.CentreOfMass();
        candidate.centerX=center.X();candidate.centerY=center.Y();candidate.centerZ=center.Z();
        Bnd_Box bounds;
        BRepBndLib::AddOptimal(part->shape,bounds,false,false);
        if(bounds.IsVoid()){setError(error,"The selected body has no measurable bounds.");return false;}
        bounds.Get(candidate.minX,candidate.minY,candidate.minZ,
                   candidate.maxX,candidate.maxY,candidate.maxZ);
        candidate.valid=candidate.valid&&candidate.solids>0&&candidate.volume>1e-9&&
            std::isfinite(candidate.volume)&&std::isfinite(candidate.surfaceArea);
        *result=candidate;
        return true;
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Geometry diagnostics failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Geometry diagnostics failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureMassFromDensity(const QString& id,double densityKgPerM3,
                                            DensityMassProperties* result,QString* error) const {
    if(!result){setError(error,"Mass-properties output is null.");return false;}
    if(!finitePositive(densityKgPerM3)){setError(error,"Density must be finite, positive, and no greater than 1,000,000 kg/m^3.");return false;}
    const Part* part=find(id);
    if(!part||part->shape.IsNull()){setError(error,"The selected body does not exist or has no geometry.");return false;}
    try{
        if(!BRepCheck_Analyzer(part->shape).IsValid()){
            setError(error,"Mass properties require valid solid geometry.");return false;
        }
        GProp_GProps volumeProperties;
        BRepGProp::VolumeProperties(part->shape,volumeProperties);
        const double volume=volumeProperties.Mass();
        const double mass=volume*densityKgPerM3/1e9; // mm^3 to m^3
        const gp_Pnt center=volumeProperties.CentreOfMass();
        if(!std::isfinite(volume)||volume<=1e-9||!std::isfinite(mass)){
            setError(error,"The selected body has no measurable positive volume.");return false;
        }
        DensityMassProperties candidate;
        candidate.volume=volume;candidate.densityKgPerM3=densityKgPerM3;candidate.massKg=mass;
        candidate.centerX=center.X();candidate.centerY=center.Y();candidate.centerZ=center.Z();
        *result=candidate;
        return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Mass-property calculation failed: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QString("Mass-property calculation failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureMassTensor(const QString& id,double densityKgPerM3,
                                       tensor::Report* result,QString* error) const {
    if(!result){setError(error,"Mass tensor output is null.");return false;}
    if(!std::isfinite(densityKgPerM3)||densityKgPerM3<=0.0||densityKgPerM3>1e6){
        setError(error,"Density must be finite, positive, and no greater than 1,000,000 kg/m^3.");return false;
    }
    const Part* part=find(id);
    if(!part||part->shape.IsNull()||part->isSurfaceBody){
        setError(error,"Select a solid body to compute its inertia tensor.");return false;
    }
    try {
        if(!BRepCheck_Analyzer(part->shape).IsValid()){
            setError(error,"Mass tensor requires a valid solid.");return false;
        }
        GProp_GProps volume;
        BRepGProp::VolumeProperties(part->shape,volume);
        const gp_Pnt center=volume.CentreOfMass();
        const gp_Mat inertia=volume.MatrixOfInertia(); // About its center of mass, unit volumetric density.
        tensor::Input input;
        input.volumeMm3=volume.Mass();input.densityKgPerM3=densityKgPerM3;
        input.centerMm={center.X(),center.Y(),center.Z()};
        for(int row=0;row<3;++row)for(int col=0;col<3;++col)
            input.geometricInertiaMm5[row*3+col]=inertia.Value(row+1,col+1);
        std::string message;
        if(!tensor::analyze(input,result,&message)){
            setError(error,QString::fromStdString(message));return false;
        }
        return true;
    } catch(const Standard_Failure& exception) {
        setError(error,QStringLiteral("Inertia calculation failed: %1").arg(exception.what()));
    } catch(const std::exception& exception) {
        setError(error,QStringLiteral("Inertia calculation failed: %1").arg(exception.what()));
    }
    return false;
}

bool SceneDocument::measureAssemblyMassTensor(const QStringList& bodyIds,
                                               tensor::Report* result,QString* error) const {
    if(!result){setError(error,"Assembly mass tensor output is null.");return false;}
    if(bodyIds.isEmpty()){setError(error,"Choose at least one physical component.");return false;}
    if(bodyIds.size()>1000){setError(error,"Too many components for one mass calculation.");return false;}
    QSet<QString> seen;
    std::vector<tensor::Report> bodies;
    bodies.reserve(static_cast<size_t>(bodyIds.size()));
    for(const QString& id:bodyIds){
        if(seen.contains(id)){setError(error,"An assembly body was selected more than once.");return false;}
        seen.insert(id);
        const Part* part=find(id);
        if(!part||part->shape.IsNull()||part->isSurfaceBody){
            setError(error,QStringLiteral("Missing or non-solid assembly component: %1").arg(id));return false;
        }
        if(!std::isfinite(part->materialDensityKgPerM3)||part->materialDensityKgPerM3<=0.0){
            setError(error,QStringLiteral("Assign material density before measuring assembly component: %1").arg(part->name));
            return false;
        }
        tensor::Report body;
        if(!measureMassTensor(id,part->materialDensityKgPerM3,&body,error))return false;
        bodies.push_back(body);
    }
    std::string message;
    if(!tensor::aggregate(bodies,result,&message)){
        setError(error,QString::fromStdString(message));return false;
    }
    return true;
}

bool SceneDocument::measurePlanarSection(const QString& id,int normalAxis,double offset,
                                         SectionMeasurement* result,QString* error) const {
    if(!result){setError(error,"Section-measurement output is null.");return false;}
    if(normalAxis<0||normalAxis>2||!std::isfinite(offset)||std::abs(offset)>1e6){
        setError(error,"Section axis must be X, Y, or Z and offset must be finite and within ±1,000,000 mm.");return false;
    }
    const Part* part=find(id);
    if(!part||part->shape.IsNull()){setError(error,"The selected body does not exist or has no geometry.");return false;}
    try{
        const gp_Pnt origin(normalAxis==0?offset:0.0,normalAxis==1?offset:0.0,normalAxis==2?offset:0.0);
        const gp_Dir normal(normalAxis==0?1.0:0.0,normalAxis==1?1.0:0.0,normalAxis==2?1.0:0.0);
        BRepAlgoAPI_Section section(part->shape,gp_Pln(origin,normal),false);
        section.Build();
        if(!section.IsDone()||section.HasErrors()){setError(error,"The kernel could not calculate the planar section.");return false;}
        NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> edges;
        TopExp::MapShapes(section.Shape(),TopAbs_EDGE,edges);
        SectionMeasurement candidate;
        candidate.edgeCount=edges.Extent();
        for(int i=1;i<=edges.Extent();++i){
            GProp_GProps properties;
            BRepGProp::LinearProperties(edges(i),properties);
            candidate.totalLength+=properties.Mass();
        }
        if(!std::isfinite(candidate.totalLength)){setError(error,"Section contour length is not finite.");return false;}
        *result=candidate;
        return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Planar section calculation failed: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QString("Planar section calculation failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::summarizeSurfaceTypes(const QString& id,SurfaceTypeSummary* result,
                                          QString* error) const {
    if(!result){setError(error,"Surface-summary output is null.");return false;}
    const Part* part=find(id);
    if(!part||part->shape.IsNull()){setError(error,"The selected body does not exist or has no geometry.");return false;}
    try {
        SurfaceTypeSummary candidate;
        NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> uniqueFaces;
        TopExp::MapShapes(part->shape,TopAbs_FACE,uniqueFaces);
        for(int faceIndex=1;faceIndex<=uniqueFaces.Extent();++faceIndex){
            const TopoDS_Face face=TopoDS::Face(uniqueFaces(faceIndex));
            const GeomAbs_SurfaceType type=BRepAdaptor_Surface(face,true).GetType();
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(face,properties);
            const double area=properties.Mass();
            if(!std::isfinite(area)||area<0.0){setError(error,"A face has a non-finite surface area.");return false;}
            int* count=&candidate.otherFaces;
            double* totalArea=&candidate.otherArea;
            switch(type){
            case GeomAbs_Plane: count=&candidate.planarFaces;totalArea=&candidate.planarArea;break;
            case GeomAbs_Cylinder: count=&candidate.cylindricalFaces;totalArea=&candidate.cylindricalArea;break;
            case GeomAbs_Cone: count=&candidate.conicalFaces;totalArea=&candidate.conicalArea;break;
            case GeomAbs_Sphere: count=&candidate.sphericalFaces;totalArea=&candidate.sphericalArea;break;
            case GeomAbs_Torus: count=&candidate.toroidalFaces;totalArea=&candidate.toroidalArea;break;
            default: break;
            }
            ++*count;
            *totalArea+=area;
        }
        *result=candidate;
        return true;
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Surface classification failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Surface classification failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureFaceArea(const FaceReference& reference,double* area,QString* error) const {
    if(!area){setError(error,"Face-area output is null.");return false;}
    const Part* part=find(reference.partId);
    if(!part||part->shape.IsNull()){setError(error,"The selected face's body does not exist or has no geometry.");return false;}
    if(reference.geometryToken.isEmpty()||reference.geometryToken!=part->geometryToken){
        setError(error,"The selected face reference is stale because the body geometry changed.");return false;
    }
    if(reference.faceIndex<1){setError(error,"Face index must be a positive one-based index.");return false;}
    try {
        int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_FACE);explorer.More();explorer.Next()){
            if(++index!=reference.faceIndex)continue;
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(TopoDS::Face(explorer.Current()),properties);
            const double candidate=properties.Mass();
            if(!std::isfinite(candidate)||candidate<=0.0){setError(error,"The selected face has no measurable positive area.");return false;}
            *area=candidate;
            return true;
        }
        setError(error,"The selected face index is outside the body's topology.");
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Face-area measurement failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Face-area measurement failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureAngleBetweenPlanarFaces(const FaceReference& first,
                                                    const FaceReference& second,
                                                    FaceAngleMeasurement* result,
                                                    QString* error) const {
    if(!result){setError(error,"Face-angle output is null.");return false;}
    const auto resolveNormal=[this](const FaceReference& reference,gp_Dir* normal,gp_Pnt* origin,QString* detail) {
        if(!normal||!origin){setError(detail,"Face plane output is null.");return false;}
        const Part* part=find(reference.partId);
        if(!part||part->shape.IsNull()){
            setError(detail,"The selected face's body does not exist or has no geometry.");return false;
        }
        if(reference.geometryToken.isEmpty()||reference.geometryToken!=part->geometryToken){
            setError(detail,"The selected face reference is stale because the body geometry changed.");return false;
        }
        if(reference.faceIndex<1){setError(detail,"Face index must be a positive one-based index.");return false;}
        int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_FACE);explorer.More();explorer.Next()){
            if(++index!=reference.faceIndex)continue;
            const TopoDS_Face face=TopoDS::Face(explorer.Current());
            const BRepAdaptor_Surface surface(face,true);
            if(surface.GetType()!=GeomAbs_Plane){
                setError(detail,"Angle measurement requires two planar faces.");return false;
            }
            gp_Dir candidate=surface.Plane().Axis().Direction();
            if(face.Orientation()==TopAbs_REVERSED)candidate.Reverse();
            *normal=candidate;
            *origin=surface.Plane().Location();
            return true;
        }
        setError(detail,"The selected face index is outside the body's topology.");
        return false;
    };
    try {
        gp_Dir firstNormal,secondNormal;gp_Pnt firstOrigin,secondOrigin;
        QString detail;
        if(!resolveNormal(first,&firstNormal,&firstOrigin,&detail)){
            setError(error,QStringLiteral("First face: %1").arg(detail));return false;
        }
        if(!resolveNormal(second,&secondNormal,&secondOrigin,&detail)){
            setError(error,QStringLiteral("Second face: %1").arg(detail));return false;
        }
        const double dot=std::clamp(firstNormal.Dot(secondNormal),-1.0,1.0);
        FaceAngleMeasurement candidate;
        candidate.angleDegrees=std::acos(dot)*(180.0/std::acos(-1.0));
        candidate.parallel=std::abs(dot)>=1.0-1e-7;
        if(candidate.parallel)
            candidate.planeSeparationMm=std::abs(gp_Vec(firstOrigin,secondOrigin).Dot(gp_Vec(firstNormal)));
        candidate.firstNormalX=firstNormal.X();candidate.firstNormalY=firstNormal.Y();candidate.firstNormalZ=firstNormal.Z();
        candidate.secondNormalX=secondNormal.X();candidate.secondNormalY=secondNormal.Y();candidate.secondNormalZ=secondNormal.Z();
        if(!std::isfinite(candidate.angleDegrees)){
            setError(error,"The planar face angle is not finite.");return false;
        }
        *result=candidate;
        return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Face-angle measurement failed: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QString("Face-angle measurement failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureCylindricalFace(const FaceReference& reference,
                                           CylindricalFaceMeasurement* result,
                                           QString* error) const {
    if(!result){setError(error,"Cylindrical-face output is null.");return false;}
    const Part* part=find(reference.partId);
    if(!part||part->shape.IsNull()){
        setError(error,"The selected face's body does not exist or has no geometry.");return false;
    }
    if(reference.geometryToken.isEmpty()||reference.geometryToken!=part->geometryToken){
        setError(error,"The selected face reference is stale because the body geometry changed.");return false;
    }
    if(reference.faceIndex<1){setError(error,"Face index must be a positive one-based index.");return false;}
    try{
        int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_FACE);explorer.More();explorer.Next()){
            if(++index!=reference.faceIndex)continue;
            const BRepAdaptor_Surface surface(TopoDS::Face(explorer.Current()),true);
            if(surface.GetType()!=GeomAbs_Cylinder){
                setError(error,"Select a cylindrical side face to measure its radius and axis.");return false;
            }
            const gp_Cylinder cylinder=surface.Cylinder();
            const gp_Ax1 axis=cylinder.Axis();
            CylindricalFaceMeasurement candidate;
            candidate.radiusMm=cylinder.Radius();
            candidate.diameterMm=2.0*candidate.radiusMm;
            candidate.axisX=axis.Direction().X();candidate.axisY=axis.Direction().Y();candidate.axisZ=axis.Direction().Z();
            candidate.originX=axis.Location().X();candidate.originY=axis.Location().Y();candidate.originZ=axis.Location().Z();
            if(!std::isfinite(candidate.radiusMm)||candidate.radiusMm<=0.0||
               !std::isfinite(candidate.diameterMm)){
                setError(error,"The cylindrical face has an invalid radius.");return false;
            }
            *result=candidate;
            return true;
        }
        setError(error,"The selected face index is outside the body's topology.");
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Cylindrical-face measurement failed: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QString("Cylindrical-face measurement failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureEdgeLength(const EdgeReference& reference,double* length,QString* error) const {
    if(!length){setError(error,"Edge-length output is null.");return false;}
    const Part* part=find(reference.partId);
    if(!part||part->shape.IsNull()){setError(error,"The selected edge's body does not exist or has no geometry.");return false;}
    if(reference.geometryToken.isEmpty()||reference.geometryToken!=part->geometryToken){
        setError(error,"The selected edge reference is stale because the body geometry changed.");return false;
    }
    if(reference.edgeIndex<1){setError(error,"Edge index must be a positive one-based index.");return false;}
    try {
        int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next()){
            if(++index!=reference.edgeIndex)continue;
            GProp_GProps properties;
            BRepGProp::LinearProperties(TopoDS::Edge(explorer.Current()),properties);
            const double candidate=properties.Mass();
            if(!std::isfinite(candidate)||candidate<=0.0){setError(error,"The selected edge has no measurable positive length.");return false;}
            *length=candidate;
            return true;
        }
        setError(error,"The selected edge index is outside the body's topology.");
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Edge-length measurement failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Edge-length measurement failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureVertexCoordinates(const VertexReference& reference,double* x,double* y,double* z,QString* error) const {
    if(!x||!y||!z){setError(error,"Vertex-coordinate output is null.");return false;}
    const Part* part=find(reference.partId);
    if(!part||part->shape.IsNull()){setError(error,"The selected vertex's body does not exist or has no geometry.");return false;}
    if(reference.geometryToken.isEmpty()||reference.geometryToken!=part->geometryToken){
        setError(error,"The selected vertex reference is stale because the body geometry changed.");return false;
    }
    if(reference.vertexIndex<1){setError(error,"Vertex index must be a positive one-based index.");return false;}
    try {
        int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_VERTEX);explorer.More();explorer.Next()){
            if(++index!=reference.vertexIndex)continue;
            const gp_Pnt point=BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
            if(!std::isfinite(point.X())||!std::isfinite(point.Y())||!std::isfinite(point.Z())){
                setError(error,"The selected vertex has non-finite coordinates.");return false;
            }
            *x=point.X();*y=point.Y();*z=point.Z();return true;
        }
        setError(error,"The selected vertex index is outside the body's topology.");
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Vertex-coordinate measurement failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Vertex-coordinate measurement failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureVertexDistance(const VertexReference& first,const VertexReference& second,double* distance,QString* error) const {
    if(!distance){setError(error,"Vertex-distance output is null.");return false;}
    double ax=0,ay=0,az=0,bx=0,by=0,bz=0;
    if(!measureVertexCoordinates(first,&ax,&ay,&az,error)||!measureVertexCoordinates(second,&bx,&by,&bz,error))return false;
    const double result=std::hypot(bx-ax,by-ay,bz-az);
    if(!std::isfinite(result)){setError(error,"The selected vertices have an invalid separation.");return false;}
    *distance=result;
    return true;
}

bool SceneDocument::filletEdge(const EdgeReference& reference, double radius, QString* error) {
    FilletCommand command;
    if (!prepareFillet(reference, radius, &command, error)) return false;
    const FilletPreview preview = buildFilletPreview(command);
    if (preview.shape.IsNull()) { setError(error, preview.error); return false; }
    return commitFillet(command, preview.shape, error);
}

bool SceneDocument::filletAllEdges(const QString& id,double radius,QString* error) {
    if(!finitePositive(radius)){setError(error,"Fillet radius must be finite and positive.");return false;}
    const Part* part=find(id);if(!part){setError(error,"Object not found.");return false;}
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature){setError(error,"Edit the source bodies of a linked Boolean result instead.");return false;}
    if(!part->linkedFoldedFlatSourceId.isEmpty()){setError(error,"Associative flat blanks are read-only; edit the folded source.");return false;}
    if(rejectSurfaceOnlyInput(*part,QStringLiteral("Fillet"),error))return false;
    try{
        BRepFilletAPI_MakeFillet operation(part->shape);
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> uniqueEdges;
        TopExp::MapShapes(part->shape,TopAbs_EDGE,uniqueEdges);
        for(int index=1;index<=uniqueEdges.Extent();++index){
            operation.Add(radius,TopoDS::Edge(uniqueEdges.FindKey(index)));
        }
        if(uniqueEdges.Extent()==0){setError(error,"The body has no edges to fillet.");return false;}
        operation.Build();
        if(!operation.IsDone()){setError(error,"The selected radius cannot be applied to every body edge.");return false;}
        const TopoDS_Shape result=operation.Shape();if(!validShape(result,error))return false;
        QVector<Part> candidate=parts_;
        for(Part& target:candidate)if(target.id==id){
            target.shape=result;
            target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
            break;
        }
        if(!propagateLinkedBodies(&candidate,error))return false;
        checkpoint();parts_=std::move(candidate);
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::prepareFillet(const EdgeReference& reference, double radius, FilletCommand* command, QString* error) const {
    return prepareFillet(QVector<EdgeReference>{reference},radius,command,error);
}

bool SceneDocument::prepareFillet(const QVector<EdgeReference>& references,double radius,FilletCommand* command,QString* error) const {
    if (!command) { setError(error, "Fillet command output is unavailable."); return false; }
    if (!std::isfinite(radius) || radius < 0.00001 || radius > 1e6) {
        setError(error, "Fillet radius must be finite and between 0.00001 and 1,000,000 mm.");
        return false;
    }
    if (references.isEmpty()) { setError(error,"Select at least one edge to fillet."); return false; }
    const EdgeReference& reference=references.first();
    const Part* part = find(reference.partId);
    if (!part) { setError(error, "The selected body no longer exists."); return false; }
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature){setError(error,"Edit linked Boolean source bodies, not the derived result.");return false;}
    if(!part->linkedFoldedFlatSourceId.isEmpty()){setError(error,"Associative flat blanks are read-only; edit the folded source.");return false;}
    if(rejectSurfaceOnlyInput(*part,QStringLiteral("Fillet"),error))return false;
    QVector<TopoDS_Edge> resolved;
    QSet<int> uniqueIndices;
    for (const EdgeReference& selected:references) {
        if (selected.partId!=reference.partId) { setError(error,"Selected fillet edges must belong to one body."); return false; }
        if (selected.geometryToken.isEmpty()||part->geometryToken!=selected.geometryToken) {
            setError(error,"A selected edge is stale because its body geometry has changed. Select the edges again."); return false;
        }
        if (selected.edgeIndex<1||uniqueIndices.contains(selected.edgeIndex)) {
            setError(error,"A selected edge reference is invalid or duplicated."); return false;
        }
        uniqueIndices.insert(selected.edgeIndex);
        TopoDS_Edge edge;int index=0;
        for (TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next())
            if (++index==selected.edgeIndex) { edge=TopoDS::Edge(explorer.Current());break; }
        if (edge.IsNull()) { setError(error,"A selected edge could not be resolved. Select the edges again."); return false; }
        resolved.append(edge);
    }
    command->edge = reference;
    command->edges=references;
    command->sourceShape = part->shape;
    command->sourceEdge = resolved.first();
    command->sourceEdges=resolved;
    command->radius = radius;
    return true;
}

FilletPreview SceneDocument::buildFilletPreview(const FilletCommand& command) {
    FilletPreview preview;
    try {
        if (command.sourceShape.IsNull() || (command.sourceEdges.isEmpty()&&command.sourceEdge.IsNull())) {
            preview.error = "The fillet preview command is incomplete.";
            return preview;
        }
        BRepFilletAPI_MakeFillet operation(command.sourceShape);
        if(command.sourceEdges.isEmpty()) operation.Add(command.radius,command.sourceEdge);
        else for(const TopoDS_Edge& edge:command.sourceEdges) operation.Add(command.radius,edge);
        operation.Build();
        if (!operation.IsDone()) { preview.error = "The kernel could not build a fillet for this edge and radius."; return preview; }
        preview.shape = operation.Shape();
        if (!validShape(preview.shape, &preview.error)) preview.shape = TopoDS_Shape();
    } catch (const Standard_Failure& e) { preview.error = QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error = QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitFillet(const FilletCommand& command, const TopoDS_Shape& preview, QString* error) {
    const Part* part = find(command.edge.partId);
    if (!part) { setError(error, "The selected body no longer exists."); return false; }
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature){setError(error,"Edit linked Boolean source bodies, not the derived result.");return false;}
    if(!part->linkedFoldedFlatSourceId.isEmpty()){setError(error,"Associative flat blanks are read-only; edit the folded source.");return false;}
    if (part->geometryToken != command.edge.geometryToken) {
        setError(error, "The fillet preview is stale because the body geometry changed. Select the edge again.");
        return false;
    }
    if (!validShape(preview, error)) return false;
    QVector<Part> candidate=parts_;
    for(Part& target:candidate)if(target.id==command.edge.partId){
        target.shape=preview;
        target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);
    return true;
}

bool SceneDocument::chamferEdge(const EdgeReference& reference, double distance, QString* error) {
    ChamferCommand command;
    if (!prepareChamfer(reference, distance, &command, error)) return false;
    const ChamferPreview preview = buildChamferPreview(command);
    if (preview.shape.IsNull()) { setError(error, preview.error); return false; }
    return commitChamfer(command, preview.shape, error);
}

bool SceneDocument::chamferAllEdges(const QString& id,double distance,QString* error) {
    if(!finitePositive(distance)){setError(error,"Chamfer distance must be finite and positive.");return false;}
    const Part* part=find(id);if(!part){setError(error,"Object not found.");return false;}
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature){setError(error,"Edit the source bodies of a linked Boolean result instead.");return false;}
    if(!part->linkedFoldedFlatSourceId.isEmpty()){setError(error,"Associative flat blanks are read-only; edit the folded source.");return false;}
    if(rejectSurfaceOnlyInput(*part,QStringLiteral("Chamfer"),error))return false;
    try{
        BRepFilletAPI_MakeChamfer operation(part->shape);
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> uniqueEdges;
        TopExp::MapShapes(part->shape,TopAbs_EDGE,uniqueEdges);
        for(int index=1;index<=uniqueEdges.Extent();++index){
            operation.Add(distance,TopoDS::Edge(uniqueEdges.FindKey(index)));
        }
        if(uniqueEdges.Extent()==0){setError(error,"The body has no edges to chamfer.");return false;}
        operation.Build();
        if(!operation.IsDone()){setError(error,"The selected distance cannot be applied to every body edge.");return false;}
        const TopoDS_Shape result=operation.Shape();if(!validShape(result,error))return false;
        QVector<Part> candidate=parts_;
        for(Part& target:candidate)if(target.id==id){
            target.shape=result;
            target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
            break;
        }
        if(!propagateLinkedBodies(&candidate,error))return false;
        checkpoint();parts_=std::move(candidate);
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::draftFace(const QString& id,int faceIndex,Axis pullAxis,double neutralPlaneOffset,
                              double angleDegrees,QString* error) {
    if(faceIndex<1||!finiteOffset(neutralPlaneOffset)||!std::isfinite(angleDegrees)||
       angleDegrees==0.0||std::abs(angleDegrees)>45.0||
       (pullAxis!=Axis::X&&pullAxis!=Axis::Y&&pullAxis!=Axis::Z)){
        setError(error,"Draft face index, pull axis, neutral plane, or angle is invalid.");return false;
    }
    const Part* part=find(id);if(!part){setError(error,"Object not found.");return false;}
    if(rejectSurfaceOnlyInput(*part,QStringLiteral("Draft"),error))return false;
    TopoDS_Face face;int index=0;
    for(TopExp_Explorer explorer(part->shape,TopAbs_FACE);explorer.More();explorer.Next())
        if(++index==faceIndex){face=TopoDS::Face(explorer.Current());break;}
    if(face.IsNull()){setError(error,"The selected face index is outside this body's face list.");return false;}
    try{
        gp_Dir direction(0,0,1);
        if(pullAxis==Axis::X)direction=gp_Dir(1,0,0);else if(pullAxis==Axis::Y)direction=gp_Dir(0,1,0);
        gp_Pnt planePoint(0,0,0);
        if(pullAxis==Axis::X)planePoint.SetX(neutralPlaneOffset);
        else if(pullAxis==Axis::Y)planePoint.SetY(neutralPlaneOffset);
        else planePoint.SetZ(neutralPlaneOffset);
        const gp_Pln neutralPlane(planePoint,direction);
        BRepOffsetAPI_DraftAngle operation(part->shape);
        operation.Add(face,direction,angleDegrees*std::numbers::pi/180.0,neutralPlane,true);
        if(!operation.AddDone()){setError(error,"The requested face cannot be drafted against this neutral plane.");return false;}
        operation.Build();
        if(!operation.IsDone()){setError(error,"The CAD kernel could not complete the face draft.");return false;}
        const TopoDS_Shape result=operation.Shape();if(!validShape(result,error))return false;
        checkpoint();for(Part& target:parts_)if(target.id==id){target.shape=result;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::prepareChamfer(const EdgeReference& reference, double distance, ChamferCommand* command, QString* error) const {
    return prepareChamfer(QVector<EdgeReference>{reference},distance,command,error);
}

bool SceneDocument::prepareChamfer(const QVector<EdgeReference>& references,double distance,ChamferCommand* command,QString* error) const {
    if (!command) { setError(error, "Chamfer command output is unavailable."); return false; }
    if (!std::isfinite(distance) || distance < 0.00001 || distance > 1e6) {
        setError(error, "Chamfer distance must be finite and between 0.00001 and 1,000,000 mm.");
        return false;
    }
    if(references.isEmpty()){setError(error,"Select at least one edge to chamfer.");return false;}
    const EdgeReference& reference=references.first();
    const Part* part = find(reference.partId);
    if (!part) { setError(error, "The selected body no longer exists."); return false; }
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature){setError(error,"Edit linked Boolean source bodies, not the derived result.");return false;}
    if(!part->linkedFoldedFlatSourceId.isEmpty()){setError(error,"Associative flat blanks are read-only; edit the folded source.");return false;}
    QVector<TopoDS_Edge> resolved;QSet<int> uniqueIndices;
    for(const EdgeReference& selected:references){
        if(selected.partId!=reference.partId){setError(error,"Selected chamfer edges must belong to one body.");return false;}
        if(selected.geometryToken.isEmpty()||part->geometryToken!=selected.geometryToken){
            setError(error,"A selected edge is stale because its body geometry has changed. Select the edges again.");return false;
        }
        if(selected.edgeIndex<1||uniqueIndices.contains(selected.edgeIndex)){
            setError(error,"A selected edge reference is invalid or duplicated.");return false;
        }
        uniqueIndices.insert(selected.edgeIndex);TopoDS_Edge edge;int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next())
            if(++index==selected.edgeIndex){edge=TopoDS::Edge(explorer.Current());break;}
        if(edge.IsNull()){setError(error,"A selected edge could not be resolved. Select the edges again.");return false;}
        resolved.append(edge);
    }
    command->edge = reference;
    command->edges=references;
    command->sourceShape = part->shape;
    command->sourceEdge = resolved.first();
    command->sourceEdges=resolved;
    command->distance = distance;
    return true;
}

bool SceneDocument::prepareAsymmetricChamfer(const EdgeReference& reference,double firstDistance,
                                              double secondDistance,ChamferCommand* command,QString* error) const {
    if (!command) { setError(error,"Chamfer command output is unavailable."); return false; }
    if (!std::isfinite(firstDistance)||!std::isfinite(secondDistance)||firstDistance<0.00001||
        secondDistance<0.00001||firstDistance>1e6||secondDistance>1e6) {
        setError(error,"Both chamfer distances must be finite and between 0.00001 and 1,000,000 mm."); return false;
    }
    const Part* part=find(reference.partId);
    if (!part) { setError(error,"The selected body no longer exists."); return false; }
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature){setError(error,"Edit linked Boolean source bodies, not the derived result.");return false;}
    if(!part->linkedFoldedFlatSourceId.isEmpty()){setError(error,"Associative flat blanks are read-only; edit the folded source.");return false;}
    if (reference.geometryToken.isEmpty()||part->geometryToken!=reference.geometryToken) {
        setError(error,"The selected edge is stale because its body geometry has changed. Select the edge again."); return false;
    }
    if (reference.edgeIndex<1) { setError(error,"The selected edge reference is invalid."); return false; }
    TopoDS_Edge edge;int index=0;
    for (TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next())
        if (++index==reference.edgeIndex) { edge=TopoDS::Edge(explorer.Current());break; }
    if (edge.IsNull()) { setError(error,"The selected edge could not be resolved. Select it again."); return false; }
    TopoDS_Face referenceFace;
    for (TopExp_Explorer faces(part->shape,TopAbs_FACE);faces.More()&&referenceFace.IsNull();faces.Next()) {
        const TopoDS_Face face=TopoDS::Face(faces.Current());
        for (TopExp_Explorer faceEdges(face,TopAbs_EDGE);faceEdges.More();faceEdges.Next())
            if (edge.IsSame(faceEdges.Current())) { referenceFace=face;break; }
    }
    if (referenceFace.IsNull()) { setError(error,"The selected edge does not belong to a planarizable body face."); return false; }
    command->edge=reference;command->sourceShape=part->shape;command->sourceEdge=edge;
    command->referenceFace=referenceFace;command->distance=firstDistance;command->secondDistance=secondDistance;
    return true;
}

ChamferPreview SceneDocument::buildChamferPreview(const ChamferCommand& command) {
    ChamferPreview preview;
    try {
        if (command.sourceShape.IsNull() || (command.sourceEdges.isEmpty()&&command.sourceEdge.IsNull())) {
            preview.error = "The chamfer preview command is incomplete.";
            return preview;
        }
        BRepFilletAPI_MakeChamfer operation(command.sourceShape);
        if (command.secondDistance>0.0) {
            if (command.referenceFace.IsNull()) { preview.error="The asymmetric chamfer reference face is missing.";return preview; }
            operation.Add(command.distance,command.secondDistance,command.sourceEdge,command.referenceFace);
        } else if(command.sourceEdges.isEmpty()) operation.Add(command.distance,command.sourceEdge);
        else for(const TopoDS_Edge& edge:command.sourceEdges) operation.Add(command.distance,edge);
        operation.Build();
        if (!operation.IsDone()) { preview.error = "The kernel could not build a chamfer for this edge and distance."; return preview; }
        preview.shape = operation.Shape();
        if (!validShape(preview.shape, &preview.error)) preview.shape = TopoDS_Shape();
    } catch (const Standard_Failure& e) { preview.error = QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error = QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitChamfer(const ChamferCommand& command, const TopoDS_Shape& preview, QString* error) {
    const Part* part = find(command.edge.partId);
    if (!part) { setError(error, "The selected body no longer exists."); return false; }
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature){setError(error,"Edit linked Boolean source bodies, not the derived result.");return false;}
    if(!part->linkedFoldedFlatSourceId.isEmpty()){setError(error,"Associative flat blanks are read-only; edit the folded source.");return false;}
    if (part->geometryToken != command.edge.geometryToken) {
        setError(error, "The chamfer preview is stale because the body geometry changed. Select the edge again.");
        return false;
    }
    if (!validShape(preview, error)) return false;
    QVector<Part> candidate=parts_;
    for(Part& target:candidate)if(target.id==command.edge.partId){
        target.shape=preview;
        target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);
    return true;
}

bool SceneDocument::prepareExtrusion(SketchProfile profile, double originX, double originY,
                                     double widthOrDiameter, double depth, double height,
                                     ExtrusionCommand* command, QString* error) const {
    return prepareExtrusion(profile, originX, originY, widthOrDiameter, depth, height,
                            ExtrusionMode::NewBody, QString(), command, error);
}

bool SceneDocument::prepareExtrusion(SketchProfile profile, double originX, double originY,
                                     double widthOrDiameter, double depth, double height,
                                     ExtrusionMode mode, const QString& targetPartId,
                                     ExtrusionCommand* command, QString* error) const {
    if (!command) { setError(error, "Extrusion command output is unavailable."); return false; }
    if (mode == ExtrusionMode::NewBody && parts_.size() >= kMaxObjects) {
        setError(error, "Scene object limit reached."); return false;
    }
    if (!finiteOffset(originX) || !finiteOffset(originY)) {
        setError(error, "Profile origin must be finite and within +/-1,000,000 mm.");
        return false;
    }
    if (!finitePositive(widthOrDiameter) || !finitePositive(height)) {
        setError(error, "Profile size and extrusion height must be between 0.00001 and 1,000,000 mm.");
        return false;
    }
    if ((profile == SketchProfile::Rectangle || profile == SketchProfile::Ellipse || profile == SketchProfile::Slot ||
         profile == SketchProfile::RoundedRectangle || profile == SketchProfile::Ring) &&
        !finitePositive(depth)) {
        setError(error, "The sketch profile depth must be between 0.00001 and 1,000,000 mm."); return false;
    }
    if ((profile == SketchProfile::Ellipse || profile == SketchProfile::Slot) && depth > widthOrDiameter) {
        setError(error, "Ellipse minor diameter or slot width must not exceed its major dimension."); return false;
    }
    if(profile==SketchProfile::Ring&&depth>=widthOrDiameter){setError(error,"Ring inner diameter must be smaller than its outer diameter.");return false;}
    if (profile == SketchProfile::RegularPolygon &&
        (!std::isfinite(depth) || depth < 3 || depth > 64 || std::floor(depth) != depth)) {
        setError(error, "Regular polygon sides must be an integer from 3 through 64."); return false;
    }
    const Part* target = nullptr;
    if (mode == ExtrusionMode::Join || mode == ExtrusionMode::Cut) {
        target = find(targetPartId);
        if (!target) { setError(error, "Select an existing target body for Join or Cut."); return false; }
    } else if (mode != ExtrusionMode::NewBody) {
        setError(error, "The extrusion operation is not supported."); return false;
    }
    command->profile = profile;
    command->sketchPlane = ExtrusionSketchPlane::XY;
    command->mode = mode;
    command->targetPartId = target ? target->id : QString();
    command->targetGeometryToken = target ? target->geometryToken : QString();
    // Boolean preview builders may update topology flags on their input handles.
    // Keep the document's body detached so preview remains strictly non-mutating.
    command->targetShape = target ? BRepBuilderAPI_Copy(target->shape).Shape() : TopoDS_Shape();
    command->originX = originX;
    command->originY = originY;
    command->widthOrDiameter = widthOrDiameter;
    command->depth = depth;
    command->height = height;
    command->symmetric = false;
    command->throughAll = false;
    command->taperDegrees = 0.0;
    command->cornerRadius = 0.0;
    command->sourceRevision = revision_;
    return true;
}

bool SceneDocument::prepareSymmetricExtrusion(SketchProfile profile,double originX,double originY,double widthOrDiameter,
                                               double depth,double height,ExtrusionMode mode,const QString& targetPartId,
                                               ExtrusionCommand* command,QString* error) const {
    if(!prepareExtrusion(profile,originX,originY,widthOrDiameter,depth,height,mode,targetPartId,command,error))return false;
    command->symmetric=true;
    return true;
}

bool SceneDocument::prepareTaperedExtrusion(SketchProfile profile,double originX,double originY,double widthOrDiameter,
                                             double depth,double height,double taperDegrees,ExtrusionMode mode,
                                             const QString& targetPartId,ExtrusionCommand* command,QString* error) const {
    if(profile!=SketchProfile::Rectangle&&profile!=SketchProfile::Circle){
        setError(error,"Tapered extrusion currently supports rectangle and circle profiles.");return false;
    }
    if(!std::isfinite(taperDegrees)||std::abs(taperDegrees)>45.0||taperDegrees==0.0){
        setError(error,"Taper angle must be non-zero and between -45 and 45 degrees.");return false;
    }
    if(!prepareExtrusion(profile,originX,originY,widthOrDiameter,depth,height,mode,targetPartId,command,error))return false;
    const double inset=height*std::tan(taperDegrees*std::numbers::pi/180.0);
    if((profile==SketchProfile::Circle&&widthOrDiameter/2.0-inset<=1e-5)||
       (profile==SketchProfile::Rectangle&&(widthOrDiameter-2.0*inset<=1e-5||depth-2.0*inset<=1e-5))){
        setError(error,"The taper angle collapses the top profile. Reduce the angle or height.");return false;
    }
    command->taperDegrees=taperDegrees;
    return true;
}

bool SceneDocument::prepareRoundedRectangleExtrusion(double originX,double originY,double width,double depth,double cornerRadius,
                                                       double height,ExtrusionMode mode,const QString& targetPartId,
                                                       ExtrusionCommand* command,QString* error) const {
    if(!finitePositive(cornerRadius)||cornerRadius>=std::min(width,depth)/2.0){
        setError(error,"Corner radius must be positive and less than half the smaller rectangle dimension.");return false;
    }
    if(!prepareExtrusion(SketchProfile::RoundedRectangle,originX,originY,width,depth,height,mode,targetPartId,command,error))return false;
    command->cornerRadius=cornerRadius;
    return true;
}

ExtrusionPreview SceneDocument::buildExtrusionPreview(const ExtrusionCommand& command) {
    ExtrusionPreview preview;
    try {
        gp_Dir customNormal(0,0,1),customU(1,0,0),customV(0,1,0);
        if(command.sketchPlane==ExtrusionSketchPlane::Custom){
            const double frameValues[]={command.customOriginX,command.customOriginY,command.customOriginZ,
                command.customNormalX,command.customNormalY,command.customNormalZ,
                command.customUAxisX,command.customUAxisY,command.customUAxisZ};
            for(double value:frameValues)if(!std::isfinite(value)||std::abs(value)>1e6){
                preview.error="The custom sketch frame must contain finite values within +/-1,000,000.";return preview;
            }
            const gp_Vec normal(command.customNormalX,command.customNormalY,command.customNormalZ);
            const gp_Vec uInput(command.customUAxisX,command.customUAxisY,command.customUAxisZ);
            if(normal.Magnitude()<=1e-9){preview.error="The custom sketch-plane normal cannot be zero.";return preview;}
            customNormal=gp_Dir(normal);
            const gp_Vec projectedU=uInput-normal*(uInput.Dot(normal)/normal.SquareMagnitude());
            if(projectedU.Magnitude()<=1e-9){preview.error="The custom U axis cannot be parallel to the sketch-plane normal.";return preview;}
            customU=gp_Dir(projectedU);
            customV=customNormal.Crossed(customU);
        }
        if (!finiteOffset(command.originX) || !finiteOffset(command.originY) || !finiteOffset(command.planeOffset) ||
            !finitePositive(command.widthOrDiameter) || !finitePositive(command.height) ||
            ((command.profile == SketchProfile::Rectangle || command.profile == SketchProfile::Ellipse ||
              command.profile == SketchProfile::Slot || command.profile == SketchProfile::RoundedRectangle ||
              command.profile == SketchProfile::Ring) && !finitePositive(command.depth)) ||
            ((command.profile == SketchProfile::Ellipse || command.profile == SketchProfile::Slot) &&
             command.depth > command.widthOrDiameter) ||
            (command.profile==SketchProfile::Ring&&command.depth>=command.widthOrDiameter) ||
            (command.profile==SketchProfile::RoundedRectangle&&
             (!finitePositive(command.cornerRadius)||command.cornerRadius>=std::min(command.widthOrDiameter,command.depth)/2.0)) ||
            (command.profile == SketchProfile::RegularPolygon &&
             (!std::isfinite(command.depth) || command.depth < 3 || command.depth > 64 ||
              std::floor(command.depth) != command.depth)) ||
            !std::isfinite(command.taperDegrees)||std::abs(command.taperDegrees)>45.0||
            (command.symmetric&&command.taperDegrees!=0.0)||
            (command.sketchPlane!=ExtrusionSketchPlane::XY&&command.sketchPlane!=ExtrusionSketchPlane::XZ&&
             command.sketchPlane!=ExtrusionSketchPlane::YZ&&command.sketchPlane!=ExtrusionSketchPlane::Custom)) {
            preview.error = "The sketch extrusion parameters are invalid.";
            return preview;
        }

        double extrusionHeight=command.height;
        if(command.throughAll){
            if(command.mode!=ExtrusionMode::Cut||command.symmetric||command.taperDegrees!=0.0||command.targetShape.IsNull()){
                preview.error="Through-all extrusion requires an untapered, one-sided Cut with a valid target body.";
                return preview;
            }
            Bnd_Box targetBounds;
            BRepBndLib::AddOptimal(command.targetShape,targetBounds,false,false);
            if(targetBounds.IsVoid()) { preview.error="The through-all target body has no measurable bounds.";return preview; }
            double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
            targetBounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
            double low=0,high=0,start=0,worldDirection=0;
            if(command.sketchPlane==ExtrusionSketchPlane::Custom){
                low=std::numeric_limits<double>::infinity();high=-low;start=command.planeOffset;worldDirection=1;
                const gp_Pnt origin(command.customOriginX,command.customOriginY,command.customOriginZ);
                for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z){
                    const gp_Pnt corner(x?xmax:xmin,y?ymax:ymin,z?zmax:zmin);
                    const double projection=gp_Vec(origin,corner).Dot(gp_Vec(customNormal));
                    low=std::min(low,projection);high=std::max(high,projection);
                }
            }else if(command.sketchPlane==ExtrusionSketchPlane::XY){low=zmin;high=zmax;start=command.planeOffset;worldDirection=1;}
            else if(command.sketchPlane==ExtrusionSketchPlane::XZ){low=ymin;high=ymax;start=-command.planeOffset;worldDirection=-1;}
            else {low=xmin;high=xmax;start=command.planeOffset;worldDirection=1;}
            worldDirection*=command.reverseDirection?-1.0:1.0;
            const double distance=worldDirection>0?high-start:start-low;
            const double clearance=std::max(1e-3,(high-low)*1e-6);
            if(!std::isfinite(distance)||distance<=0||distance+clearance>1e6){
                preview.error="The through-all direction does not cross the target body or exceeds the modeling limit.";
                return preview;
            }
            extrusionHeight=distance+clearance;
        }

        TopoDS_Face profileFace;
        if (command.profile == SketchProfile::Rectangle) {
            BRepBuilderAPI_MakePolygon boundary;
            boundary.Add(gp_Pnt(command.originX, command.originY, 0.0));
            boundary.Add(gp_Pnt(command.originX + command.widthOrDiameter, command.originY, 0.0));
            boundary.Add(gp_Pnt(command.originX + command.widthOrDiameter,
                                command.originY + command.depth, 0.0));
            boundary.Add(gp_Pnt(command.originX, command.originY + command.depth, 0.0));
            boundary.Close();
            if (!boundary.IsDone()) { preview.error = "The rectangle sketch boundary could not be closed."; return preview; }
            BRepBuilderAPI_MakeFace faceMaker(boundary.Wire());
            if (!faceMaker.IsDone()) { preview.error = "The rectangle sketch could not form a planar face."; return preview; }
            profileFace = faceMaker.Face();
        } else if (command.profile == SketchProfile::Circle) {
            const gp_Circ circle(gp_Ax2(gp_Pnt(command.originX, command.originY, 0.0), gp_Dir(0,0,1)),
                                 command.widthOrDiameter / 2.0);
            BRepBuilderAPI_MakeEdge edgeMaker(circle);
            if (!edgeMaker.IsDone()) { preview.error = "The circle sketch edge could not be built."; return preview; }
            BRepBuilderAPI_MakeWire wireMaker(edgeMaker.Edge());
            if (!wireMaker.IsDone()) { preview.error = "The circle sketch boundary could not be closed."; return preview; }
            BRepBuilderAPI_MakeFace faceMaker(wireMaker.Wire());
            if (!faceMaker.IsDone()) { preview.error = "The circle sketch could not form a planar face."; return preview; }
            profileFace = faceMaker.Face();
        } else if (command.profile == SketchProfile::Ellipse) {
            const gp_Elips ellipse(gp_Ax2(gp_Pnt(command.originX,command.originY,0.0),gp_Dir(0,0,1)),
                                   command.widthOrDiameter/2.0,command.depth/2.0);
            BRepBuilderAPI_MakeEdge edgeMaker(ellipse);
            BRepBuilderAPI_MakeWire wireMaker(edgeMaker.Edge());
            BRepBuilderAPI_MakeFace faceMaker(wireMaker.Wire());
            if (!faceMaker.IsDone()) { preview.error = "The ellipse sketch could not form a planar face."; return preview; }
            profileFace = faceMaker.Face();
        } else if (command.profile == SketchProfile::Slot) {
            const double radius=command.depth/2.0;
            const double left=command.originX+radius, right=command.originX+command.widthOrDiameter-radius;
            const double bottom=command.originY, top=command.originY+command.depth;
            const gp_Circ rightCap(gp_Ax2(gp_Pnt(right,bottom+radius,0),gp_Dir(0,0,1)),radius);
            const gp_Circ leftCap(gp_Ax2(gp_Pnt(left,bottom+radius,0),gp_Dir(0,0,1)),radius);
            BRepBuilderAPI_MakeWire wireMaker;
            wireMaker.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(left,bottom,0),gp_Pnt(right,bottom,0)).Edge());
            wireMaker.Add(BRepBuilderAPI_MakeEdge(rightCap,-std::numbers::pi/2.0,std::numbers::pi/2.0).Edge());
            wireMaker.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(right,top,0),gp_Pnt(left,top,0)).Edge());
            wireMaker.Add(BRepBuilderAPI_MakeEdge(leftCap,std::numbers::pi/2.0,3.0*std::numbers::pi/2.0).Edge());
            BRepBuilderAPI_MakeFace faceMaker(wireMaker.Wire());
            if (!wireMaker.IsDone() || !faceMaker.IsDone()) { preview.error = "The slot sketch could not form a closed planar face."; return preview; }
            profileFace = faceMaker.Face();
        } else if(command.profile==SketchProfile::Ring){
            const gp_Ax2 axis(gp_Pnt(command.originX,command.originY,0),gp_Dir(0,0,1));
            BRepBuilderAPI_MakeWire outer(BRepBuilderAPI_MakeEdge(gp_Circ(axis,command.widthOrDiameter/2.0)).Edge());
            BRepBuilderAPI_MakeWire inner(BRepBuilderAPI_MakeEdge(gp_Circ(axis,command.depth/2.0)).Edge());
            BRepBuilderAPI_MakeFace faceMaker(outer.Wire());faceMaker.Add(TopoDS::Wire(inner.Wire().Reversed()));
            if(!faceMaker.IsDone()){preview.error="The ring sketch could not form a planar face.";return preview;}
            profileFace=faceMaker.Face();
        } else if(command.profile==SketchProfile::RoundedRectangle){
            const double r=command.cornerRadius,x=command.originX,y=command.originY,w=command.widthOrDiameter,h=command.depth;
            BRepBuilderAPI_MakeWire wire;
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(x+r,y,0),gp_Pnt(x+w-r,y,0)).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(x+w-r,y+r,0),gp_Dir(0,0,1)),r),-std::numbers::pi/2,0).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(x+w,y+r,0),gp_Pnt(x+w,y+h-r,0)).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(x+w-r,y+h-r,0),gp_Dir(0,0,1)),r),0,std::numbers::pi/2).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(x+w-r,y+h,0),gp_Pnt(x+r,y+h,0)).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(x+r,y+h-r,0),gp_Dir(0,0,1)),r),std::numbers::pi/2,std::numbers::pi).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(x,y+h-r,0),gp_Pnt(x,y+r,0)).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(x+r,y+r,0),gp_Dir(0,0,1)),r),std::numbers::pi,3*std::numbers::pi/2).Edge());
            BRepBuilderAPI_MakeFace faceMaker(wire.Wire());
            if(!wire.IsDone()||!faceMaker.IsDone()){preview.error="The rounded-rectangle sketch could not form a planar face.";return preview;}
            profileFace=faceMaker.Face();
        } else if (command.profile == SketchProfile::Triangle || command.profile == SketchProfile::RegularPolygon) {
            const int sides=command.profile == SketchProfile::Triangle ? 3 : static_cast<int>(command.depth);
            BRepBuilderAPI_MakePolygon boundary;
            for (int i=0;i<sides;++i) {
                const double angle=2.0*std::numbers::pi*i/sides;
                boundary.Add(gp_Pnt(command.originX+command.widthOrDiameter/2.0*std::cos(angle),
                                    command.originY+command.widthOrDiameter/2.0*std::sin(angle),0.0));
            }
            boundary.Close();
            BRepBuilderAPI_MakeFace faceMaker(boundary.Wire());
            if (!boundary.IsDone() || !faceMaker.IsDone()) { preview.error = "The polygon sketch could not form a planar face."; return preview; }
            profileFace=faceMaker.Face();
        } else {
            preview.error = "The requested sketch profile is not supported.";
            return preview;
        }

        TopoDS_Shape tool;
        if(command.taperDegrees!=0.0){
            const double inset=command.height*std::tan(command.taperDegrees*std::numbers::pi/180.0);
            BRepBuilderAPI_MakeWire bottom,top;
            if(command.profile==SketchProfile::Rectangle){
                const double topWidth=command.widthOrDiameter-2.0*inset;
                const double topDepth=command.depth-2.0*inset;
                const double offsetX=inset,offsetY=inset;
                BRepBuilderAPI_MakePolygon bottomPolygon,topPolygon;
                bottomPolygon.Add(gp_Pnt(command.originX,command.originY,0));
                bottomPolygon.Add(gp_Pnt(command.originX+command.widthOrDiameter,command.originY,0));
                bottomPolygon.Add(gp_Pnt(command.originX+command.widthOrDiameter,command.originY+command.depth,0));
                bottomPolygon.Add(gp_Pnt(command.originX,command.originY+command.depth,0));bottomPolygon.Close();
                const double endZ=command.reverseDirection?-command.height:command.height;
                topPolygon.Add(gp_Pnt(command.originX+offsetX,command.originY+offsetY,endZ));
                topPolygon.Add(gp_Pnt(command.originX+offsetX+topWidth,command.originY+offsetY,endZ));
                topPolygon.Add(gp_Pnt(command.originX+offsetX+topWidth,command.originY+offsetY+topDepth,endZ));
                topPolygon.Add(gp_Pnt(command.originX+offsetX,command.originY+offsetY+topDepth,endZ));topPolygon.Close();
                bottom.Add(bottomPolygon.Wire());top.Add(topPolygon.Wire());
            }else{
                const gp_Circ bottomCircle(gp_Ax2(gp_Pnt(command.originX,command.originY,0),gp_Dir(0,0,1)),command.widthOrDiameter/2.0);
                const gp_Circ topCircle(gp_Ax2(gp_Pnt(command.originX,command.originY,command.reverseDirection?-command.height:command.height),gp_Dir(0,0,1)),command.widthOrDiameter/2.0-inset);
                bottom.Add(BRepBuilderAPI_MakeEdge(bottomCircle).Edge());top.Add(BRepBuilderAPI_MakeEdge(topCircle).Edge());
            }
            BRepOffsetAPI_ThruSections loft(true,true);
            loft.AddWire(bottom.Wire());loft.AddWire(top.Wire());loft.Build();
            if(!loft.IsDone()){preview.error="The kernel could not build the tapered extrusion.";return preview;}
            tool=loft.Shape();
        }else{
            const double direction=command.reverseDirection?-1.0:1.0;
            BRepPrimAPI_MakePrism prism(profileFace,gp_Vec(0,0,direction*extrusionHeight),true);
            prism.Build();
            if(!prism.IsDone()){preview.error="The kernel could not extrude the sketch profile.";return preview;}
            tool=prism.Shape();
            if(command.symmetric){
                gp_Trsf shift;shift.SetTranslation(gp_Vec(0,0,-direction*command.height/2.0));
                tool=BRepBuilderAPI_Transform(tool,shift,true).Shape();
            }
        }
        if(command.planeOffset!=0.0){
            gp_Trsf offsetTransform;
            offsetTransform.SetTranslation(gp_Vec(0,0,command.planeOffset));
            tool=BRepBuilderAPI_Transform(tool,offsetTransform,true).Shape();
        }
        if(command.sketchPlane==ExtrusionSketchPlane::Custom){
            const gp_Pnt frameOrigin(command.customOriginX,command.customOriginY,command.customOriginZ);
            gp_Trsf frameTransform;
            frameTransform.SetValues(customU.X(),customV.X(),customNormal.X(),frameOrigin.X(),
                                     customU.Y(),customV.Y(),customNormal.Y(),frameOrigin.Y(),
                                     customU.Z(),customV.Z(),customNormal.Z(),frameOrigin.Z());
            tool=BRepBuilderAPI_Transform(tool,frameTransform,true).Shape();
        }else if(command.sketchPlane!=ExtrusionSketchPlane::XY){
            gp_Trsf planeTransform;
            if(command.sketchPlane==ExtrusionSketchPlane::XZ)
                planeTransform.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,0,0)),std::numbers::pi/2.0);
            else
                planeTransform.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,1,1)),2.0*std::numbers::pi/3.0);
            tool=BRepBuilderAPI_Transform(tool,planeTransform,true).Shape();
        }
        if (command.mode == ExtrusionMode::NewBody) {
            preview.shape = tool;
        } else if (command.mode == ExtrusionMode::Join) {
            if (command.targetShape.IsNull()) { preview.error = "The Join target body is unavailable."; return preview; }
            BRepAlgoAPI_Fuse operation(command.targetShape, tool);
            operation.Build();
            if (!operation.IsDone() || operation.HasErrors()) {
                preview.error = "The extrusion could not be joined to the target body.";
                return preview;
            }
            preview.shape = operation.Shape();
        } else if (command.mode == ExtrusionMode::Cut) {
            if (command.targetShape.IsNull()) { preview.error = "The Cut target body is unavailable."; return preview; }
            BRepAlgoAPI_Cut operation(command.targetShape, tool);
            operation.Build();
            if (!operation.IsDone() || operation.HasErrors()) {
                preview.error = "The extrusion could not cut the target body.";
                return preview;
            }
            preview.shape = operation.Shape();
        } else {
            preview.error = "The requested extrusion operation is not supported.";
            return preview;
        }
        if (!validShape(preview.shape, &preview.error)) preview.shape = TopoDS_Shape();
    } catch (const Standard_Failure& e) { preview.error = QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error = QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitExtrusion(const ExtrusionCommand& command, const TopoDS_Shape& preview, QString* error) {
    if (command.sourceRevision != revision_) {
        setError(error, "The extrusion preview is stale because the document changed. Recreate the preview.");
        return false;
    }
    if (!validShape(preview, error)) return false;
    if (command.mode == ExtrusionMode::NewBody) {
        if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
        QString profileName;
        switch(command.profile) {
            case SketchProfile::Rectangle: profileName="Rectangle"; break;
            case SketchProfile::Circle: profileName="Circle"; break;
            case SketchProfile::Ellipse: profileName="Ellipse"; break;
            case SketchProfile::Slot: profileName="Slot"; break;
            case SketchProfile::Triangle: profileName="Triangle"; break;
            case SketchProfile::RegularPolygon: profileName="Polygon"; break;
            case SketchProfile::RoundedRectangle: profileName="Rounded Rectangle"; break;
            case SketchProfile::Ring: profileName="Ring"; break;
        }
        const QString baseName = command.symmetric ? QStringLiteral("Symmetric Extruded ")+profileName :
                                 command.taperDegrees!=0.0 ? QStringLiteral("Tapered Extruded ")+profileName :
                                 QStringLiteral("Extruded ")+profileName;
        const QString name = uniqueName(baseName);
        checkpoint();
        parts_.append({generatedId(), name, preview});
        return true;
    }
    if (command.mode != ExtrusionMode::Join && command.mode != ExtrusionMode::Cut) {
        setError(error, "The extrusion operation is not supported."); return false;
    }
    Part* target = nullptr;
    for (auto& part : parts_) if (part.id == command.targetPartId) { target = &part; break; }
    if (!target) { setError(error, "The target body no longer exists."); return false; }
    if (command.targetGeometryToken.isEmpty() || target->geometryToken != command.targetGeometryToken) {
        setError(error, "The target body changed after the extrusion preview. Recreate the preview.");
        return false;
    }
    checkpoint();
    target->shape = preview;
    target->geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return true;
}

bool SceneDocument::prepareRevolve(double radialOffset, double axialOrigin, double radialWidth,
                                   double axialHeight, double angleDegrees, RevolveCommand* command,
                                   QString* error) const {
    if (!command) { setError(error, "Revolve command output is unavailable."); return false; }
    if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
    if (!std::isfinite(radialOffset) || radialOffset < 0.0 || radialOffset > 1e6 ||
        !finiteOffset(axialOrigin)) {
        setError(error, "Revolve profile origin is out of range."); return false;
    }
    if (!finitePositive(radialWidth) || !finitePositive(axialHeight)) {
        setError(error, "Revolve profile width and height must be between 0.00001 and 1,000,000 mm.");
        return false;
    }
    if (!std::isfinite(angleDegrees) || angleDegrees < 1.0 || angleDegrees > 360.0) {
        setError(error, "Revolve angle must be between 1 and 360 degrees."); return false;
    }
    command->radialOffset = radialOffset;
    command->axialOrigin = axialOrigin;
    command->radialWidth = radialWidth;
    command->axialHeight = axialHeight;
    command->angleDegrees = angleDegrees;
    command->axisIndex=2;command->axisOriginX=0;command->axisOriginY=0;command->axisOriginZ=0;
    command->sourceRevision = revision_;
    return true;
}

bool SceneDocument::prepareRevolveAboutAxis(double radialOffset,double axialOrigin,double radialWidth,double axialHeight,
                                             double angleDegrees,int axisIndex,double originX,double originY,double originZ,
                                             RevolveCommand* command,QString* error) const {
    if(axisIndex<0||axisIndex>2||!finiteOffset(originX)||!finiteOffset(originY)||!finiteOffset(originZ)){
        setError(error,"Choose X, Y, or Z and an axis origin within +/-1,000,000 mm.");return false;
    }
    if(!prepareRevolve(radialOffset,axialOrigin,radialWidth,axialHeight,angleDegrees,command,error))return false;
    command->axisIndex=axisIndex;command->axisOriginX=originX;command->axisOriginY=originY;command->axisOriginZ=originZ;
    return true;
}

bool SceneDocument::prepareRevolveAboutCustomAxis(double radialOffset,double axialOrigin,double radialWidth,double axialHeight,
                                                   double angleDegrees,double originX,double originY,double originZ,
                                                   double directionX,double directionY,double directionZ,
                                                   RevolveCommand* command,QString* error) const {
    const double directionLength=std::hypot(directionX,directionY,directionZ);
    if(!finiteOffset(originX)||!finiteOffset(originY)||!finiteOffset(originZ)||
       !std::isfinite(directionX)||!std::isfinite(directionY)||!std::isfinite(directionZ)||
       std::abs(directionX)>1e6||std::abs(directionY)>1e6||std::abs(directionZ)>1e6||
       !std::isfinite(directionLength)||directionLength<1e-8){
        setError(error,"Enter a finite axis origin and a non-zero custom axis direction.");return false;
    }
    if(!prepareRevolve(radialOffset,axialOrigin,radialWidth,axialHeight,angleDegrees,command,error))return false;
    command->axisIndex=3;command->axisOriginX=originX;command->axisOriginY=originY;command->axisOriginZ=originZ;
    command->axisDirectionX=directionX;command->axisDirectionY=directionY;command->axisDirectionZ=directionZ;
    return true;
}

RevolvePreview SceneDocument::buildRevolvePreview(const RevolveCommand& command) {
    RevolvePreview preview;
    try {
        if (!std::isfinite(command.radialOffset) || command.radialOffset < 0.0 || command.radialOffset > 1e6 ||
            !finiteOffset(command.axialOrigin) || !finitePositive(command.radialWidth) ||
            !finitePositive(command.axialHeight) || !std::isfinite(command.angleDegrees) ||
            command.angleDegrees < 1.0 || command.angleDegrees > 360.0 || command.axisIndex<0 || command.axisIndex>3 ||
            !finiteOffset(command.axisOriginX)||!finiteOffset(command.axisOriginY)||!finiteOffset(command.axisOriginZ)) {
            preview.error = "The revolve profile parameters are invalid.";
            return preview;
        }
        if(command.axisIndex==3){
            const double directionLength=std::hypot(command.axisDirectionX,command.axisDirectionY,command.axisDirectionZ);
            if(!std::isfinite(command.axisDirectionX)||!std::isfinite(command.axisDirectionY)||
               !std::isfinite(command.axisDirectionZ)||std::abs(command.axisDirectionX)>1e6||
               std::abs(command.axisDirectionY)>1e6||std::abs(command.axisDirectionZ)>1e6||
               !std::isfinite(directionLength)||directionLength<1e-8){
                preview.error="The custom revolve axis direction must be finite and non-zero.";return preview;
            }
        }
        BRepBuilderAPI_MakePolygon boundary;
        boundary.Add(gp_Pnt(command.radialOffset,0.0,command.axialOrigin));
        boundary.Add(gp_Pnt(command.radialOffset+command.radialWidth,0.0,command.axialOrigin));
        boundary.Add(gp_Pnt(command.radialOffset+command.radialWidth,0.0,command.axialOrigin+command.axialHeight));
        boundary.Add(gp_Pnt(command.radialOffset,0.0,command.axialOrigin+command.axialHeight));
        boundary.Close();
        if (!boundary.IsDone()) { preview.error = "The revolve rectangle could not be closed."; return preview; }
        BRepBuilderAPI_MakeFace faceMaker(boundary.Wire());
        if (!faceMaker.IsDone()) { preview.error = "The revolve rectangle could not form a planar face."; return preview; }

        const gp_Ax1 axis(gp_Pnt(0,0,0),gp_Dir(0,0,1));
        const double angleRadians = command.angleDegrees * std::numbers::pi / 180.0;
        BRepPrimAPI_MakeRevol revolver(faceMaker.Face(),axis,angleRadians,true);
        revolver.Build();
        if (!revolver.IsDone()) { preview.error = "The kernel could not revolve the rectangular profile."; return preview; }
        preview.shape = revolver.Shape();
        if(command.axisIndex==3){
            const gp_Dir axisDirection(command.axisDirectionX,command.axisDirectionY,command.axisDirectionZ);
            const gp_Vec reference=std::abs(axisDirection.Z())<0.9?gp_Vec(0,0,1):gp_Vec(0,1,0);
            const gp_Dir radialDirection(reference.Crossed(gp_Vec(axisDirection)));
            const gp_Dir transverseDirection=axisDirection.Crossed(radialDirection);
            gp_Trsf placement;
            placement.SetValues(radialDirection.X(),transverseDirection.X(),axisDirection.X(),command.axisOriginX,
                                radialDirection.Y(),transverseDirection.Y(),axisDirection.Y(),command.axisOriginY,
                                radialDirection.Z(),transverseDirection.Z(),axisDirection.Z(),command.axisOriginZ);
            preview.shape=BRepBuilderAPI_Transform(preview.shape,placement,true).Shape();
        }else if(command.axisIndex!=2||command.axisOriginX!=0||command.axisOriginY!=0||command.axisOriginZ!=0){
            gp_Trsf placement;
            if(command.axisIndex==0)placement.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,1,0)),std::numbers::pi/2.0);
            else if(command.axisIndex==1)placement.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,0,0)),-std::numbers::pi/2.0);
            placement.SetTranslationPart(gp_Vec(command.axisOriginX,command.axisOriginY,command.axisOriginZ));
            preview.shape=BRepBuilderAPI_Transform(preview.shape,placement,true).Shape();
        }
        if (!validShape(preview.shape,&preview.error)) preview.shape = TopoDS_Shape();
    } catch (const Standard_Failure& e) { preview.error = QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error = QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitRevolve(const RevolveCommand& command, const TopoDS_Shape& preview, QString* error) {
    if (command.sourceRevision != revision_) {
        setError(error, "The revolve preview is stale because the document changed. Recreate the preview.");
        return false;
    }
    if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
    if (!validShape(preview,error)) return false;
    const QString name = uniqueName(QStringLiteral("Revolved Rectangle"));
    checkpoint();
    parts_.append({generatedId(),name,preview});
    return true;
}

bool SceneDocument::mirror(const QString& id, Axis planeNormal, double planeOffset, QString* error) {
    if (!finiteOffset(planeOffset)) { setError(error,"Mirror plane offset is out of range."); return false; }
    if (parts_.size() >= kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source = find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const QString sourceName = source->name;
    const bool visible = source->visible;
    const QString color = source->color;
    try {
        gp_Pnt point(0,0,0);
        gp_Dir normal(1,0,0);
        if (planeNormal == Axis::X) { point.SetX(planeOffset); normal = gp_Dir(1,0,0); }
        else if (planeNormal == Axis::Y) { point.SetY(planeOffset); normal = gp_Dir(0,1,0); }
        else { point.SetZ(planeOffset); normal = gp_Dir(0,0,1); }
        gp_Trsf transform;
        transform.SetMirror(gp_Ax2(point,normal));
        const TopoDS_Shape reflected = BRepBuilderAPI_Transform(source->shape,transform,true).Shape();
        if (!validShape(reflected,error)) return false;
        Part copy{generatedId(),uniqueName(sourceName+QStringLiteral(" Mirror")),reflected,visible,color};
        inheritMaterial(copy, *source);
        checkpoint();
        parts_.append(copy);
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::mirrorAcrossPlane(const QString& id,double nx,double ny,double nz,double planeOffset,QString* error) {
    if (!std::isfinite(nx)||!std::isfinite(ny)||!std::isfinite(nz)||!finiteOffset(planeOffset)||
        std::sqrt(nx*nx+ny*ny+nz*nz)<1e-8) {
        setError(error,"Enter a finite, non-zero plane normal and an offset within +/-1,000,000 mm."); return false;
    }
    if (parts_.size()>=kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    try {
        const gp_Dir normal(nx,ny,nz);
        const gp_Pnt point(normal.X()*planeOffset,normal.Y()*planeOffset,normal.Z()*planeOffset);
        gp_Trsf transform;
        transform.SetMirror(gp_Ax2(point,normal));
        const TopoDS_Shape reflected=BRepBuilderAPI_Transform(source->shape,transform,true).Shape();
        if (!validShape(reflected,error)) return false;
        Part copy{generatedId(),uniqueName(sourceName+QStringLiteral(" Plane Mirror")),reflected,visible,color};
        inheritMaterial(copy, *source);
        checkpoint(); parts_.append(copy); return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::rotateAboutAxis(const QString& id,double ox,double oy,double oz,
                                    double ax,double ay,double az,double degrees,QString* error) {
    if (!finiteOffset(ox)||!finiteOffset(oy)||!finiteOffset(oz)||!std::isfinite(ax)||!std::isfinite(ay)||
        !std::isfinite(az)||std::sqrt(ax*ax+ay*ay+az*az)<1e-8||!std::isfinite(degrees)||std::abs(degrees)>36000) {
        setError(error,"Rotation origin, axis, or angle is invalid."); return false;
    }
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    if(!source->linkedFoldedFlatSourceId.isEmpty()){
        setError(error,"Linked folded flat blank geometry is read-only; edit the folded source.");return false;
    }
    if (degrees==0.0) return true;
    if(!source->componentDefinitionId.isEmpty()){
        setError(error,"Use the occurrence orientation command for component placements.");return false;
    }
    try {
        gp_Trsf transform;
        transform.SetRotation(gp_Ax1(gp_Pnt(ox,oy,oz),gp_Dir(ax,ay,az)),degrees*std::numbers::pi/180.0);
        const TopoDS_Shape result=BRepBuilderAPI_Transform(source->shape,transform,true).Shape();
        if (!validShape(result,error)) return false;
        checkpoint();
        for (Part& part:parts_) if (part.id==id) {
            part.shape=result; part.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces); break;
        }
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::linearPattern(const QString& id, Axis axis, int instanceCount, double spacing, QString* error) {
    if (instanceCount < 2 || instanceCount > 1000) {
        setError(error,"Linear pattern count must be between 2 and 1,000 total instances."); return false;
    }
    if (!std::isfinite(spacing) || std::abs(spacing) < 0.00001 || std::abs(spacing) > 1e6 ||
        std::abs(spacing) * (instanceCount-1) > 1e6) {
        setError(error,"Pattern spacing and total offset must be finite and within 1,000,000 mm."); return false;
    }
    if (parts_.size()+instanceCount-1 > kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source = find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const TopoDS_Shape sourceShape = source->shape;
    const QString sourceName = source->name;
    const bool visible = source->visible;
    const QString color = source->color;
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try {
        for (int index=1; index<instanceCount; ++index) {
            const double offset = spacing * index;
            const gp_Vec translation = axis == Axis::X ? gp_Vec(offset,0,0) :
                                       axis == Axis::Y ? gp_Vec(0,offset,0) : gp_Vec(0,0,offset);
            gp_Trsf transform;
            transform.SetTranslation(translation);
            const TopoDS_Shape result = BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if (!validShape(result,error)) return false;
            const QString baseName = sourceName + QStringLiteral(" Pattern %1").arg(index+1);
            QString name = uniqueName(baseName);
            int suffix = 2;
            while (reservedNames.contains(name.toCaseFolded())) {
                name = uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            }
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
            inheritMaterial(copies.last(), *source);
        }
        checkpoint();
        for (const Part& copy : copies) parts_.append(copy);
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::linearPatternAlongVector(const QString& id,double directionX,double directionY,double directionZ,
                                              int instanceCount,double spacing,QString* error) {
    if(instanceCount<2||instanceCount>1000){setError(error,"Vector pattern count must be between 2 and 1,000 total instances.");return false;}
    if(!std::isfinite(directionX)||!std::isfinite(directionY)||!std::isfinite(directionZ)||
       std::max({std::abs(directionX),std::abs(directionY),std::abs(directionZ)})>1e6){
        setError(error,"Pattern direction components must be finite and within ±1,000,000.");return false;
    }
    const double magnitude=std::hypot(directionX,directionY,directionZ);
    if(!std::isfinite(magnitude)||magnitude<=1e-12){setError(error,"Pattern direction vector must not be zero.");return false;}
    if(!std::isfinite(spacing)||std::abs(spacing)<1e-5||std::abs(spacing)>1e6||
       std::abs(spacing)*(instanceCount-1)>1e6){
        setError(error,"Pattern spacing and total offset must be finite and within 1,000,000 mm.");return false;
    }
    if(parts_.size()+instanceCount-1>kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    const Part* source=find(id);
    if(!source){setError(error,"Object not found.");return false;}
    const TopoDS_Shape sourceShape=source->shape;
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    const gp_Vec direction(directionX/magnitude,directionY/magnitude,directionZ/magnitude);
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try{
        for(int index=1;index<instanceCount;++index){
            gp_Trsf transform;
            transform.SetTranslation(direction*(spacing*index));
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if(!validShape(result,error))return false;
            const QString baseName=sourceName+QStringLiteral(" Vector Pattern %1").arg(index+1);
            QString name=uniqueName(baseName);int suffix=2;
            while(reservedNames.contains(name.toCaseFolded()))name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
            inheritMaterial(copies.last(), *source);
        }
        checkpoint();
        for(const Part& copy:copies)parts_.append(copy);
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::rectangularPattern(const QString& id,Axis firstAxis,int firstCount,double firstSpacing,
                                       Axis secondAxis,int secondCount,double secondSpacing,QString* error) {
    if(firstCount<1||secondCount<1||firstCount>1000||secondCount>1000||firstCount>1000/secondCount||
       (firstAxis!=Axis::X&&firstAxis!=Axis::Y&&firstAxis!=Axis::Z)||
       (secondAxis!=Axis::X&&secondAxis!=Axis::Y&&secondAxis!=Axis::Z)||firstAxis==secondAxis) {
        setError(error,"Choose two different axes and a pattern with 2 to 1,000 total instances."); return false;
    }
    const int total=firstCount*secondCount;
    if(total<2||total>1000){setError(error,"Choose a pattern with 2 to 1,000 total instances.");return false;}
    auto spacingValid=[](int count,double spacing) {
        return std::isfinite(spacing)&&std::abs(spacing)<=1e6&&
               (count==1|| (std::abs(spacing)>=1e-5&&std::abs(spacing)*(count-1)<=1e6));
    };
    if (!spacingValid(firstCount,firstSpacing)||!spacingValid(secondCount,secondSpacing)) {
        setError(error,"Pattern spacings and total offsets must be within 1,000,000 mm."); return false;
    }
    if (parts_.size()+total-1>kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const TopoDS_Shape sourceShape=source->shape;
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    auto axisVector=[](Axis axis,double amount) {
        return axis==Axis::X?gp_Vec(amount,0,0):axis==Axis::Y?gp_Vec(0,amount,0):gp_Vec(0,0,amount);
    };
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try {
        for(int i=0;i<firstCount;++i) for(int j=0;j<secondCount;++j) {
            if(i==0&&j==0) continue;
            gp_Trsf transform;
            transform.SetTranslation(axisVector(firstAxis,firstSpacing*i)+axisVector(secondAxis,secondSpacing*j));
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if(!validShape(result,error)) return false;
            const QString baseName=sourceName+QStringLiteral(" Rect Pattern %1-%2").arg(i+1).arg(j+1);
            QString name=uniqueName(baseName); int suffix=2;
            while(reservedNames.contains(name.toCaseFolded())) name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
            inheritMaterial(copies.last(), *source);
        }
        checkpoint(); for(const Part& copy:copies) parts_.append(copy); return true;
    } catch(const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch(const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::rectangularPatternAlongVectors(const QString& id,double firstX,double firstY,double firstZ,
                                                    int firstCount,double firstSpacing,double secondX,double secondY,
                                                    double secondZ,int secondCount,double secondSpacing,QString* error) {
    if(firstCount<1||secondCount<1||firstCount>1000||secondCount>1000||firstCount>1000/secondCount){
        setError(error,"Choose a two-direction pattern with 2 to 1,000 total instances.");return false;
    }
    const int total=firstCount*secondCount;
    if(total<2||total>1000){setError(error,"Choose a pattern with 2 to 1,000 total instances.");return false;}
    auto vectorValid=[](double x,double y,double z){
        return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&
               std::max({std::abs(x),std::abs(y),std::abs(z)})<=1e6;
    };
    if(!vectorValid(firstX,firstY,firstZ)||!vectorValid(secondX,secondY,secondZ)){
        setError(error,"Pattern direction components must be finite and within ±1,000,000.");return false;
    }
    const double firstMagnitude=std::hypot(firstX,firstY,firstZ);
    const double secondMagnitude=std::hypot(secondX,secondY,secondZ);
    if(!std::isfinite(firstMagnitude)||!std::isfinite(secondMagnitude)||firstMagnitude<=1e-12||secondMagnitude<=1e-12){
        setError(error,"Both pattern direction vectors must be nonzero.");return false;
    }
    const gp_Vec firstDirection(firstX/firstMagnitude,firstY/firstMagnitude,firstZ/firstMagnitude);
    const gp_Vec secondDirection(secondX/secondMagnitude,secondY/secondMagnitude,secondZ/secondMagnitude);
    if(firstDirection.Crossed(secondDirection).Magnitude()<=1e-8){
        setError(error,"Pattern direction vectors must not be parallel.");return false;
    }
    auto spacingValid=[](int count,double spacing){
        return std::isfinite(spacing)&&std::abs(spacing)<=1e6&&
               (count==1||(std::abs(spacing)>=1e-5&&std::abs(spacing)*(count-1)<=1e6));
    };
    if(!spacingValid(firstCount,firstSpacing)||!spacingValid(secondCount,secondSpacing)){
        setError(error,"Pattern spacings and total offsets must be within 1,000,000 mm.");return false;
    }
    if(parts_.size()+total-1>kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    const Part* source=find(id);
    if(!source){setError(error,"Object not found.");return false;}
    const TopoDS_Shape sourceShape=source->shape;
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try{
        for(int i=0;i<firstCount;++i)for(int j=0;j<secondCount;++j){
            if(i==0&&j==0)continue;
            gp_Trsf transform;
            transform.SetTranslation(firstDirection*(firstSpacing*i)+secondDirection*(secondSpacing*j));
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if(!validShape(result,error))return false;
            const QString baseName=sourceName+QStringLiteral(" Vector Rect Pattern %1-%2").arg(i+1).arg(j+1);
            QString name=uniqueName(baseName);int suffix=2;
            while(reservedNames.contains(name.toCaseFolded()))name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
            inheritMaterial(copies.last(), *source);
        }
        checkpoint();for(const Part& copy:copies)parts_.append(copy);return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::prepareHole(const QString& id, double x, double y, double entryZ, double diameter,
                                double depth, bool throughAll, HoleCommand* command, QString* error) const {
    if (!command) { setError(error,"Hole command output is unavailable."); return false; }
    if (!finiteOffset(x) || !finiteOffset(y) || !finiteOffset(entryZ)) {
        setError(error,"Hole location must be finite and within +/-1,000,000 mm."); return false;
    }
    if (!finitePositive(diameter)) { setError(error,"Hole diameter must be between 0.00001 and 1,000,000 mm."); return false; }
    if (!throughAll && !finitePositive(depth)) { setError(error,"Blind hole depth must be between 0.00001 and 1,000,000 mm."); return false; }
    const Part* part=find(id);
    if (!part) { setError(error,"Object not found."); return false; }
    if(rejectSurfaceOnlyInput(*part,QStringLiteral("Hole"),error))return false;
    command->partId=id;
    command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->x=x;
    command->y=y;
    command->entryZ=entryZ;
    command->inwardAxis=gp_Dir(0,0,-1); // Reset reused commands to the legacy world-axis default.
    command->diameter=diameter;
    command->depth=depth;
    command->throughAll=throughAll;
    command->style=HoleStyle::Simple;
    command->headDiameter=0.0;
    command->headDepth=0.0;
    command->sinkAngleDegrees=90.0;
    return true;
}

bool SceneDocument::prepareHoleOnPlanarFace(const FaceReference& reference,double x,double y,double z,
                                            double diameter,double depth,bool throughAll,
                                            HoleCommand* command,QString* error) const {
    const Part* part=find(reference.partId);
    if(!part||reference.geometryToken.isEmpty()||part->geometryToken!=reference.geometryToken){
        setError(error,"The selected face is stale or its body no longer exists. Select it again.");return false;
    }
    if(reference.faceIndex<1){setError(error,"Select a valid planar face.");return false;}
    TopoDS_Face face;int index=0;
    for(TopExp_Explorer faces(part->shape,TopAbs_FACE);faces.More();faces.Next())
        if(++index==reference.faceIndex){face=TopoDS::Face(faces.Current());break;}
    if(face.IsNull()||!BRepCheck_Analyzer(face).IsValid()){
        setError(error,"The selected face is missing or invalid.");return false;
    }
    const BRepAdaptor_Surface surface(face);
    if(surface.GetType()!=GeomAbs_Plane){
        setError(error,"Face-normal drilling requires a planar face. Use a datum plane for curved surfaces.");return false;
    }
    const gp_Pln plane=surface.Plane();
    const gp_Pnt requested(x,y,z);
    if(!finiteOffset(x)||!finiteOffset(y)||!finiteOffset(z)||
       plane.Distance(requested)>0.02){
        setError(error,"The drill center must lie on the selected face plane (within 0.02 mm).");return false;
    }
    // Snap small UI round-off onto the analytic support plane.
    const gp_Dir outward=plane.Axis().Direction();
    const gp_Vec toPoint(plane.Location(),requested);
    const double shift=toPoint.Dot(gp_Vec(outward));
    const gp_Pnt entry=requested.Translated(gp_Vec(outward)*(-shift));
    if(!prepareHole(reference.partId,entry.X(),entry.Y(),entry.Z(),diameter,depth,throughAll,command,error))
        return false;
    gp_Dir inward=outward;
    if(face.Orientation()!=TopAbs_REVERSED)inward.Reverse();
    command->inwardAxis=inward;
    return true;
}

bool SceneDocument::prepareCounterbore(const QString& id,double x,double y,double entryZ,double diameter,double depth,
                                       bool throughAll,double headDiameter,double headDepth,HoleCommand* command,
                                       QString* error) const {
    if(!finitePositive(headDiameter)||headDiameter<=diameter||!finitePositive(headDepth)||
       (!throughAll&&headDepth>depth)){
        setError(error,"Counterbore head diameter must exceed the drill diameter; its depth must fit within a blind hole.");return false;
    }
    if(!prepareHole(id,x,y,entryZ,diameter,depth,throughAll,command,error))return false;
    command->style=HoleStyle::Counterbore;command->headDiameter=headDiameter;command->headDepth=headDepth;
    return true;
}

bool SceneDocument::prepareCountersink(const QString& id,double x,double y,double entryZ,double diameter,double depth,
                                       bool throughAll,double headDiameter,double includedAngleDegrees,
                                       HoleCommand* command,QString* error) const {
    if(!finitePositive(headDiameter)||headDiameter<=diameter||!std::isfinite(includedAngleDegrees)||
       includedAngleDegrees<=1.0||includedAngleDegrees>=179.0){
        setError(error,"Countersink head diameter must exceed the drill diameter and included angle must be between 1 and 179 degrees.");return false;
    }
    if(!prepareHole(id,x,y,entryZ,diameter,depth,throughAll,command,error))return false;
    command->style=HoleStyle::Countersink;command->headDiameter=headDiameter;command->sinkAngleDegrees=includedAngleDegrees;
    return true;
}

HolePreview SceneDocument::buildHolePreview(const HoleCommand& command) {
    HolePreview preview;
    try {
        if (command.partId.isEmpty() || command.sourceShape.IsNull() ||
            !finiteOffset(command.x) || !finiteOffset(command.y) || !finiteOffset(command.entryZ) ||
            !finitePositive(command.diameter) || (!command.throughAll && !finitePositive(command.depth)) ||
            (command.style!=HoleStyle::Simple&&(!finitePositive(command.headDiameter)||command.headDiameter<=command.diameter)) ||
            (command.style==HoleStyle::Counterbore&&(!finitePositive(command.headDepth)||(!command.throughAll&&command.headDepth>command.depth))) ||
            (command.style==HoleStyle::Countersink&&(!std::isfinite(command.sinkAngleDegrees)||command.sinkAngleDegrees<=1||command.sinkAngleDegrees>=179))) {
            preview.error="The hole command is incomplete or invalid.";
            return preview;
        }
        const gp_Pnt entry(command.x,command.y,command.entryZ);
        const gp_Vec direction(command.inwardAxis);
        double margin=0.01;
        double startOffset=-margin;
        double toolLength=command.depth+margin;
        if(command.throughAll){
            Bnd_Box bounds;
            BRepBndLib::Add(command.sourceShape,bounds);
            if(bounds.IsVoid()){
                preview.error="The selected body has no measurable bounds.";return preview;
            }
            double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
            bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
            directional_holes::Span span;
            if(!directional_holes::throughAllSpan(
                {xmin,ymin,zmin,xmax,ymax,zmax},
                {entry.X(),entry.Y(),entry.Z()},
                {direction.X(),direction.Y(),direction.Z()},&span)){
                preview.error="Could not establish a finite through-all depth along the selected drill axis.";
                return preview;
            }
            startOffset=span.start;
            toolLength=span.length;
        }
        const gp_Pnt start=entry.Translated(direction*startOffset);
        if(!std::isfinite(start.X())||!std::isfinite(start.Y())||!std::isfinite(start.Z())||
           !std::isfinite(toolLength)||toolLength<=0.0){
            preview.error="Hole cutter dimensions are out of range.";return preview;
        }
        const gp_Ax2 axis(start,command.inwardAxis);
        const TopoDS_Shape cutter=BRepPrimAPI_MakeCylinder(axis,command.diameter/2.0,toolLength).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);
        operation.Build();
        if (!operation.IsDone()) { preview.error="The kernel could not cut the requested hole."; return preview; }
        preview.shape=operation.Shape();
        const gp_Ax2 headAxis(entry.Translated(direction*(-margin)),command.inwardAxis);
        if(command.style==HoleStyle::Counterbore){
            const TopoDS_Shape headCutter=BRepPrimAPI_MakeCylinder(headAxis,command.headDiameter/2.0,command.headDepth+margin).Shape();
            BRepAlgoAPI_Cut headCut(preview.shape,headCutter);headCut.Build();
            if(!headCut.IsDone()){preview.shape=TopoDS_Shape();preview.error="The counterbore recess could not be cut.";return preview;}
            preview.shape=headCut.Shape();
        } else if(command.style==HoleStyle::Countersink){
            const double sinkDepth=(command.headDiameter-command.diameter)/(2.0*std::tan(command.sinkAngleDegrees*std::numbers::pi/360.0));
            if(!finitePositive(sinkDepth)){preview.shape=TopoDS_Shape();preview.error="The countersink depth is invalid.";return preview;}
            const double drillRadius=command.diameter/2.0;
            const double radialSlope=(command.headDiameter/2.0-drillRadius)/sinkDepth;
            const TopoDS_Shape sinkCutter=BRepPrimAPI_MakeCone(headAxis,command.headDiameter/2.0+radialSlope*margin,
                                                              drillRadius,sinkDepth+margin).Shape();
            BRepAlgoAPI_Cut sinkCut(preview.shape,sinkCutter);sinkCut.Build();
            if(!sinkCut.IsDone()){preview.shape=TopoDS_Shape();preview.error="The countersink recess could not be cut.";return preview;}
            preview.shape=sinkCut.Shape();
        }
        if (!validShape(preview.shape,&preview.error)) { preview.shape=TopoDS_Shape(); return preview; }
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);
        BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if (!std::isfinite(removed) || removed<=std::max(1e-9,before.Mass()*1e-12)) {
            preview.shape=TopoDS_Shape();
            preview.error="The hole cutter does not intersect the selected body.";
        }
    } catch (const Standard_Failure& e) { preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error=QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitHole(const HoleCommand& command, const TopoDS_Shape& preview, QString* error) {
    const Part* part=find(command.partId);
    if (!part) { setError(error,"The selected body no longer exists."); return false; }
    if (part->geometryToken!=command.geometryToken) {
        setError(error,"The hole preview is stale because the body geometry changed. Select the body again."); return false;
    }
    if(part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||part->hasInPlaceSketchFeature||
       !part->linkedFoldedFlatSourceId.isEmpty()){
        setError(error,"The selected body is an associative result. Edit its source bodies instead.");
        return false;
    }
    if (!validShape(preview,error)) return false;
    QVector<Part> candidate=parts_;
    for(Part& target:candidate)if(target.id==command.partId){
        target.shape=preview;
        target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    // All dependent linked Boolean/sketch bodies must regenerate in the same
    // transaction. If any descendant fails, preserve the original geometry,
    // history, and undo stack rather than leaving silently stale derived parts.
    if(!propagateLinkedBodies(&candidate,error))return false;
    checkpoint();parts_=std::move(candidate);
    return true;
}

bool SceneDocument::prepareDovetailGroove(const QString& id,double centerX,double centerY,double entryZ,
                                          double mouthWidth,double bottomWidth,double depth,double length,
                                          double angleDegrees,DovetailGrooveCommand* command,QString* error) const {
    if(!command){setError(error,"The dovetail-groove command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||
       !finitePositive(mouthWidth)||!finitePositive(bottomWidth)||bottomWidth<=mouthWidth||
       !finitePositive(depth)||!finitePositive(length)||!std::isfinite(angleDegrees)||
       angleDegrees < -360.0||angleDegrees > 360.0||
       std::abs(centerX)+bottomWidth/2.0>1e6||std::abs(centerY)+length/2.0>1e6||
       std::abs(entryZ-depth)>1e6){
        setError(error,"Dovetail groove dimensions are invalid. The undercut width must exceed the mouth width.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;
    command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->mouthWidth=mouthWidth;command->bottomWidth=bottomWidth;
    command->depth=depth;command->length=length;command->angleDegrees=angleDegrees;
    return true;
}

DovetailGroovePreview SceneDocument::buildDovetailGroovePreview(const DovetailGrooveCommand& command) {
    DovetailGroovePreview preview;
    try{
        if(command.partId.isEmpty()||command.sourceShape.IsNull()||!finiteOffset(command.centerX)||
           !finiteOffset(command.centerY)||!finiteOffset(command.entryZ)||!finitePositive(command.mouthWidth)||
           !finitePositive(command.bottomWidth)||command.bottomWidth<=command.mouthWidth||
           !finitePositive(command.depth)||!finitePositive(command.length)||!std::isfinite(command.angleDegrees)||
           command.angleDegrees < -360.0||command.angleDegrees > 360.0){
            preview.error="The dovetail-groove command is incomplete or invalid.";return preview;
        }
        const double margin=std::max(0.01,command.depth*1e-4);
        const double halfLength=command.length/2.0+margin;
        const double topZ=command.entryZ+margin;
        BRepBuilderAPI_MakePolygon polygon;
        polygon.Add(gp_Pnt(command.centerX-command.mouthWidth/2.0,command.centerY-halfLength,topZ));
        polygon.Add(gp_Pnt(command.centerX+command.mouthWidth/2.0,command.centerY-halfLength,topZ));
        polygon.Add(gp_Pnt(command.centerX+command.bottomWidth/2.0,command.centerY-halfLength,command.entryZ-command.depth));
        polygon.Add(gp_Pnt(command.centerX-command.bottomWidth/2.0,command.centerY-halfLength,command.entryZ-command.depth));
        polygon.Close();
        if(!polygon.IsDone()){preview.error="The dovetail cutter profile could not be constructed.";return preview;}
        const TopoDS_Face profile=BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
        const TopoDS_Shape straightCutter=BRepPrimAPI_MakePrism(profile,gp_Vec(0,2.0*halfLength,0)).Shape();
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(gp_Pnt(command.centerX,command.centerY,0),gp_Dir(0,0,1)),
                             command.angleDegrees*std::numbers::pi/180.0);
        const TopoDS_Shape cutter=BRepBuilderAPI_Transform(straightCutter,rotation,true).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);operation.Build();
        if(!operation.IsDone()){preview.error="The kernel could not cut the requested dovetail groove.";return preview;}
        preview.shape=operation.Shape();
        if(!validShape(preview.shape,&preview.error)){preview.shape=TopoDS_Shape();return preview;}
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);
        BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
            preview.shape=TopoDS_Shape();preview.error="The dovetail cutter does not intersect the selected body.";
        }
    }catch(const Standard_Failure& e){preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what());}
     catch(const std::exception& e){preview.error=QString::fromUtf8(e.what());}
    return preview;
}

bool SceneDocument::commitMachiningShape(const QString& bodyId,
                                         const QString& expectedRevision,
                                         const TopoDS_Shape& result,QString* error){
    const Part* original=find(bodyId);
    if(!original){setError(error,"The machining source body no longer exists.");return false;}
    if(expectedRevision.isEmpty()||original->geometryToken!=expectedRevision){
        setError(error,"The machining result is stale: the source geometry changed.");return false;
    }
    // Derived solids must be rebuilt through their definitions. A direct
    // replacement would break upstream lineage and its downstream drawing links.
    if(original->hasLinkedBoolean||original->hasLinkedSketchExtrusion||
       original->hasInPlaceSketchFeature||!original->linkedFoldedFlatSourceId.isEmpty()){
        setError(error,"Edit the linked source or folded body, not its associative result.");return false;
    }
    try{
    if(original->shape.IsNull()){
        setError(error,"The source machining solid is missing.");return false;
    }
    if(!validShape(result,error))return false;
    // A milling/drilling cut must actually remove material; a mechanically
    // valid but unchanged or material-adding solid is not a valid cut result.
    GProp_GProps sourceProps,resultProps;
    BRepGProp::VolumeProperties(original->shape,sourceProps);
    BRepGProp::VolumeProperties(result,resultProps);
    const double sourceVolume=sourceProps.Mass(),resultVolume=resultProps.Mass();
    if(!std::isfinite(sourceVolume)||sourceVolume<=1e-12||
       !std::isfinite(resultVolume)||resultVolume<=1e-12||
       sourceVolume-resultVolume<=std::max(1e-9,sourceVolume*1e-12)){
        setError(error,"Machining must remove a measurable amount of material from the source solid.");
        return false;
    }
    QVector<Part> staged=parts_;
    bool matched=false;
    for(Part& item:staged)if(item.id==bodyId){
        item.shape=result;
        item.geometryToken=generatedId();
        matched=true;
        break;
    }
    if(!matched){setError(error,"Machining target vanished during staging.");return false;}
    // Reject a cutter edit if any linked dependent fails to rebuild. Neither
    // geometry, drawing-source revisions nor undo/redo are touched on failure.
    if(!propagateLinkedBodies(&staged,error))return false;
    checkpoint();
    parts_=std::move(staged);
    if(error)error->clear();
    return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Machining regeneration failed in CAD kernel: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QString::fromUtf8(failure.what()));
    }
    return false;
}

bool SceneDocument::commitDovetailGroove(const DovetailGrooveCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The dovetail-groove preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    return commitMachiningShape(command.partId,command.geometryToken,preview,error);
}

bool SceneDocument::prepareTSlotGroove(const QString& id,double centerX,double centerY,double entryZ,
                                        double neckWidth,double undercutWidth,double depth,double undercutHeight,
                                        double length,double angleDegrees,TSlotGrooveCommand* command,QString* error) const {
    if(!command){setError(error,"The T-slot command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||
       !finitePositive(neckWidth)||!finitePositive(undercutWidth)||undercutWidth<=neckWidth||
       !finitePositive(depth)||!finitePositive(undercutHeight)||undercutHeight>=depth||
       !finitePositive(length)||!std::isfinite(angleDegrees)||angleDegrees < -360.0||angleDegrees > 360.0||
       std::abs(centerX)+undercutWidth/2.0>1e6||std::abs(centerY)+length/2.0>1e6||
       std::abs(entryZ-depth)>1e6){
        setError(error,"T-slot dimensions are invalid. The undercut must be wider than the neck and shallower than the total depth.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->neckWidth=neckWidth;command->undercutWidth=undercutWidth;
    command->depth=depth;command->undercutHeight=undercutHeight;
    command->length=length;command->angleDegrees=angleDegrees;
    return true;
}

TSlotGroovePreview SceneDocument::buildTSlotGroovePreview(const TSlotGrooveCommand& command){
    TSlotGroovePreview preview;
    try{
        if(command.partId.isEmpty()||command.sourceShape.IsNull()||!finiteOffset(command.centerX)||
           !finiteOffset(command.centerY)||!finiteOffset(command.entryZ)||!finitePositive(command.neckWidth)||
           !finitePositive(command.undercutWidth)||command.undercutWidth<=command.neckWidth||
           !finitePositive(command.depth)||!finitePositive(command.undercutHeight)||command.undercutHeight>=command.depth||
           !finitePositive(command.length)||!std::isfinite(command.angleDegrees)||
           command.angleDegrees < -360.0||command.angleDegrees > 360.0){
            preview.error="The T-slot command is incomplete or invalid.";return preview;
        }
        const double margin=std::max(0.01,command.depth*1e-4);
        const double halfLength=command.length/2.0+margin;
        const double topZ=command.entryZ+margin;
        const double shoulderZ=command.entryZ-command.depth+command.undercutHeight;
        const double bottomZ=command.entryZ-command.depth;
        BRepBuilderAPI_MakePolygon polygon;
        polygon.Add(gp_Pnt(command.centerX-command.neckWidth/2.0,command.centerY-halfLength,topZ));
        polygon.Add(gp_Pnt(command.centerX+command.neckWidth/2.0,command.centerY-halfLength,topZ));
        polygon.Add(gp_Pnt(command.centerX+command.neckWidth/2.0,command.centerY-halfLength,shoulderZ));
        polygon.Add(gp_Pnt(command.centerX+command.undercutWidth/2.0,command.centerY-halfLength,shoulderZ));
        polygon.Add(gp_Pnt(command.centerX+command.undercutWidth/2.0,command.centerY-halfLength,bottomZ));
        polygon.Add(gp_Pnt(command.centerX-command.undercutWidth/2.0,command.centerY-halfLength,bottomZ));
        polygon.Add(gp_Pnt(command.centerX-command.undercutWidth/2.0,command.centerY-halfLength,shoulderZ));
        polygon.Add(gp_Pnt(command.centerX-command.neckWidth/2.0,command.centerY-halfLength,shoulderZ));
        polygon.Close();
        if(!polygon.IsDone()){preview.error="The T-slot cutter profile could not be constructed.";return preview;}
        const TopoDS_Face profile=BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
        const TopoDS_Shape straightCutter=BRepPrimAPI_MakePrism(profile,gp_Vec(0,2.0*halfLength,0)).Shape();
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(gp_Pnt(command.centerX,command.centerY,0),gp_Dir(0,0,1)),
                             command.angleDegrees*std::numbers::pi/180.0);
        const TopoDS_Shape cutter=BRepBuilderAPI_Transform(straightCutter,rotation,true).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);operation.Build();
        if(!operation.IsDone()){preview.error="The kernel could not cut the requested T-slot.";return preview;}
        preview.shape=operation.Shape();
        if(!validShape(preview.shape,&preview.error)){preview.shape=TopoDS_Shape();return preview;}
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);
        BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
            preview.shape=TopoDS_Shape();preview.error="The T-slot cutter does not intersect the selected body.";
        }
    }catch(const Standard_Failure& e){preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what());}
     catch(const std::exception& e){preview.error=QString::fromUtf8(e.what());}
    return preview;
}

bool SceneDocument::commitTSlotGroove(const TSlotGrooveCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The T-slot preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    return commitMachiningShape(command.partId,command.geometryToken,preview,error);
}

bool SceneDocument::prepareRoundedSlot(const QString& id,double centerX,double centerY,double entryZ,
                                        double width,double length,double depth,double angleDegrees,
                                        RoundedSlotCommand* command,QString* error) const {
    if(!command){setError(error,"The rounded-slot command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||
       !finitePositive(width)||!finitePositive(length)||length<width||!finitePositive(depth)||
       !std::isfinite(angleDegrees)||angleDegrees < -360.0||angleDegrees > 360.0||
       std::abs(centerX)+length/2.0>1e6||std::abs(centerY)+length/2.0>1e6||
       std::abs(entryZ-depth)>1e6){
        setError(error,"Rounded-slot dimensions are invalid. Length must be at least the slot width.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->width=width;command->length=length;command->depth=depth;command->angleDegrees=angleDegrees;
    return true;
}

RoundedSlotPreview SceneDocument::buildRoundedSlotPreview(const RoundedSlotCommand& command){
    RoundedSlotPreview preview;
    try{
        if(command.partId.isEmpty()||command.sourceShape.IsNull()||!finiteOffset(command.centerX)||
           !finiteOffset(command.centerY)||!finiteOffset(command.entryZ)||!finitePositive(command.width)||
           !finitePositive(command.length)||command.length<command.width||!finitePositive(command.depth)||
           !std::isfinite(command.angleDegrees)||command.angleDegrees < -360.0||command.angleDegrees > 360.0){
            preview.error="The rounded-slot command is incomplete or invalid.";return preview;
        }
        const double radius=command.width/2.0;
        const double endOffset=(command.length-command.width)/2.0;
        const double margin=std::max(0.01,command.depth*1e-4);
        const double bottomZ=command.entryZ-command.depth;
        TopoDS_Shape cutter;
        if(endOffset>1e-8){
            cutter=BRepPrimAPI_MakeBox(gp_Pnt(command.centerX-radius,command.centerY-endOffset,bottomZ),
                                       command.width,2.0*endOffset,command.depth+margin).Shape();
        }
        const TopoDS_Shape firstEnd=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(command.centerX,command.centerY-endOffset,bottomZ),gp_Dir(0,0,1)),
            radius,command.depth+margin).Shape();
        if(endOffset<=1e-8)cutter=firstEnd;
        else {
            const TopoDS_Shape secondEnd=BRepPrimAPI_MakeCylinder(
                gp_Ax2(gp_Pnt(command.centerX,command.centerY+endOffset,bottomZ),gp_Dir(0,0,1)),
                radius,command.depth+margin).Shape();
            BRepAlgoAPI_Fuse firstJoin(cutter,firstEnd);firstJoin.Build();
            if(!firstJoin.IsDone()){preview.error="The rounded-slot cutter could not be joined.";return preview;}
            BRepAlgoAPI_Fuse fullJoin(firstJoin.Shape(),secondEnd);fullJoin.Build();
            if(!fullJoin.IsDone()){preview.error="The rounded-slot cutter could not be joined.";return preview;}
            cutter=fullJoin.Shape();
        }
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(gp_Pnt(command.centerX,command.centerY,0),gp_Dir(0,0,1)),
                             command.angleDegrees*std::numbers::pi/180.0);
        cutter=BRepBuilderAPI_Transform(cutter,rotation,true).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);operation.Build();
        if(!operation.IsDone()){preview.error="The kernel could not cut the requested rounded slot.";return preview;}
        preview.shape=operation.Shape();
        if(!validShape(preview.shape,&preview.error)){preview.shape=TopoDS_Shape();return preview;}
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
            preview.shape=TopoDS_Shape();preview.error="The rounded-slot cutter does not intersect the selected body.";
        }
    }catch(const Standard_Failure& e){preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what());}
     catch(const std::exception& e){preview.error=QString::fromUtf8(e.what());}
    return preview;
}

bool SceneDocument::commitRoundedSlot(const RoundedSlotCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The rounded-slot preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    return commitMachiningShape(command.partId,command.geometryToken,preview,error);
}

bool SceneDocument::prepareKeyholeSlot(const QString& id,double centerX,double centerY,double entryZ,
                                        double headDiameter,double throatWidth,double throatLength,double depth,
                                        double angleDegrees,KeyholeSlotCommand* command,QString* error) const {
    if(!command){setError(error,"The keyhole-slot command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||
       !finitePositive(headDiameter)||!finitePositive(throatWidth)||throatWidth>=headDiameter||
       !finitePositive(throatLength)||!finitePositive(depth)||!std::isfinite(angleDegrees)||
       angleDegrees < -360.0||angleDegrees > 360.0||
       std::abs(centerX)+headDiameter/2.0+throatLength>1e6||
       std::abs(centerY)+headDiameter/2.0+throatLength>1e6||std::abs(entryZ-depth)>1e6){
        setError(error,"Keyhole-slot dimensions are invalid. The throat must be narrower than the round head.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->headDiameter=headDiameter;command->throatWidth=throatWidth;
    command->throatLength=throatLength;command->depth=depth;command->angleDegrees=angleDegrees;
    return true;
}

KeyholeSlotPreview SceneDocument::buildKeyholeSlotPreview(const KeyholeSlotCommand& command){
    KeyholeSlotPreview preview;
    try{
        if(command.partId.isEmpty()||command.sourceShape.IsNull()||!finiteOffset(command.centerX)||
           !finiteOffset(command.centerY)||!finiteOffset(command.entryZ)||!finitePositive(command.headDiameter)||
           !finitePositive(command.throatWidth)||command.throatWidth>=command.headDiameter||
           !finitePositive(command.throatLength)||!finitePositive(command.depth)||
           !std::isfinite(command.angleDegrees)||command.angleDegrees < -360.0||command.angleDegrees > 360.0){
            preview.error="The keyhole-slot command is incomplete or invalid.";return preview;
        }
        const double margin=std::max(0.01,command.depth*1e-4);
        const double bottomZ=command.entryZ-command.depth;
        const double height=command.depth+margin;
        const TopoDS_Shape head=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(command.centerX,command.centerY,bottomZ),gp_Dir(0,0,1)),
            command.headDiameter/2.0,height).Shape();
        const TopoDS_Shape throat=BRepPrimAPI_MakeBox(
            gp_Pnt(command.centerX-command.throatWidth/2.0,command.centerY,bottomZ),
            command.throatWidth,command.throatLength,height).Shape();
        BRepAlgoAPI_Fuse cutterJoin(head,throat);cutterJoin.Build();
        if(!cutterJoin.IsDone()){preview.error="The keyhole-slot cutter could not be joined.";return preview;}
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(gp_Pnt(command.centerX,command.centerY,0),gp_Dir(0,0,1)),
                             command.angleDegrees*std::numbers::pi/180.0);
        const TopoDS_Shape cutter=BRepBuilderAPI_Transform(cutterJoin.Shape(),rotation,true).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);operation.Build();
        if(!operation.IsDone()){preview.error="The kernel could not cut the requested keyhole slot.";return preview;}
        preview.shape=operation.Shape();
        if(!validShape(preview.shape,&preview.error)){preview.shape=TopoDS_Shape();return preview;}
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
            preview.shape=TopoDS_Shape();preview.error="The keyhole-slot cutter does not intersect the selected body.";
        }
    }catch(const Standard_Failure& e){preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what());}
     catch(const std::exception& e){preview.error=QString::fromUtf8(e.what());}
    return preview;
}

bool SceneDocument::commitKeyholeSlot(const KeyholeSlotCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The keyhole-slot preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    return commitMachiningShape(command.partId,command.geometryToken,preview,error);
}

bool SceneDocument::prepareRoundedSlotPattern(const QString& id,double centerX,double centerY,double entryZ,
                                               double width,double length,double depth,double angleDegrees,
                                               int instanceCount,double spacingX,double spacingY,
                                               RoundedSlotPatternCommand* command,QString* error) const {
    if(!command){setError(error,"The rounded-slot pattern command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(width)||
       !finitePositive(length)||length<width||!finitePositive(depth)||!std::isfinite(angleDegrees)||
       angleDegrees < -360.0||angleDegrees > 360.0||instanceCount<2||instanceCount>100||
       !std::isfinite(spacingX)||!std::isfinite(spacingY)||
       (std::abs(spacingX)<1e-5&&std::abs(spacingY)<1e-5)||
       std::max(std::abs(spacingX),std::abs(spacingY))*(instanceCount-1)>1e6||
       std::abs(centerX)+std::abs(spacingX)*(instanceCount-1)+length/2.0>1e6||
       std::abs(centerY)+std::abs(spacingY)*(instanceCount-1)+length/2.0>1e6||
       std::abs(entryZ-depth)>1e6){
        setError(error,"Rounded-slot pattern dimensions are invalid. Use 2 to 100 instances and nonzero spacing.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->width=width;command->length=length;command->depth=depth;command->angleDegrees=angleDegrees;
    command->instanceCount=instanceCount;command->spacingX=spacingX;command->spacingY=spacingY;
    return true;
}

RoundedSlotPatternPreview SceneDocument::buildRoundedSlotPatternPreview(const RoundedSlotPatternCommand& command){
    RoundedSlotPatternPreview preview;
    if(command.sourceShape.IsNull()||command.partId.isEmpty()||command.instanceCount<2||command.instanceCount>100||
       !std::isfinite(command.spacingX)||!std::isfinite(command.spacingY)||
       (std::abs(command.spacingX)<1e-5&&std::abs(command.spacingY)<1e-5)){
        preview.error="The rounded-slot pattern command is incomplete or invalid.";return preview;
    }
    TopoDS_Shape working=command.sourceShape;
    GProp_GProps before,after;BRepGProp::VolumeProperties(command.sourceShape,before);
    for(int i=0;i<command.instanceCount;++i){
        RoundedSlotCommand slot;
        slot.partId=command.partId;slot.geometryToken=command.geometryToken;slot.sourceShape=working;
        slot.centerX=command.centerX+command.spacingX*i;
        slot.centerY=command.centerY+command.spacingY*i;
        slot.entryZ=command.entryZ;slot.width=command.width;slot.length=command.length;
        slot.depth=command.depth;slot.angleDegrees=command.angleDegrees;
        const RoundedSlotPreview slotPreview=buildRoundedSlotPreview(slot);
        if(slotPreview.shape.IsNull()){
            preview.error=QStringLiteral("Rounded slot %1 failed: %2").arg(i+1).arg(slotPreview.error);return preview;
        }
        working=slotPreview.shape;
    }
    if(!validShape(working,&preview.error))return preview;
    BRepGProp::VolumeProperties(working,after);
    const double removed=before.Mass()-after.Mass();
    if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
        preview.error="The rounded-slot pattern did not remove material.";return preview;
    }
    preview.shape=working;
    return preview;
}

bool SceneDocument::commitRoundedSlotPattern(const RoundedSlotPatternCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The rounded-slot pattern preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    return commitMachiningShape(command.partId,command.geometryToken,preview,error);
}

bool SceneDocument::holePattern(const QString& id,double x,double y,double entryZ,double diameter,double depth,bool throughAll,
                                int countX,double spacingX,int countY,double spacingY,QString* error) {
    if(countX<1||countY<1||countX>1000||countY>1000||countX>1000/countY||countX*countY<2){
        setError(error,"Hole grid must contain between 2 and 1,000 instances.");return false;
    }
    auto spacingValid=[](int count,double spacing){return std::isfinite(spacing)&&std::abs(spacing)<=1e6&&
        (count==1||(std::abs(spacing)>=1e-5&&std::abs(spacing)*(count-1)<=1e6));};
    if(!finiteOffset(x)||!finiteOffset(y)||!finiteOffset(entryZ)||!finitePositive(diameter)||
       (!throughAll&&!finitePositive(depth))||!spacingValid(countX,spacingX)||!spacingValid(countY,spacingY)){
        setError(error,"Hole-grid dimensions, location, or spacing are invalid.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString sourceToken=source->geometryToken;
    TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    for(int ix=0;ix<countX;++ix)for(int iy=0;iy<countY;++iy){
        HoleCommand command;command.partId=id;command.geometryToken=sourceToken;command.sourceShape=working;
        command.x=x+spacingX*ix;command.y=y+spacingY*iy;command.entryZ=entryZ;
        command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){setError(error,QStringLiteral("Hole at grid position %1, %2 failed: %3").arg(ix+1).arg(iy+1).arg(preview.error));return false;}
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    return commitMachiningShape(id,sourceToken,working,error);
}

bool SceneDocument::circularHolePattern(const QString& id,double centerX,double centerY,double entryZ,double boltCircleRadius,
                                       double diameter,double depth,bool throughAll,int instanceCount,
                                       double totalAngleDegrees,QString* error) {
    if(instanceCount<2||instanceCount>1000){setError(error,"Circular hole pattern count must be between 2 and 1,000.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(boltCircleRadius)||
       std::abs(centerX)+boltCircleRadius>1e6||std::abs(centerY)+boltCircleRadius>1e6||!finitePositive(diameter)||
       (!throughAll&&!finitePositive(depth))||!std::isfinite(totalAngleDegrees)||totalAngleDegrees<1||totalAngleDegrees>360){
        setError(error,"Circular hole pattern location, dimensions, or angle are invalid.");return false;
    }
    const Part* source=find(id);
    if(!source){setError(error,"Object not found.");return false;}
    const QString geometryToken=source->geometryToken;
    TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle?instanceCount:instanceCount-1;
    for(int index=0;index<instanceCount;++index){
        const double angle=totalAngleDegrees*index/denominator*std::numbers::pi/180.0;
        HoleCommand command;command.partId=id;command.geometryToken=geometryToken;command.sourceShape=working;
        command.x=centerX+boltCircleRadius*std::cos(angle);command.y=centerY+boltCircleRadius*std::sin(angle);
        command.entryZ=entryZ;command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Hole %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    return commitMachiningShape(id,geometryToken,working,error);
}

bool SceneDocument::circularCountersinkPattern(const QString& id,double centerX,double centerY,double entryZ,double boltCircleRadius,
                                               double diameter,double depth,bool throughAll,double headDiameter,
                                               double includedAngleDegrees,int instanceCount,double totalAngleDegrees,
                                               QString* error){
    if(instanceCount<2||instanceCount>1000){setError(error,"Circular countersink pattern count must be between 2 and 1,000.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(boltCircleRadius)||
       std::abs(centerX)+boltCircleRadius>1e6||std::abs(centerY)+boltCircleRadius>1e6||!finitePositive(diameter)||
       !finitePositive(headDiameter)||headDiameter<=diameter||(!throughAll&&!finitePositive(depth))||
       !std::isfinite(includedAngleDegrees)||includedAngleDegrees<=1||includedAngleDegrees>=179||
       !std::isfinite(totalAngleDegrees)||totalAngleDegrees<1||totalAngleDegrees>360){
        setError(error,"Circular countersink pattern location, dimensions, angle, or count are invalid.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString geometryToken=source->geometryToken;
    TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle?instanceCount:instanceCount-1;
    for(int index=0;index<instanceCount;++index){
        const double angle=totalAngleDegrees*index/denominator*std::numbers::pi/180.0;
        HoleCommand command;command.partId=id;command.geometryToken=geometryToken;command.sourceShape=working;
        command.x=centerX+boltCircleRadius*std::cos(angle);command.y=centerY+boltCircleRadius*std::sin(angle);
        command.entryZ=entryZ;command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        command.style=HoleStyle::Countersink;command.headDiameter=headDiameter;command.sinkAngleDegrees=includedAngleDegrees;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Countersink %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    return commitMachiningShape(id,geometryToken,working,error);
}

bool SceneDocument::circularCounterborePattern(const QString& id,double centerX,double centerY,double entryZ,double boltCircleRadius,
                                               double diameter,double depth,bool throughAll,double headDiameter,double headDepth,
                                               int instanceCount,double totalAngleDegrees,QString* error){
    if(instanceCount<2||instanceCount>1000){setError(error,"Circular counterbore pattern count must be between 2 and 1,000.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(boltCircleRadius)||
       std::abs(centerX)+boltCircleRadius>1e6||std::abs(centerY)+boltCircleRadius>1e6||!finitePositive(diameter)||
       !finitePositive(headDiameter)||headDiameter<=diameter||!finitePositive(headDepth)||
       (!throughAll&&(!finitePositive(depth)||headDepth>depth))||!std::isfinite(totalAngleDegrees)||
       totalAngleDegrees<1||totalAngleDegrees>360){
        setError(error,"Circular counterbore pattern location, dimensions, depth, angle, or count are invalid.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString geometryToken=source->geometryToken;
    TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle?instanceCount:instanceCount-1;
    for(int index=0;index<instanceCount;++index){
        const double angle=totalAngleDegrees*index/denominator*std::numbers::pi/180.0;
        HoleCommand command;command.partId=id;command.geometryToken=geometryToken;command.sourceShape=working;
        command.x=centerX+boltCircleRadius*std::cos(angle);command.y=centerY+boltCircleRadius*std::sin(angle);
        command.entryZ=entryZ;command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        command.style=HoleStyle::Counterbore;command.headDiameter=headDiameter;command.headDepth=headDepth;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Counterbore %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    return commitMachiningShape(id,geometryToken,working,error);
}

bool SceneDocument::linearCounterborePattern(const QString& id,double centerX,double centerY,double entryZ,double diameter,
                                             double depth,bool throughAll,double headDiameter,double headDepth,
                                             int instanceCount,double spacingX,double spacingY,
                                             QString* error){
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(diameter)||
       !finitePositive(headDiameter)||headDiameter<=diameter||!finitePositive(headDepth)||
       (!throughAll&&(!finitePositive(depth)||headDepth>depth))||instanceCount<2||instanceCount>100||
       !std::isfinite(spacingX)||!std::isfinite(spacingY)||(std::abs(spacingX)<1e-5&&std::abs(spacingY)<1e-5)||
       std::max(std::abs(spacingX),std::abs(spacingY))*(instanceCount-1)>1e6||
       std::abs(centerX)+std::abs(spacingX)*(instanceCount-1)+headDiameter>1e6||
       std::abs(centerY)+std::abs(spacingY)*(instanceCount-1)+headDiameter>1e6||std::abs(entryZ-depth)>1e6){
        setError(error,"Counterbore pattern dimensions are invalid. Use 2 to 100 instances and nonzero spacing.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString token=source->geometryToken;TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    for(int index=0;index<instanceCount;++index){
        HoleCommand command;command.partId=id;command.geometryToken=token;command.sourceShape=working;
        command.x=centerX+spacingX*index;command.y=centerY+spacingY*index;command.entryZ=entryZ;
        command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        command.style=HoleStyle::Counterbore;command.headDiameter=headDiameter;command.headDepth=headDepth;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Counterbore %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    return commitMachiningShape(id,token,working,error);
}

bool SceneDocument::linearCountersinkPattern(const QString& id,double centerX,double centerY,double entryZ,double diameter,
                                             double depth,bool throughAll,double headDiameter,double includedAngleDegrees,
                                             int instanceCount,double spacingX,double spacingY,QString* error){
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(diameter)||
       !finitePositive(headDiameter)||headDiameter<=diameter||(!throughAll&&!finitePositive(depth))||
       !std::isfinite(includedAngleDegrees)||includedAngleDegrees<=1||includedAngleDegrees>=179||
       instanceCount<2||instanceCount>100||!std::isfinite(spacingX)||!std::isfinite(spacingY)||
       (std::abs(spacingX)<1e-5&&std::abs(spacingY)<1e-5)||
       std::max(std::abs(spacingX),std::abs(spacingY))*(instanceCount-1)>1e6||
       std::abs(centerX)+std::abs(spacingX)*(instanceCount-1)+headDiameter>1e6||
       std::abs(centerY)+std::abs(spacingY)*(instanceCount-1)+headDiameter>1e6||std::abs(entryZ-depth)>1e6){
        setError(error,"Countersink pattern dimensions are invalid. Use 2 to 100 instances and nonzero spacing.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString token=source->geometryToken;TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    for(int index=0;index<instanceCount;++index){
        HoleCommand command;command.partId=id;command.geometryToken=token;command.sourceShape=working;
        command.x=centerX+spacingX*index;command.y=centerY+spacingY*index;command.entryZ=entryZ;
        command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        command.style=HoleStyle::Countersink;command.headDiameter=headDiameter;command.sinkAngleDegrees=includedAngleDegrees;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Countersink %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    return commitMachiningShape(id,token,working,error);
}

bool SceneDocument::linearKeyholeSlotPattern(const QString& id,double centerX,double centerY,double entryZ,double headDiameter,
                                              double throatWidth,double throatLength,double depth,double angleDegrees,
                                              int instanceCount,double spacingX,double spacingY,QString* error){
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(headDiameter)||
       !finitePositive(throatWidth)||throatWidth>=headDiameter||!finitePositive(throatLength)||!finitePositive(depth)||
       !std::isfinite(angleDegrees)||angleDegrees < -360||angleDegrees > 360||instanceCount<2||instanceCount>100||
       !std::isfinite(spacingX)||!std::isfinite(spacingY)||(std::abs(spacingX)<1e-5&&std::abs(spacingY)<1e-5)||
       std::max(std::abs(spacingX),std::abs(spacingY))*(instanceCount-1)>1e6||
       std::abs(centerX)+std::abs(spacingX)*(instanceCount-1)+throatLength+headDiameter>1e6||
       std::abs(centerY)+std::abs(spacingY)*(instanceCount-1)+throatLength+headDiameter>1e6||std::abs(entryZ-depth)>1e6){
        setError(error,"Keyhole-slot pattern dimensions are invalid. Use 2 to 100 instances and nonzero spacing.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString token=source->geometryToken;TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    for(int index=0;index<instanceCount;++index){
        KeyholeSlotCommand command;command.partId=id;command.geometryToken=token;command.sourceShape=working;
        command.centerX=centerX+spacingX*index;command.centerY=centerY+spacingY*index;command.entryZ=entryZ;
        command.headDiameter=headDiameter;command.throatWidth=throatWidth;command.throatLength=throatLength;
        command.depth=depth;command.angleDegrees=angleDegrees;
        const KeyholeSlotPreview preview=buildKeyholeSlotPreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Keyhole slot %1 failed: %2").arg(index+1).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    return commitMachiningShape(id,token,working,error);
}

bool SceneDocument::circularPattern(const QString& id, Axis axis, double centerU, double centerV,
                                   int instanceCount, double totalAngleDegrees, QString* error) {
    if (instanceCount<2 || instanceCount>1000) {
        setError(error,"Circular pattern count must be between 2 and 1,000 total instances."); return false;
    }
    if (!finiteOffset(centerU) || !finiteOffset(centerV)) { setError(error,"Pattern center is out of range."); return false; }
    if (!std::isfinite(totalAngleDegrees) || totalAngleDegrees<1.0 || totalAngleDegrees>360.0) {
        setError(error,"Circular pattern angle must be between 1 and 360 degrees."); return false;
    }
    if (parts_.size()+instanceCount-1>kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const TopoDS_Shape sourceShape=source->shape;
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    gp_Pnt center(0,0,0);
    gp_Dir direction(0,0,1);
    if (axis==Axis::X) { center.SetY(centerU); center.SetZ(centerV); direction=gp_Dir(1,0,0); }
    else if (axis==Axis::Y) { center.SetX(centerU); center.SetZ(centerV); direction=gp_Dir(0,1,0); }
    else { center.SetX(centerU); center.SetY(centerV); direction=gp_Dir(0,0,1); }
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle ? instanceCount : instanceCount-1;
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try {
        for (int index=1;index<instanceCount;++index) {
            const double angle=totalAngleDegrees*index/denominator*std::numbers::pi/180.0;
            gp_Trsf transform;
            transform.SetRotation(gp_Ax1(center,direction),angle);
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if (!validShape(result,error)) return false;
            const QString baseName=sourceName+QStringLiteral(" Circular Pattern %1").arg(index+1);
            QString name=uniqueName(baseName);
            int suffix=2;
            while (reservedNames.contains(name.toCaseFolded())) name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
            inheritMaterial(copies.last(), *source);
        }
        checkpoint();
        for (const Part& copy:copies) parts_.append(copy);
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::circularPatternAboutAxis(const QString& id,double originX,double originY,double originZ,
                                             double axisX,double axisY,double axisZ,int instanceCount,
                                             double totalAngleDegrees,QString* error) {
    if(instanceCount<2||instanceCount>1000){setError(error,"Circular pattern count must be between 2 and 1,000.");return false;}
    const double axisMagnitude=std::hypot(axisX,axisY,axisZ);
    if(!finiteOffset(originX)||!finiteOffset(originY)||!finiteOffset(originZ)||
       !std::isfinite(axisX)||!std::isfinite(axisY)||!std::isfinite(axisZ)||
       !std::isfinite(axisMagnitude)||axisMagnitude<1e-8||axisMagnitude>1e6){
        setError(error,"Enter a finite axis origin and a finite non-zero axis direction.");return false;
    }
    if(!std::isfinite(totalAngleDegrees)||totalAngleDegrees<1||totalAngleDegrees>360){
        setError(error,"Circular pattern angle must be between 1 and 360 degrees.");return false;
    }
    if(parts_.size()+instanceCount-1>kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const TopoDS_Shape sourceShape=source->shape;const QString sourceName=source->name;
    const bool visible=source->visible;const QString color=source->color;
    const gp_Ax1 axis(gp_Pnt(originX,originY,originZ),gp_Dir(axisX,axisY,axisZ));
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle?instanceCount:instanceCount-1;
    QVector<Part> copies;QSet<QString> reservedNames;
    try{
        for(int index=1;index<instanceCount;++index){
            gp_Trsf transform;
            transform.SetRotation(axis,totalAngleDegrees*index/denominator*std::numbers::pi/180.0);
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if(!validShape(result,error))return false;
            const QString baseName=sourceName+QStringLiteral(" Axis Pattern %1").arg(index+1);
            QString name=uniqueName(baseName);int suffix=2;
            while(reservedNames.contains(name.toCaseFolded()))name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());copies.append({generatedId(),name,result,visible,color});
            inheritMaterial(copies.last(), *source);
        }
        checkpoint();for(const Part& copy:copies)parts_.append(copy);return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::splitBody(const QString& id, Axis axis, double planeOffset, QString* error) {
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    if(rejectSurfaceOnlyInput(*source,QStringLiteral("Split solid"),error))return false;
    if (!finiteOffset(planeOffset)) { setError(error,"Split plane offset must be finite and within +/-1,000,000 mm."); return false; }
    if (axis!=Axis::X && axis!=Axis::Y && axis!=Axis::Z) { setError(error,"Choose X, Y, or Z as the split plane normal."); return false; }
    Bnd_Box bounds;
    BRepBndLib::Add(source->shape,bounds);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const double low=axis==Axis::X ? xmin : axis==Axis::Y ? ymin : zmin;
    const double high=axis==Axis::X ? xmax : axis==Axis::Y ? ymax : zmax;
    if (planeOffset<=low+1e-8 || planeOffset>=high-1e-8) {
        setError(error,"The split plane must pass through the interior of the selected body."); return false;
    }
    const double margin=std::max({xmax-xmin,ymax-ymin,zmax-zmin,1.0})*2.0+1.0;
    const double ax0=xmin-margin, ay0=ymin-margin, az0=zmin-margin;
    const double ax1=xmax+margin, ay1=ymax+margin, az1=zmax+margin;
    double leftX=ax0,leftY=ay0,leftZ=az0,rightX=ax0,rightY=ay0,rightZ=az0;
    double leftX1=ax1,leftY1=ay1,leftZ1=az1,rightX1=ax1,rightY1=ay1,rightZ1=az1;
    if (axis==Axis::X) { leftX1=planeOffset; rightX=planeOffset; }
    else if (axis==Axis::Y) { leftY1=planeOffset; rightY=planeOffset; }
    else { leftZ1=planeOffset; rightZ=planeOffset; }
    try {
        BRepAlgoAPI_Common lower(source->shape,BRepPrimAPI_MakeBox(gp_Pnt(leftX,leftY,leftZ),gp_Pnt(leftX1,leftY1,leftZ1)).Shape());
        lower.Build();
        BRepAlgoAPI_Common upper(source->shape,BRepPrimAPI_MakeBox(gp_Pnt(rightX,rightY,rightZ),gp_Pnt(rightX1,rightY1,rightZ1)).Shape());
        upper.Build();
        if (!lower.IsDone() || !upper.IsDone()) { setError(error,"OCCT could not split the body at that plane."); return false; }
        TopoDS_Shape lowerShape=lower.Shape(),upperShape=upper.Shape();
        if (!validShape(lowerShape,error) || !validShape(upperShape,error)) return false;
        const Part sourceCopy=*source;
        Part second=sourceCopy;
        second.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        second.name=uniqueName(sourceCopy.name+QStringLiteral(" Split 2"));
        Part first=sourceCopy;
        first.shape=lowerShape;
        second.shape=upperShape;
        checkpoint();
        for (auto& part:parts_) if (part.id==id) { part=std::move(first); break; }
        parts_.append(std::move(second));
        return true;
    } catch (const Standard_Failure& failure) {
        setError(error,QStringLiteral("OCCT split failed: %1").arg(QString::fromUtf8(failure.what())));
        return false;
    }
}

bool SceneDocument::shellBody(const QString& id, Axis openingAxis, bool positiveSide,
                              double thickness, QString* error) {
    return shellBodyWithOpenings(id,{{openingAxis,positiveSide}},thickness,error);
}

bool SceneDocument::shellBodyWithOpenings(const QString& id,const QVector<ShellOpeningFace>& requestedOpenings,
                                          double thickness,QString* error) {
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    if(rejectSurfaceOnlyInput(*source,QStringLiteral("Shell"),error))return false;
    if (requestedOpenings.isEmpty() || requestedOpenings.size()>6 || !finitePositive(thickness)) {
        setError(error,"Choose one to six unique planar opening faces and a positive shell thickness."); return false;
    }
    int requestedSideMask=0;
    for(const ShellOpeningFace& item:requestedOpenings){
        if(item.axis!=Axis::X&&item.axis!=Axis::Y&&item.axis!=Axis::Z){
            setError(error,"Choose a valid opening direction.");return false;
        }
        const int side=static_cast<int>(item.axis)*2+(item.positiveSide?1:0);
        const int sideBit=1<<side;
        if(requestedSideMask&sideBit){setError(error,"An opening face may only be selected once.");return false;}
        requestedSideMask|=sideBit;
    }
    Bnd_Box bounds;
    BRepBndLib::Add(source->shape,bounds);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const double scale=std::max({xmax-xmin,ymax-ymin,zmax-zmin,1.0});
    QVector<TopoDS_Face> openings;
    for(int side=0;side<6;++side){
        if(!(requestedSideMask&(1<<side)))continue;
        const Axis axis=static_cast<Axis>(side/2);
        const bool positive=(side%2)==1;
        const double target=axis==Axis::X?(positive?xmax:xmin):
                            axis==Axis::Y?(positive?ymax:ymin):(positive?zmax:zmin);
        const gp_Dir requestedAxis=axis==Axis::X?gp_Dir(1,0,0):axis==Axis::Y?gp_Dir(0,1,0):gp_Dir(0,0,1);
        TopoDS_Face opening;
        double best=std::numeric_limits<double>::infinity();
        for(TopExp_Explorer explorer(source->shape,TopAbs_FACE);explorer.More();explorer.Next()){
            const TopoDS_Face face=TopoDS::Face(explorer.Current());
            BRepAdaptor_Surface surface(face,true);
            if(surface.GetType()!=GeomAbs_Plane)continue;
            const gp_Dir normal=surface.Plane().Axis().Direction();
            if(std::abs(normal.Dot(requestedAxis))<1.0-1e-7)continue;
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(face,properties);
            const gp_Pnt center=properties.CentreOfMass();
            const double coordinate=axis==Axis::X?center.X():axis==Axis::Y?center.Y():center.Z();
            const double distance=std::abs(coordinate-target);
            if(distance<best&&distance<scale*1e-5+1e-7){best=distance;opening=face;}
        }
        if(opening.IsNull()){
            setError(error,QStringLiteral("No planar face was found on the %1%2 side of the selected body.")
                     .arg(positive?QStringLiteral("+"):QStringLiteral("-"))
                     .arg(axis==Axis::X?QStringLiteral("X"):axis==Axis::Y?QStringLiteral("Y"):QStringLiteral("Z")));
            return false;
        }
        openings.append(opening);
    }
    try {
        NCollection_List<TopoDS_Shape> openingList;
        for(const TopoDS_Face& opening:openings)openingList.Append(opening);
        BRepOffsetAPI_MakeThickSolid operation;
        operation.MakeThickSolidByJoin(source->shape,openingList,-thickness,1e-3,BRepOffset_Skin);
        if (!operation.IsDone()) { setError(error,"OCCT could not shell the body. Try a smaller thickness or another opening face."); return false; }
        TopoDS_Shape result=operation.Shape();
        if (!validShape(result,error)) return false;
        GProp_GProps oldVolume,newVolume;
        BRepGProp::VolumeProperties(source->shape,oldVolume);
        BRepGProp::VolumeProperties(result,newVolume);
        if (newVolume.Mass()>=oldVolume.Mass()-1e-9) { setError(error,"Shell did not remove material from the selected body."); return false; }
        checkpoint();
        for (auto& part:parts_) if (part.id==id) { part.shape=result; break; }
        return true;
    } catch (const Standard_Failure& failure) {
        setError(error,QStringLiteral("OCCT shell failed: %1").arg(QString::fromUtf8(failure.what())));
        return false;
    }
}

bool SceneDocument::rename(const QString& id, const QString& name, QString* error) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 120 || trimmed.contains(QLatin1Char('\n'))) {
        setError(error, "Names must contain 1â€“120 characters on a single line."); return false;
    }
    for (const auto& part : parts_) if (part.id != id && part.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
        setError(error, "Another body already uses this name."); return false;
    }
    for (const auto& part : parts_) if (part.id == id) {
        if (part.name == trimmed) return true;
        checkpoint();
        // A checkpoint can reallocate the vector, so never hold an element reference across it.
        for (auto& item : parts_) if (item.id == id) { item.name = trimmed; break; }
        return true;
    }
    setError(error, "Object not found."); return false;
}

bool SceneDocument::renameBatch(const QMap<QString,QString>& desired, QString* error) {
    if(desired.isEmpty()) { setError(error,"Select at least one body to rename."); return false; }
    // Validate the final state across ALL bodies, including untouched bodies.
    // This permits intentional swaps A->B and B->A while rejecting collisions.
    QSet<QString> finalNames;
    bool changed=false;
    for(const auto& part:parts_) {
        const auto proposal=desired.constFind(part.id);
        const QString candidate=proposal==desired.cend()?part.name:proposal.value().trimmed();
        if(candidate.isEmpty()||candidate.size()>120||candidate.contains('\n')||candidate.contains('\r')){
            setError(error,QStringLiteral("Invalid single-line body name for %1 (maximum 120 characters).").arg(part.id));
            return false;
        }
        const QString identity=candidate.toCaseFolded();
        if(finalNames.contains(identity)){
            setError(error,QStringLiteral("Duplicate final body name: %1").arg(candidate));return false;
        }
        finalNames.insert(identity);
        if(proposal!=desired.cend()&&candidate!=part.name)changed=true;
    }
    for(auto it=desired.cbegin();it!=desired.cend();++it){
        if(!find(it.key())){
            setError(error,QStringLiteral("Selected body %1 no longer exists.").arg(it.key()));return false;
        }
    }
    if(!changed)return true;
    checkpoint();
    for(auto& part:parts_){
        const auto proposal=desired.constFind(part.id);
        if(proposal!=desired.cend())part.name=proposal.value().trimmed();
    }
    return true;
}

bool SceneDocument::setVisibility(const QString& id, bool visible, QString* error) {
    for (const auto& part : parts_) if (part.id == id) {
        if (part.visible == visible) return true;
        checkpoint();
        for (auto& item : parts_) if (item.id == id) { item.visible = visible; break; }
        return true;
    }
    setError(error, "Object not found."); return false;
}

bool SceneDocument::setVisibilityBatch(const QMap<QString,bool>& desired, QString* error) {
    if(desired.isEmpty()) { setError(error,"Choose at least one body visibility change."); return false; }
    // Validate the ENTIRE request before any document checkpoint. Never let a
    // missing ID produce a partially hidden model or a misleading undo entry.
    bool changed=false;
    for(auto it=desired.cbegin();it!=desired.cend();++it){
        const Part* part=find(it.key());
        if(!part){setError(error,QStringLiteral("Body %1 no longer exists.").arg(it.key()));return false;}
        changed=changed || part->visible!=it.value();
    }
    if(!changed)return true;
    checkpoint();
    for(Part& part:parts_){
        auto it=desired.constFind(part.id);
        if(it!=desired.cend())part.visible=it.value();
    }
    return true;
}

bool SceneDocument::isolateBodies(const QStringList& selectedIds, QString* error) {
    QSet<QString> selected;
    for(const QString& id:selectedIds)selected.insert(id);
    if(selected.isEmpty()) {setError(error,"Select at least one body to isolate.");return false;}
    for(const QString& id:selected)if(!find(id)){
        setError(error,QStringLiteral("Selected body %1 no longer exists.").arg(id));return false;
    }
    QMap<QString,bool> desired;
    for(const Part& part:parts_)desired.insert(part.id,selected.contains(part.id));
    return setVisibilityBatch(desired,error);
}

bool SceneDocument::showAllBodies(QString* error) {
    QMap<QString,bool> desired;
    for(const Part& part:parts_)desired.insert(part.id,true);
    if(desired.isEmpty())return true;
    return setVisibilityBatch(desired,error);
}

bool SceneDocument::setColor(const QString& id, const QString& color, QString* error) {
    if (!isHexColor(color)) { setError(error, "Color must be #RRGGBB."); return false; }
    for (const auto& part : parts_) if (part.id == id) {
        if (part.color.compare(color, Qt::CaseInsensitive) == 0) return true;
        checkpoint();
        for (auto& item : parts_) if (item.id == id) { item.color = color.toLower(); break; }
        return true;
    }
    setError(error, "Object not found."); return false;
}

bool SceneDocument::setMaterial(const QString& id,const QString& materialName,double densityKgPerM3,QString* error) {
    return setMaterial(QStringList{id},materialName,densityKgPerM3,error);
}

bool SceneDocument::setMaterial(const QStringList& ids,const QString& materialName,double densityKgPerM3,QString* error) {
    QSet<QString> requested;
    for(const QString& id:ids)requested.insert(id);
    if(requested.isEmpty()){setError(error,"Select at least one body.");return false;}
    for(const QString& id:requested)if(!find(id)){setError(error,"One or more selected bodies no longer exist.");return false;}
    const QString name=materialName.trimmed();
    const bool clearing=name.isEmpty()&&densityKgPerM3==0.0;
    if(!clearing&&(name.isEmpty()||name.size()>80||name.contains('\n')||name.contains('\r')||
                   !std::isfinite(densityKgPerM3)||densityKgPerM3<0.00001||densityKgPerM3>1000000.0)){
        setError(error,"Choose a single-line material name and a finite density from 0.00001 to 1,000,000 kg/m^3, or clear both.");return false;
    }
    bool changed=false;
    for(const QString& id:requested){
        const Part* part=find(id);
        if(part&&(part->materialName!=name||part->materialDensityKgPerM3!=densityKgPerM3)){changed=true;break;}
    }
    if(!changed)return true;
    checkpoint();
    for(Part& item:parts_)if(requested.contains(item.id)){
        item.materialName=name;item.materialDensityKgPerM3=densityKgPerM3;
        item.materialEngineering={}; // A new material must not inherit the old one's properties.
    }
    return true;
}

bool SceneDocument::setMaterialEngineering(const QStringList& ids, const QString& provenance,
                                           double youngsModulusGPa, QString* error) {
    QSet<QString> requested;
    for(const QString& id:ids)requested.insert(id);
    if(requested.isEmpty()){setError(error,"Select at least one body.");return false;}
    const QString source=provenance.trimmed();
    if(source.size()>200 || provenance.contains('\n') || provenance.contains('\r') ||
       !std::isfinite(youngsModulusGPa) || youngsModulusGPa<0.0 || youngsModulusGPa>2000.0){
        setError(error,"Provide single-line provenance (max 200 characters) and Young's modulus from 0 to 2000 GPa (0 = unknown).");
        return false;
    }
    bool changed=false;
    for(const QString& id:requested){
        const Part* part=find(id);
        if(!part || part->materialName.isEmpty() || part->materialDensityKgPerM3<=0.0){
            setError(error,"Engineering data requires an existing assigned material on every body.");return false;
        }
        if(part->materialEngineering.provenance!=source ||
           part->materialEngineering.youngsModulusGPa!=youngsModulusGPa)changed=true;
    }
    if(!changed)return true;
    checkpoint();
    for(Part& part:parts_)if(requested.contains(part.id)){
        part.materialEngineering={source,youngsModulusGPa};
    }
    return true;
}
QVector<MaterialLibraryEntry> SceneDocument::documentMaterialLibrary() const {
    QVector<MaterialLibraryEntry> library;
    for(const Part& part:parts_){
        if(part.materialName.isEmpty())continue;
        const bool present=std::any_of(library.cbegin(),library.cend(),[&part](const MaterialLibraryEntry& entry){
            return entry.name==part.materialName && entry.densityKgPerM3==part.materialDensityKgPerM3 &&
                   entry.engineering.provenance==part.materialEngineering.provenance &&
                   entry.engineering.youngsModulusGPa==part.materialEngineering.youngsModulusGPa;
        });
        if(!present)library.append({part.materialName,part.materialDensityKgPerM3,part.materialEngineering});
    }
    return library;
}


bool SceneDocument::erase(const QString& id, QString* error) { return eraseMany({id}, error); }
bool SceneDocument::eraseMany(const QStringList& ids, QString* error) {
    QSet<QString> requested;
    for (const auto& id : ids) requested.insert(id);
    if (requested.isEmpty()) return true;
    for (const auto& id : requested) if (!find(id)) { setError(error, "One or more selected bodies no longer exist."); return false; }
    for(const Part& part:parts_)if(!part.linkedFoldedFlatSourceId.isEmpty()&&
       !requested.contains(part.id)&&requested.contains(part.linkedFoldedFlatSourceId)){
        setError(error,"A folded body has an associative flat blank. Delete the dependent blank too.");return false;
    }
    for(const Part& part:parts_)if(part.hasLinkedBoolean&&!requested.contains(part.id)&&
       (requested.contains(part.linkedPrimaryId)||requested.contains(part.linkedToolId))){
        setError(error,"A selected body has a linked Boolean dependent. Include that dependent in the delete selection.");return false;
    }
    for(const Part& part:parts_)if(part.hasInPlaceSketchFeature&&!requested.contains(part.id)&&
       (requested.contains(part.inPlaceSketchSourceId)||
        (part.hasInPlaceSketchFollowUp&&requested.contains(part.inPlaceFollowSourceId))||
        std::any_of(part.extraInPlaceSketchStages.begin(),part.extraInPlaceSketchStages.end(),
                    [&](const auto& stage){return requested.contains(stage.sourceId);}))){
        setError(error,"A selected body is the source of an in-place sketch feature. Include the target in the deletion.");return false;
    }
    for(const Part& part:parts_)if(part.hasInterleavedHistory&&!requested.contains(part.id)&&
       std::any_of(part.interleavedSteps.begin(),part.interleavedSteps.end(),
                   [&](const auto& step){return step.kind!=QStringLiteral("Shape")&&
                       requested.contains(step.sketchSourceId);})){
        setError(error,"A selected body drives an interleaved feature history. Include the dependent target.");return false;
    }
    for(const Part& part:parts_)if(part.hasLinkedSketchExtrusion&&!requested.contains(part.id)&&
       requested.contains(part.linkedSketchSourceId)){
        setError(error,"A selected body is the source of a linked sketch extrusion. Include its dependent in the deletion.");return false;
    }
    for(const Part& part:parts_)if(!requested.contains(part.id)&&requested.contains(part.assemblyJointParentBodyId)){
        setError(error,"A selected occurrence is the parent of a joint. Include its jointed descendants in the delete selection.");return false;
    }
    if(!liveRigidMateFixed_.isEmpty()){
        QSet<QString> endpoints{liveRigidMateFixed_};
        for(const auto& link:liveRigidMates_){
            endpoints.insert(link.parentBodyId);endpoints.insert(link.childBodyId);
        }
        bool any=false,all=true;
        for(const QString& id:endpoints){
            any=any||requested.contains(id);all=all&&requested.contains(id);
        }
        if(any&&!all){
            setError(error,"A selected occurrence belongs to a saved live rigid mate network. Remove its mate network first, or delete all its member occurrences.");return false;
        }
    }
    if(!liveArticulatedFixed_.isEmpty()){
        QSet<QString> members{liveArticulatedFixed_};
        for(const auto& joint:liveArticulatedJoints_){
            members.insert(joint.parentBodyId);members.insert(joint.childBodyId);
        }
        bool any=false,all=true;
        for(const auto& id:members){any=any||requested.contains(id);all=all&&requested.contains(id);}
        if(any&&!all){
            setError(error,"A selected occurrence belongs to a saved live articulated mechanism. Remove that network first or delete every member.");return false;
        }
    }
    checkpoint();
    for (int i = parts_.size()-1; i >= 0; --i) if (requested.contains(parts_[i].id)) parts_.removeAt(i);
    if(!liveRigidMateFixed_.isEmpty()&&requested.contains(liveRigidMateFixed_)){
        liveRigidMateFixed_.clear();liveRigidMates_.clear();
    }
    if(!liveArticulatedFixed_.isEmpty()&&requested.contains(liveArticulatedFixed_)){
        liveArticulatedFixed_.clear();liveArticulatedJoints_.clear();
        liveArticulatedReferencePoses_.clear();
    }
    return true;
}

void SceneDocument::clear() {
    if(!parts_.isEmpty()||!liveRigidMateFixed_.isEmpty()||!liveArticulatedFixed_.isEmpty()){
        checkpoint();parts_.clear();liveRigidMateFixed_.clear();liveRigidMates_.clear();
        liveArticulatedFixed_.clear();liveArticulatedJoints_.clear();
        liveArticulatedReferencePoses_.clear();
    }
}
void SceneDocument::newDocument() {
    parts_.clear(); undo_.clear(); redo_.clear(); path_.clear();
    drawingReadinessPolicy_={};
    liveRigidMateFixed_.clear();liveRigidMates_.clear();
    liveArticulatedFixed_.clear();liveArticulatedJoints_.clear();
    liveArticulatedReferencePoses_.clear();
    revision_ = 0; savedRevision_ = 0; nextRevision_ = 1;
}
bool SceneDocument::undo() {
    if (undo_.isEmpty()) return false;
    QVector<Part> snapshotParts = parts_;
    for (auto& part : snapshotParts) part.shape = BRepBuilderAPI_Copy(part.shape).Shape();
    redo_.append({std::move(snapshotParts),drawingReadinessPolicy_,revision_,
                  liveRigidMateFixed_,liveRigidMates_,
                  liveArticulatedFixed_,liveArticulatedJoints_,liveArticulatedReferencePoses_});
    const Snapshot previous = undo_.takeLast();
    parts_ = previous.parts; drawingReadinessPolicy_=previous.drawingReadinessPolicy;
    liveRigidMateFixed_=previous.liveRigidMateFixed;liveRigidMates_=previous.liveRigidMates;
    liveArticulatedFixed_=previous.liveArticulatedFixed;
    liveArticulatedJoints_=previous.liveArticulatedJoints;
    liveArticulatedReferencePoses_=previous.liveArticulatedReferencePoses;
    revision_ = previous.revision;
    return true;
}
bool SceneDocument::redo() {
    if (redo_.isEmpty()) return false;
    QVector<Part> snapshotParts = parts_;
    for (auto& part : snapshotParts) part.shape = BRepBuilderAPI_Copy(part.shape).Shape();
    undo_.append({std::move(snapshotParts),drawingReadinessPolicy_,revision_,
                  liveRigidMateFixed_,liveRigidMates_,
                  liveArticulatedFixed_,liveArticulatedJoints_,liveArticulatedReferencePoses_});
    if (undo_.size() > kHistoryDepth) undo_.removeFirst();
    const Snapshot next = redo_.takeLast();
    parts_ = next.parts; drawingReadinessPolicy_=next.drawingReadinessPolicy;
    liveRigidMateFixed_=next.liveRigidMateFixed;liveRigidMates_=next.liveRigidMates;
    liveArticulatedFixed_=next.liveArticulatedFixed;
    liveArticulatedJoints_=next.liveArticulatedJoints;
    liveArticulatedReferencePoses_=next.liveArticulatedReferencePoses;
    revision_ = next.revision;
    return true;
}

bool SceneDocument::saveAs(const QString& path, QString* error) {
    if (path.trimmed().isEmpty()) { setError(error, "Provide a file path."); return false; }
    try {
        QJsonArray array;
        for (const auto& part : parts_) {
            QJsonObject item;
            item.insert("id",part.id); item.insert("name",part.name);
            item.insert("brep",brepToText(part.shape));
            if(part.isSurfaceBody)item.insert("body_type",QStringLiteral("surface"));
            if(part.hasLinkedBoolean){
                QJsonObject linked;
                linked.insert("primary",part.linkedPrimaryId);
                linked.insert("tool",part.linkedToolId);
                linked.insert("operation",part.linkedOperation);
                linked.insert("primary_token",part.linkedPrimaryToken);
                linked.insert("tool_token",part.linkedToolToken);
                item.insert("linked_boolean",linked);
            }
            if(part.hasLinkedSketchExtrusion){
                QJsonObject link;
                link.insert("source",part.linkedSketchSourceId);
                link.insert("source_token",part.linkedSketchSourceToken);
                link.insert("depth_mm",part.linkedSketchDepthMm);
                link.insert("normal_offset_mm",part.linkedSketchOffsetMm);
                item.insert("linked_sketch_extrusion",link);
            }
            if(part.hasInPlaceSketchFeature){
                QJsonObject link;
                link.insert("source",part.inPlaceSketchSourceId);
                link.insert("source_token",part.inPlaceSketchSourceToken);
                link.insert("operation",part.inPlaceSketchOperation);
                link.insert("depth_mm",part.inPlaceSketchDepthMm);
                link.insert("normal_offset_mm",part.inPlaceSketchOffsetMm);
                link.insert("base_brep",brepToText(part.inPlaceSketchBase));
                if(part.hasInPlaceSketchFollowUp){
                    QJsonObject second;
                    second.insert("source",part.inPlaceFollowSourceId);
                    second.insert("source_token",part.inPlaceFollowSourceToken);
                    second.insert("operation",part.inPlaceFollowOperation);
                    second.insert("depth_mm",part.inPlaceFollowDepthMm);
                    second.insert("normal_offset_mm",part.inPlaceFollowOffsetMm);
                    link.insert("follow_up",second);
                }
                if(!part.extraInPlaceSketchStages.isEmpty()){
                    QJsonArray extra;
                    for(const auto& stage:part.extraInPlaceSketchStages){
                        QJsonObject record;
                        record.insert("source",stage.sourceId);
                        record.insert("source_token",stage.sourceToken);
                        record.insert("operation",stage.operation);
                        record.insert("depth_mm",stage.depthMm);
                        record.insert("normal_offset_mm",stage.offsetMm);
                        extra.append(record);
                    }
                    link.insert("extra_stages",extra);
                }
                // Detect accidental edits of serialized feature parameters before
                // replaying geometry. This is a consistency checksum, not an
                // authenticity signature. Older project files remain readable.
                const QByteArray linkedBytes=QJsonDocument(link).toJson(QJsonDocument::Compact);
                link.insert("integrity_sha256",QString::fromLatin1(
                    QCryptographicHash::hash(linkedBytes,QCryptographicHash::Sha256).toHex()));
                item.insert("in_place_sketch_feature",link);
            }
            if(part.hasInterleavedHistory){
                QJsonObject record;
                const bool branched=std::any_of(part.interleavedSteps.cbegin(),part.interleavedSteps.cend(),
                    [](const InterleavedHistoryStep& step){return !step.inputs.isEmpty();});
                const bool topologyLinks=std::any_of(part.interleavedSteps.cbegin(),part.interleavedSteps.cend(),
                    [](const InterleavedHistoryStep& step){return !step.referenceSourceId.isEmpty();});
                const bool nestedOccurrences=std::any_of(part.interleavedSteps.cbegin(),part.interleavedSteps.cend(),
                    [](const InterleavedHistoryStep& step){return !step.referencePatternOccurrences.isEmpty();});
                const bool patternOccurrences=std::any_of(part.interleavedSteps.cbegin(),part.interleavedSteps.cend(),
                    [](const InterleavedHistoryStep& step){return step.referencePatternOccurrence!=0;});
                record.insert("type",nestedOccurrences?QStringLiteral("dag-interleaved-history-v5"):
                    patternOccurrences?QStringLiteral("dag-interleaved-history-v4"):
                    topologyLinks?QStringLiteral("dag-interleaved-history-v3"):
                    branched?QStringLiteral("dag-interleaved-history-v2"):
                    QStringLiteral("linear-interleaved-history-v1"));
                record.insert("base_brep",brepToText(part.interleavedBase));
                record.insert("geometry_token",part.interleavedGeometryToken);
                QJsonArray entries;
                for(int i=0;i<part.interleavedSteps.size();++i){
                    const auto& step=part.interleavedSteps[i];
                    QJsonObject itemStep;itemStep.insert("id",step.id);itemStep.insert("kind",step.kind);
                    if(!step.inputs.isEmpty()){
                        QJsonArray links;for(const QString& link:step.inputs)links.append(link);
                        itemStep.insert("inputs",links);
                    }
                    if(step.kind==QStringLiteral("Shape")){
                        if(!step.referenceSourceId.isEmpty())
                            itemStep.insert("reference_source",step.referenceSourceId);
                        if(step.referencePatternOccurrence!=0)
                            itemStep.insert("reference_occurrence",step.referencePatternOccurrence);
                        if(!step.referencePatternOccurrences.isEmpty()){
                            QJsonObject occurrences;
                            for(auto it=step.referencePatternOccurrences.cbegin();
                                it!=step.referencePatternOccurrences.cend();++it)
                                occurrences.insert(it.key(),it.value());
                            itemStep.insert("reference_occurrences",occurrences);
                        }
                        itemStep.insert("operation",step.feature.type);
                        itemStep.insert("suppressed",step.feature.suppressed);
                        QJsonObject params;
                        for(auto it=step.feature.parameters.cbegin();it!=step.feature.parameters.cend();++it)
                            params.insert(it.key(),it.value());
                        itemStep.insert("parameters",params);
                    }else{
                        itemStep.insert("source",step.sketchSourceId);
                        itemStep.insert("source_token",part.interleavedSourceTokens.value(i));
                        itemStep.insert("depth_mm",step.depthMm);
                        itemStep.insert("normal_offset_mm",step.offsetMm);
                    }
                    entries.append(itemStep);
                }
                record.insert("steps",entries);item.insert("interleaved_history",record);
            }
            if(part.hasShapeFeatureTree){
                QJsonObject tree;
                tree.insert("type",QStringLiteral("shape-independent-modifiers-v1"));
                tree.insert("base_brep",brepToText(part.shapeFeatureBase));
                tree.insert("geometry_token",part.shapeFeatureGeometryToken);
                QJsonArray nodes;
                for(const ShapeFeatureNode& node:part.shapeFeatureNodes){
                    QJsonObject record;record.insert("id",node.id);
                    record.insert("operation",node.type);record.insert("suppressed",node.suppressed);
                    QJsonObject parameters;
                    for(auto it=node.parameters.cbegin();it!=node.parameters.cend();++it)parameters.insert(it.key(),it.value());
                    record.insert("parameters",parameters);
                    if(!node.inputs.isEmpty()){
                        QJsonArray sources;for(const QString& id:node.inputs)sources.append(id);
                        record.insert("inputs",sources);
                    }
                    nodes.append(record);
                }
                tree.insert("nodes",nodes);
                item.insert("shape_feature_tree",tree);
            }

            item.insert("visible",part.visible); item.insert("color",part.color);
            item.insert("material",part.materialName);
            item.insert("density_kg_per_m3",part.materialDensityKgPerM3);
            if(!part.materialEngineering.provenance.isEmpty() || part.materialEngineering.youngsModulusGPa>0.0){
                QJsonObject engineering;
                engineering.insert("provenance",part.materialEngineering.provenance);
                engineering.insert("youngs_modulus_gpa",part.materialEngineering.youngsModulusGPa);
                item.insert("material_engineering",engineering);
            }
            item.insert("part_number",part.partNumber);
            item.insert("description",part.description);
            item.insert("revision",part.revision);
            item.insert("assembly_path",part.assemblyPath);
            item.insert("procurement_type",part.procurementType);
            item.insert("supplier",part.supplier);
            item.insert("supplier_part_number",part.supplierPartNumber);
            item.insert("geometry_token",part.geometryToken);
            if(!part.linkedFoldedFlatSourceId.isEmpty()){
                QJsonObject flatLink;
                flatLink.insert("source_id",part.linkedFoldedFlatSourceId);
                flatLink.insert("source_token",part.linkedFoldedFlatSourceToken);
                if(part.linkedFoldedFlatKFactors.isEmpty()&&
                   part.linkedFoldedFlatRelief.widthMm==0.0&&
                   part.linkedFoldedFlatRelief.depthMm==0.0){
                    flatLink.insert("type","neutral-flat-blank-v1");
                }else{
                    flatLink.insert("type","developed-flat-blank-v2");
                    QJsonArray factors;
                    for(double value:part.linkedFoldedFlatKFactors)factors.append(value);
                    flatLink.insert("k_factors",factors);
                    flatLink.insert("relief_width_mm",part.linkedFoldedFlatRelief.widthMm);
                    flatLink.insert("relief_depth_mm",part.linkedFoldedFlatRelief.depthMm);
                }
                item.insert("linked_folded_flat_blank",flatLink);
            }
            item.insert("component_definition_id",part.componentDefinitionId);
            item.insert("component_name",part.componentName);
            item.insert("occurrence_id",part.occurrenceId);
            item.insert("placement_x",part.placementX); item.insert("placement_y",part.placementY); item.insert("placement_z",part.placementZ);
            if(!part.componentDefinitionId.isEmpty()&&part.placementYawDegrees!=0.0)
                item.insert("placement_yaw_degrees",part.placementYawDegrees);
            if(!part.componentDefinitionId.isEmpty()&&part.placementPitchDegrees!=0.0)
                item.insert("placement_pitch_degrees",part.placementPitchDegrees);
            if(!part.componentDefinitionId.isEmpty()&&part.placementRollDegrees!=0.0)
                item.insert("placement_roll_degrees",part.placementRollDegrees);
            if(!part.assemblyJointType.isEmpty()){
                QJsonObject joint;joint.insert("type",part.assemblyJointType);joint.insert("parent_body_id",part.assemblyJointParentBodyId);
                joint.insert("origin_x",part.jointOriginX);joint.insert("origin_y",part.jointOriginY);joint.insert("origin_z",part.jointOriginZ);
                joint.insert("axis_x",part.jointAxisX);joint.insert("axis_y",part.jointAxisY);joint.insert("axis_z",part.jointAxisZ);
                joint.insert("angle_degrees",part.jointAngleDegrees);joint.insert("minimum_degrees",part.jointMinimumDegrees);
                joint.insert("maximum_degrees",part.jointMaximumDegrees);
                joint.insert("translation_mm",part.jointTranslationMm);joint.insert("minimum_mm",part.jointMinimumMm);
                joint.insert("maximum_mm",part.jointMaximumMm);
                QJsonArray motionKeys;for(const JointMotionKeyframe& key:part.jointMotionKeyframes){
                    QJsonObject record;record.insert("time_seconds",key.timeSeconds);record.insert("value",key.value);motionKeys.append(record);
                }
                joint.insert("motion_keyframes",motionKeys);item.insert("assembly_joint",joint);
            }else item.insert("assembly_joint",QJsonValue::Null);
            if(part.hasDrawingSettings){
                QJsonObject views,centers;
                for(auto it=part.drawingSettings.viewIds.cbegin();it!=part.drawingSettings.viewIds.cend();++it)views.insert(it.key(),it.value());
                for(auto it=part.drawingSettings.viewCenters.cbegin();it!=part.drawingSettings.viewCenters.cend();++it){
                    QJsonObject center;center.insert("x",it.value().x());center.insert("y",it.value().y());centers.insert(it.key(),center);
                }
                QJsonArray annotations;
                for(const DrawingNoteAnnotation& note:part.drawingSettings.noteAnnotations){
                    QJsonObject annotation;annotation.insert("id",note.id);annotation.insert("text",note.text);
                    annotation.insert("x_mm",note.xMm);annotation.insert("y_mm",note.yMm);annotations.append(annotation);
                }
                QJsonArray dimensions;
                for(const DrawingDimensionAnnotation& dimension:part.drawingSettings.dimensionAnnotations){
                    QJsonObject annotation;annotation.insert("id",dimension.id);annotation.insert("axis",dimension.axis);annotation.insert("offset_mm",dimension.offsetMm);dimensions.append(annotation);
                }
                QJsonArray edgeDimensions;
                for(const DrawingEdgeDimensionAnnotation& dimension:part.drawingSettings.edgeDimensionAnnotations){
                    QJsonObject annotation;annotation.insert("id",dimension.id);annotation.insert("source_body_id",dimension.sourceBodyId);
                    annotation.insert("source_geometry_token",dimension.sourceGeometryToken);annotation.insert("edge_index",dimension.edgeIndex);
                    annotation.insert("view",dimension.view);annotation.insert("offset_mm",dimension.offsetMm);
                    annotation.insert("kind",dimension.kind);
                    annotation.insert("second_edge_index",dimension.secondEdgeIndex);
                    annotation.insert("upper_tolerance_mm",dimension.upperToleranceMm);
                    annotation.insert("lower_tolerance_mm",dimension.lowerToleranceMm);
                    if(!dimension.secondEdgeAnchor.isEmpty()){
                        QJsonObject secondWitness;
                        for(auto it=dimension.secondEdgeAnchor.cbegin();it!=dimension.secondEdgeAnchor.cend();++it)
                            secondWitness.insert(it.key(),it.value());
                        annotation.insert("second_edge_anchor",secondWitness);
                    }
                    if(!dimension.edgeAnchor.isEmpty()){
                        QJsonObject witness;
                        for(auto it=dimension.edgeAnchor.cbegin();it!=dimension.edgeAnchor.cend();++it)
                            witness.insert(it.key(),it.value());
                        annotation.insert("edge_anchor",witness);
                    }
                    edgeDimensions.append(annotation);
                }
                QJsonArray gdtFrames;
                for(const DrawingGdtFrameAnnotation& frame:part.drawingSettings.gdtFrames){
                    QJsonObject annotation;
                    annotation.insert("id",frame.id);annotation.insert("source_body_id",frame.sourceBodyId);
                    annotation.insert("source_geometry_token",frame.sourceGeometryToken);
                    annotation.insert("characteristic",frame.characteristic);
                    annotation.insert("tolerance_mm",frame.toleranceMm);
                    annotation.insert("datum_a",frame.datumA);annotation.insert("datum_b",frame.datumB);
                    annotation.insert("datum_c",frame.datumC);
                    annotation.insert("x_mm",frame.xMm);annotation.insert("y_mm",frame.yMm);
                    gdtFrames.append(annotation);
                }
                QJsonObject partsList;partsList.insert("id",part.drawingSettings.partsListAnnotation.id);
                partsList.insert("visible",part.drawingSettings.partsListAnnotation.visible);
                partsList.insert("x_mm",part.drawingSettings.partsListAnnotation.xMm);
                partsList.insert("y_mm",part.drawingSettings.partsListAnnotation.yMm);
                QJsonArray balloons;
                for(const DrawingBalloonAnnotation& balloon:part.drawingSettings.balloonAnnotations){
                    QJsonObject annotation;annotation.insert("id",balloon.id);annotation.insert("source_body_id",balloon.sourceBodyId);
                    annotation.insert("x_mm",balloon.xMm);annotation.insert("y_mm",balloon.yMm);balloons.append(annotation);
                }
                QJsonArray detailViews;
                for(const DrawingDetailAnnotation& detail:part.drawingSettings.detailAnnotations){
                    QJsonObject annotation;annotation.insert("id",detail.id);annotation.insert("source_view",detail.sourceView);
                    annotation.insert("source_x_mm",detail.sourceXmm);annotation.insert("source_y_mm",detail.sourceYmm);
                    annotation.insert("capture_radius_mm",detail.captureRadiusMm);annotation.insert("x_mm",detail.xMm);
                    annotation.insert("y_mm",detail.yMm);annotation.insert("radius_mm",detail.radiusMm);detailViews.append(annotation);
                }
                QJsonArray sectionViews;
                for(const DrawingSectionAnnotation& section:part.drawingSettings.sectionAnnotations){
                    QJsonObject annotation;annotation.insert("id",section.id);annotation.insert("plane_axis",section.planeAxis);
                    annotation.insert("depth_fraction",section.depthFraction);
                    annotation.insert("x_mm",section.xMm);annotation.insert("y_mm",section.yMm);sectionViews.append(annotation);
                }
                QJsonObject drawing;drawing.insert("sheet_id",part.drawingSettings.sheetId);drawing.insert("source_geometry_token",part.drawingSettings.sourceGeometryToken);
                drawing.insert("source_data_token",part.drawingSettings.sourceDataToken);
                QJsonArray sourceBodies;for(const QString& id:part.drawingSettings.sourceBodyIds)sourceBodies.append(id);drawing.insert("source_body_ids",sourceBodies);
                drawing.insert("view_ids",views);drawing.insert("view_centers",centers);drawing.insert("note_annotations",annotations);
                drawing.insert("dimension_annotations",dimensions);
                drawing.insert("edge_dimension_annotations",edgeDimensions);
                drawing.insert("gdt_frame_annotations",gdtFrames);
                drawing.insert("linked_parts_list",partsList);
                drawing.insert("balloon_annotations",balloons);
                drawing.insert("detail_views",detailViews);
                drawing.insert("section_views",sectionViews);
                drawing.insert("projection",part.drawingSettings.projection);drawing.insert("sheet_size",part.drawingSettings.sheetSize);
                drawing.insert("auxiliary_view",part.drawingSettings.auxiliaryView);drawing.insert("section_depth_fraction",part.drawingSettings.sectionDepthFraction);
                QJsonArray notes;for(const QString& note:part.drawingSettings.generalNotes)notes.append(note);drawing.insert("general_notes",notes);
                drawing.insert("drawing_number",part.drawingSettings.drawingNumber);drawing.insert("drawn_by",part.drawingSettings.drawnBy);
                drawing.insert("checked_by",part.drawingSettings.checkedBy);drawing.insert("issue_date",part.drawingSettings.issueDate);drawing.insert("revision",part.drawingSettings.revision);
                item.insert("drawing_settings",drawing);
            }else item.insert("drawing_settings",QJsonValue::Null);
            if(part.hasConstrainedPolygonDefinition){
                QJsonObject definition;definition.insert("type","constrained-polygon-extrusion-v1");
                definition.insert("sketch",sketchModelToJson(part.constrainedPolygonSketch));
                QJsonArray boundary;for(const QString& id:part.constrainedPolygonBoundary)boundary.append(id);
                definition.insert("boundary",boundary);definition.insert("depth_mm",part.constrainedPolygonDepthMm);
                definition.insert("geometry_token",part.constrainedPolygonGeometryToken);
                definition.insert("plane",sketchPlaneToJson(part.constrainedPolygonPlane));
                item.insert("parametric_definition",definition);
            }else if(part.hasParametricDefinition){
                const bool hasThroughHole=part.featureHistory.contains(QStringLiteral("Cut: Parametric through-hole"));
                const bool hasBlindHole=part.featureHistory.contains(QStringLiteral("Cut: Parametric blind hole"));
                const bool hasHolePattern=part.featureHistory.contains(QStringLiteral("Pattern: Linear through-hole array along X"))||
                    part.featureHistory.contains(QStringLiteral("Pattern: Linear through-hole array along Y"))||
                    part.featureHistory.contains(QStringLiteral("Pattern: Linear blind-hole array along X"))||
                    part.featureHistory.contains(QStringLiteral("Pattern: Linear blind-hole array along Y"));
                const bool offsetProfile=part.namedParameters.contains(QStringLiteral("origin_x"));
                QString type;
                if(offsetProfile)type=hasHolePattern&&hasBlindHole?QStringLiteral("rectangle-extrusion-offset-blind-hole-pattern-v1"):
                    hasHolePattern?QStringLiteral("rectangle-extrusion-offset-through-hole-pattern-v1"):
                    hasThroughHole?QStringLiteral("rectangle-extrusion-offset-through-hole-v1"):
                    hasBlindHole?QStringLiteral("rectangle-extrusion-offset-blind-hole-v1"):QStringLiteral("rectangle-extrusion-offset-v1");
                else type=hasHolePattern&&hasBlindHole?QStringLiteral("rectangle-extrusion-blind-hole-pattern-v1"):
                    hasHolePattern?QStringLiteral("rectangle-extrusion-through-hole-pattern-v1"):
                    hasThroughHole?QStringLiteral("rectangle-extrusion-through-hole-v2"):
                    hasBlindHole?QStringLiteral("rectangle-extrusion-blind-hole-v1"):QStringLiteral("rectangle-extrusion-v1");
                QJsonObject definition;definition.insert("type",type);
                QJsonObject parameters;for(auto it=part.namedParameters.cbegin();it!=part.namedParameters.cend();++it)parameters.insert(it.key(),it.value());
                definition.insert("parameters",parameters);
                if(!part.namedParameterFormulas.isEmpty()){
                    QJsonObject persistedFormulas;
                    for(auto it=part.namedParameterFormulas.cbegin();it!=part.namedParameterFormulas.cend();++it)
                        persistedFormulas.insert(it.key(),it.value());
                    definition.insert("dimension_formulas",persistedFormulas);
                }
                if(!part.parameterLinks.isEmpty()){
                    QJsonObject links;
                    for(auto it=part.parameterLinks.cbegin();it!=part.parameterLinks.cend();++it){
                        QJsonObject rule;rule.insert("source",it->source);rule.insert("scale",it->scale);
                        if(it->offset!=0.0)rule.insert("offset",it->offset);
                        links.insert(it.key(),rule);
                    }
                    definition.insert("parameter_links",links);
                }
                if(hasHolePattern)definition.insert("pattern_axis",part.holePatternAxis);
                QJsonArray constraints;for(const QString& constraint:part.sketchConstraints)constraints.append(constraint);
                definition.insert("sketch_constraints",constraints);
                QJsonArray history;for(const QString& feature:part.featureHistory)history.append(feature);
                definition.insert("feature_history",history);
                if(!part.suppressedParametricFeatures.isEmpty()){
                    QJsonArray suppressed;for(const QString& featureId:part.suppressedParametricFeatures)suppressed.append(featureId);
                    definition.insert("suppressed_features",suppressed);
                }
                definition.insert("geometry_token",part.parametricGeometryToken);
                item.insert("parametric_definition",definition);
            }else item.insert("parametric_definition",QJsonValue::Null);
            if(!part.featureInspectionNotes.isEmpty()){
                QJsonArray notes;
                for(const auto& note:part.featureInspectionNotes){
                    QJsonObject itemNote;
                    itemNote.insert("id",note.id);itemNote.insert("feature_id",note.featureId);
                    itemNote.insert("category",note.category);itemNote.insert("note",note.text);
                    itemNote.insert("reviewed_geometry_token",note.reviewedGeometryToken);
                    notes.append(itemNote);
                }
                item.insert("feature_inspection_notes",notes);
            }
            if(!part.designConfigurations.isEmpty()){
                QJsonArray catalog;
                for(const SavedDesignConfiguration& saved:part.designConfigurations){
                    QJsonObject entry;entry.insert("name",saved.name);
                    QJsonObject expressions;
                    for(auto it=saved.expressions.cbegin();it!=saved.expressions.cend();++it)
                        expressions.insert(it.key(),it.value());
                    entry.insert("expressions",expressions);catalog.append(entry);
                }
                item.insert("design_configurations",catalog);
            }
            if(!part.activeDesignConfiguration.isEmpty())
                item.insert("active_design_configuration",part.activeDesignConfiguration);
            if(part.hasFoldedSheetMetalDefinition){
                QJsonObject definition;definition.insert("type","faceted-folded-sheet-v1");
                definition.insert("geometry_token",part.foldedSheetGeometryToken);
                definition.insert("sheet_width_mm",part.foldedSheetWidthMm);
                definition.insert("thickness_mm",part.foldedSheetThicknessMm);
                QJsonArray flanges;for(double value:part.foldedSheetFlangesMm)flanges.append(value);
                QJsonArray bends;for(const FoldedSheetBend& bend:part.foldedSheetBends){QJsonObject entry;entry.insert("angle_degrees",bend.angleDegrees);entry.insert("inside_radius_mm",bend.insideRadiusMm);bends.append(entry);}
                definition.insert("straight_flanges_mm",flanges);definition.insert("bends",bends);
                item.insert("folded_sheet_definition",definition);
            }else item.insert("folded_sheet_definition",QJsonValue::Null);
            array.append(item);
        }
        QJsonObject root;
        root.insert("schema",kSchema24);
        root.insert("units","mm");
        QJsonObject readinessPolicy;
        QJsonArray requiredFields;for(const QString& field:drawingReadinessPolicy_.requiredTitleBlockFields)requiredFields.append(field);
        QJsonArray requiredConventions;for(const QString& convention:drawingReadinessPolicy_.requiredAnnotationConventions)requiredConventions.append(convention);
        readinessPolicy.insert("required_title_block_fields",requiredFields);
        readinessPolicy.insert("required_annotation_conventions",requiredConventions);
        root.insert("drawing_readiness_policy",readinessPolicy);
        if(!liveRigidMateFixed_.isEmpty()){
            const auto vectorJson=[](mates::Vec3 v){
                QJsonArray xyz;xyz.append(v.x);xyz.append(v.y);xyz.append(v.z);return xyz;
            };
            QJsonObject savedMates;savedMates.insert("version",1);
            savedMates.insert("fixed",liveRigidMateFixed_);
            QJsonArray links;
            for(const AssemblyNetworkMate& link:liveRigidMates_){
                QJsonObject entry;entry.insert("parent",link.parentBodyId);
                entry.insert("child",link.childBodyId);
                QJsonArray observations;
                for(const auto& c:link.constraints){
                    QJsonObject observation;
                    observation.insert("kind",static_cast<int>(c.kind));
                    observation.insert("parent_point",vectorJson(c.parentPoint));
                    observation.insert("child_point",vectorJson(c.childPoint));
                    observation.insert("parent_axis",vectorJson(c.parentDirection));
                    observation.insert("child_axis",vectorJson(c.childDirection));
                    // Omit zero for exact compatibility with pre-offset scenes.
                    if(c.kind==mates::Kind::CoincidentPlane&&c.offsetMm!=0.0)
                        observation.insert("plane_offset_mm",c.offsetMm);
                    observations.append(observation);
                }
                entry.insert("constraints",observations);links.append(entry);
            }
            savedMates.insert("connections",links);
            root.insert("live_rigid_mates",savedMates);
        }
        if(!liveArticulatedFixed_.isEmpty()){
            const auto vectorJson=[](mates::Vec3 v){
                QJsonArray xyz;xyz.append(v.x);xyz.append(v.y);xyz.append(v.z);return xyz;
            };
            const auto frameJson=[&](const mates::Transform& frame){
                QJsonObject data;data.insert("translation",vectorJson(frame.translation));
                QJsonArray rotation;for(double value:frame.rotation)rotation.append(value);
                data.insert("rotation",rotation);return data;
            };
            QJsonObject network;network.insert("version",1);
            network.insert("fixed",liveArticulatedFixed_);
            QJsonArray edges;
            for(const auto& joint:liveArticulatedJoints_){
                QJsonObject item;
                item.insert("parent",joint.parentBodyId);
                item.insert("child",joint.childBodyId);
                item.insert("kind",static_cast<int>(joint.kind));
                item.insert("child_at_zero",frameJson(joint.childAtZero));
                item.insert("axis",vectorJson(joint.axis));
                item.insert("initial",joint.initial);item.insert("min",joint.minimum);
                item.insert("max",joint.maximum);item.insert("driven",joint.driven);
                edges.append(item);
            }
            network.insert("joints",edges);
            QJsonArray frames;
            for(auto it=liveArticulatedReferencePoses_.cbegin();
                it!=liveArticulatedReferencePoses_.cend();++it){
                QJsonObject item;item.insert("id",it.key());
                item.insert("frame",frameJson(it.value()));frames.append(item);
            }
            network.insert("reference_poses",frames);
            root.insert("live_articulated_mates",network);
        }
        root.insert("parts",array);
        const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
        if (bytes.size() > kMaxFileBytes) { setError(error, "Project exceeds native format size limit (96 MiB)."); return false; }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) { setError(error,file.errorString()); return false; }
        if (file.write(bytes) != bytes.size()) { setError(error,file.errorString()); file.cancelWriting(); return false; }
        if (!file.commit()) { setError(error,file.errorString()); return false; }
        path_ = path; savedRevision_ = revision_;
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("Kernel save error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::load(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { setError(error,file.errorString()); return false; }
    if (file.size() > kMaxFileBytes) { setError(error,"Project exceeds size limit (96 MiB)."); return false; }
    const QByteArray bytes = file.read(kMaxFileBytes + 1);
    if (bytes.size() > kMaxFileBytes) { setError(error,"Project exceeds size limit (96 MiB)."); return false; }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(bytes,&parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) { setError(error,"Invalid project JSON."); return false; }
    const QJsonObject root = json.object();
    const QString schema = root.value("schema").toString();
    if ((schema != kSchema1 && schema != kSchema2 && schema != kSchema3 && schema != kSchema4 && schema != kSchema5 && schema != kSchema6 && schema != kSchema7 && schema != kSchema8 && schema != kSchema9 && schema != kSchema10 && schema != kSchema11 && schema != kSchema12 && schema != kSchema13 && schema != kSchema14 && schema != kSchema15 && schema != kSchema16 && schema != kSchema17 && schema != kSchema18 && schema != kSchema19 && schema != kSchema20 && (schema != kSchema21 && schema != kSchema22 && (schema!=kSchema23&&schema!=kSchema24))) || root.value("units").toString() != "mm" || !root.value("parts").isArray()) {
        setError(error,"Unsupported project schema or units."); return false;
    }
    DrawingReadinessPolicy candidateReadinessPolicy;
    if(schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))){
        const QJsonValue rawPolicy=root.value("drawing_readiness_policy");
        if(!rawPolicy.isObject()){setError(error,"Missing drawing project readiness policy.");return false;}
        const QJsonObject policy=rawPolicy.toObject();
        const QJsonValue rawFields=policy.value("required_title_block_fields");
        const QJsonValue rawConventions=policy.value("required_annotation_conventions");
        if(!rawFields.isArray()||!rawConventions.isArray()||rawFields.toArray().size()>8||rawConventions.toArray().size()>4){
            setError(error,"Invalid drawing project readiness policy.");return false;
        }
        for(const QJsonValue& value:rawFields.toArray()){if(!value.isString()){setError(error,"Invalid drawing project title-block policy entry.");return false;}candidateReadinessPolicy.requiredTitleBlockFields.append(value.toString());}
        for(const QJsonValue& value:rawConventions.toArray()){if(!value.isString()){setError(error,"Invalid drawing project annotation policy entry.");return false;}candidateReadinessPolicy.requiredAnnotationConventions.append(value.toString());}
        if(!validDrawingReadinessPolicy(candidateReadinessPolicy,error))return false;
    }
    const QJsonArray array = root.value("parts").toArray();
    if (array.size() > kMaxObjects) { setError(error,"Project exceeds object limit."); return false; }
    QVector<Part> candidate;
    QSet<QString> ids;
    QSet<QString> names;
    QSet<QString> occurrenceIds;
    QHash<QString,QString> componentNames;
    try {
        for (const auto& value : array) {
            if (!value.isObject()) { setError(error,"Invalid part object."); return false; }
            const QJsonObject item = value.toObject();
            const QString id = item.value("id").toString();
            const QString name = item.value("name").toString();
            const QString payload = item.value("brep").toString();
            const QJsonValue rawBodyType=item.value("body_type");
            const QString bodyType=rawBodyType.isUndefined()?QStringLiteral("solid"):rawBodyType.toString();
            if (id.isEmpty() || name.trimmed().isEmpty() || name.size() > 120 || payload.isEmpty() ||
                ids.contains(id) || names.contains(name.toLower()) ||
                (!rawBodyType.isUndefined()&&!rawBodyType.isString()) ||
                (bodyType!=QStringLiteral("solid")&&bodyType!=QStringLiteral("surface"))) {
                setError(error,"Invalid or duplicate body metadata."); return false;
            }
            bool visible = true;
            QString color = "#6c9dc2";
            QString materialName;
            double materialDensityKgPerM3=0.0;
            MaterialEngineeringData materialEngineering;
            QString partNumber, description, revision, assemblyPath;
            QString procurementType = QStringLiteral("Manufactured"), supplier, supplierPartNumber;
            if (schema == kSchema2 || schema == kSchema3 || schema == kSchema4 || schema == kSchema5 || schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))) {
                if (!item.value("visible").isBool() || !item.value("color").isString()) {
                    setError(error,"Invalid v2 appearance fields."); return false;
                }
                visible = item.value("visible").toBool(); color = item.value("color").toString();
                if (!isHexColor(color)) { setError(error,"Invalid body color."); return false; }
                if(item.contains("material")||item.contains("density_kg_per_m3")){
                    if(!item.value("material").isString()||!item.value("density_kg_per_m3").isDouble()){
                        setError(error,"Invalid body material fields.");return false;
                    }
                    materialName=item.value("material").toString().trimmed();
                    materialDensityKgPerM3=item.value("density_kg_per_m3").toDouble();
                    const bool unassigned=materialName.isEmpty()&&materialDensityKgPerM3==0.0;
                    if(!unassigned&&(materialName.isEmpty()||materialName.size()>80||
                                     materialName.contains('\n')||materialName.contains('\r')||
                                     !std::isfinite(materialDensityKgPerM3)||
                                     materialDensityKgPerM3<0.00001||materialDensityKgPerM3>1000000.0)){
                        setError(error,"Invalid body material name or density.");return false;
                    }
                }
                // Additive optional field, absent in earlier scene files.
                if(item.contains("material_engineering")){
                    if(!item.value("material_engineering").isObject() || materialName.isEmpty()){
                        setError(error,"Invalid material engineering record.");return false;
                    }
                    const QJsonObject eng=item.value("material_engineering").toObject();
                    if(!eng.value("provenance").isString() || !eng.value("youngs_modulus_gpa").isDouble()){
                        setError(error,"Invalid material engineering types.");return false;
                    }
                    const QString source=eng.value("provenance").toString();
                    const double modulus=eng.value("youngs_modulus_gpa").toDouble();
                    if(source.trimmed()!=source || source.size()>200 || source.contains('\n') ||
                       source.contains('\r') || !std::isfinite(modulus) || modulus<0.0 || modulus>2000.0){
                        setError(error,"Invalid material engineering values.");return false;
                    }
                    materialEngineering={source,modulus};
                }
                if (schema == kSchema3 || schema == kSchema4 || schema == kSchema5 || schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))) {
                    if (!item.value("part_number").isString() || !item.value("description").isString() || !item.value("revision").isString()) {
                        setError(error,"Invalid part-information fields."); return false;
                    }
                    partNumber=item.value("part_number").toString().trimmed().toUpper();
                    description=item.value("description").toString().trimmed();
                    revision=item.value("revision").toString().trimmed().toUpper();
                    if (partNumber.size()>32 || description.size()>200 || revision.size()>16 ||
                        partNumber.contains('\n') || description.contains('\n') || revision.contains('\n') ||
                        partNumber.contains('\r') || description.contains('\r') || revision.contains('\r')) {
                        setError(error,"Invalid part-information values."); return false;
                    }
                    if (schema == kSchema4 || schema == kSchema5 || schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))) {
                        if (!item.value("assembly_path").isString()) { setError(error,"Invalid assembly path."); return false; }
                        assemblyPath = item.value("assembly_path").toString();
                        QStringList normalizedSegments;
                        for (const QString& segment : assemblyPath.split('/', Qt::SkipEmptyParts)) {
                            const QString clean = segment.trimmed();
                            if (clean.isEmpty() || clean == "." || clean == ".." || clean.size() > 80) {
                                setError(error,"Invalid assembly path segment."); return false;
                            }
                            normalizedSegments.append(clean);
                        }
                        assemblyPath = normalizedSegments.join('/');
                        if (assemblyPath.size() > 200 || assemblyPath.contains('\\') || assemblyPath.contains('\n') || assemblyPath.contains('\r')) {
                            setError(error,"Invalid assembly path value."); return false;
                        }
                    }
                    if (schema == kSchema5 || schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))) {
                        if (!item.value("procurement_type").isString() || !item.value("supplier").isString() ||
                            !item.value("supplier_part_number").isString()) {
                            setError(error,"Invalid procurement fields."); return false;
                        }
                        const QString typeKey = item.value("procurement_type").toString().trimmed().toCaseFolded();
                        if (typeKey != "manufactured" && typeKey != "purchased") { setError(error,"Invalid procurement type."); return false; }
                        procurementType = typeKey == "purchased" ? QStringLiteral("Purchased") : QStringLiteral("Manufactured");
                        supplier = item.value("supplier").toString().trimmed();
                        supplierPartNumber = item.value("supplier_part_number").toString().trimmed();
                        if (supplier.size() > 80 || supplierPartNumber.size() > 64 || supplier.contains('\n') || supplier.contains('\r') ||
                            supplierPartNumber.contains('\n') || supplierPartNumber.contains('\r')) {
                            setError(error,"Invalid supplier fields."); return false;
                        }
                    }
                    if (schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))) {
                        if (!item.value("component_definition_id").isString() || !item.value("component_name").isString() ||
                            !item.value("occurrence_id").isString() || !item.value("placement_x").isDouble() ||
                            !item.value("placement_y").isDouble() || !item.value("placement_z").isDouble()) {
                            setError(error,"Invalid component occurrence fields."); return false;
                        }
                    }
                }
            }
            const TopoDS_Shape shape = brepFromText(payload);
            const bool surfaceBody=bodyType==QStringLiteral("surface");
            if(surfaceBody?!validSurfaceBodyShape(shape,error):!validShape(shape,error))return false;
            candidate.append({id,name,shape,visible,color});
            candidate.last().isSurfaceBody=surfaceBody;
            if(item.contains("linked_boolean")){
                if(schema!=kSchema24||!item.value("linked_boolean").isObject()){
                    setError(error,"Linked Boolean requires the v24 scene schema.");return false;
                }
                const QJsonObject link=item.value("linked_boolean").toObject();
                if(link.size()!=5||!link.value("primary").isString()||
                   !link.value("tool").isString()||!link.value("operation").isString()||
                   !link.value("primary_token").isString()||!link.value("tool_token").isString()){
                    setError(error,"Malformed saved linked Boolean fields.");return false;
                }
                auto& linked=candidate.last();linked.hasLinkedBoolean=true;
                linked.linkedPrimaryId=link.value("primary").toString();
                linked.linkedToolId=link.value("tool").toString();
                linked.linkedOperation=link.value("operation").toString();
                linked.linkedPrimaryToken=link.value("primary_token").toString();
                linked.linkedToolToken=link.value("tool_token").toString();
                if(linked.linkedPrimaryId.isEmpty()||linked.linkedToolId.isEmpty()||
                   linked.linkedPrimaryToken.isEmpty()||linked.linkedToolToken.isEmpty()||
                   linked.linkedPrimaryToken.size()>64||linked.linkedToolToken.size()>64||
                   linked.linkedPrimaryId.size()>64||linked.linkedToolId.size()>64||
                   (linked.linkedOperation!="Fuse"&&linked.linkedOperation!="Cut"&&linked.linkedOperation!="Common")||
                   surfaceBody){
                    setError(error,"Invalid saved linked Boolean references.");return false;
                }
            }
            if(item.contains("linked_sketch_extrusion")){
                if(schema!=kSchema24||!item.value("linked_sketch_extrusion").isObject()||
                   candidate.last().hasLinkedBoolean||surfaceBody){
                    setError(error,"Linked sketch extrusion requires a solid v24 scene record.");return false;
                }
                const QJsonObject link=item.value("linked_sketch_extrusion").toObject();
                if(link.size()!=4||!link.value("source").isString()||
                   !link.value("source_token").isString()||
                   !link.value("depth_mm").isDouble()||
                   !link.value("normal_offset_mm").isDouble()){
                    setError(error,"Malformed saved linked sketch extrusion.");return false;
                }
                Part& derived=candidate.last();
                derived.hasLinkedSketchExtrusion=true;
                derived.linkedSketchSourceId=link.value("source").toString();
                derived.linkedSketchSourceToken=link.value("source_token").toString();
                derived.linkedSketchDepthMm=link.value("depth_mm").toDouble();
                derived.linkedSketchOffsetMm=link.value("normal_offset_mm").toDouble();
                if(derived.linkedSketchSourceId.isEmpty()||derived.linkedSketchSourceId.size()>64||
                   derived.linkedSketchSourceToken.isEmpty()||derived.linkedSketchSourceToken.size()>64||
                   !std::isfinite(derived.linkedSketchDepthMm)||derived.linkedSketchDepthMm<=1e-5||
                   derived.linkedSketchDepthMm>1000000||!std::isfinite(derived.linkedSketchOffsetMm)||
                   std::abs(derived.linkedSketchOffsetMm)>1000000){
                    setError(error,"Invalid saved linked sketch extrusion values.");return false;
                }
            }
            if(item.contains("in_place_sketch_feature")){
                if(schema!=kSchema24||!item.value("in_place_sketch_feature").isObject()||
                   candidate.last().hasLinkedBoolean||candidate.last().hasLinkedSketchExtrusion||
                   surfaceBody){
                    setError(error,"In-place sketch feature requires an ordinary solid v24 body.");return false;
                }
                const QJsonObject link=item.value("in_place_sketch_feature").toObject();
                if(link.contains("integrity_sha256")){
                    if(!link.value("integrity_sha256").isString()){
                        setError(error,"Malformed in-place feature integrity checksum.");return false;
                    }
                    QJsonObject payload=link;
                    payload.remove("integrity_sha256");
                    const QByteArray digest=QCryptographicHash::hash(
                        QJsonDocument(payload).toJson(QJsonDocument::Compact),
                        QCryptographicHash::Sha256).toHex();
                    if(link.value("integrity_sha256").toString()!=QString::fromLatin1(digest)){
                        setError(error,"Saved in-place feature parameters have changed since export.");return false;
                    }
                }
                const int expectedFields=6+int(link.contains("follow_up"))+
                    int(link.contains("extra_stages"))+int(link.contains("integrity_sha256"));
                if(link.size()!=expectedFields||!link.value("source").isString()||
                   !link.value("source_token").isString()||!link.value("operation").isString()||
                   !link.value("depth_mm").isDouble()||!link.value("normal_offset_mm").isDouble()||
                   !link.value("base_brep").isString()){
                    setError(error,"Malformed saved in-place sketch feature.");return false;
                }
                Part& derived=candidate.last();
                derived.inPlaceSketchSourceId=link.value("source").toString();
                derived.inPlaceSketchSourceToken=link.value("source_token").toString();
                derived.inPlaceSketchOperation=link.value("operation").toString();
                derived.inPlaceSketchDepthMm=link.value("depth_mm").toDouble();
                derived.inPlaceSketchOffsetMm=link.value("normal_offset_mm").toDouble();
                derived.inPlaceSketchBase=brepFromText(link.value("base_brep").toString());
                if(derived.inPlaceSketchSourceId.isEmpty()||derived.inPlaceSketchSourceId.size()>64||
                   derived.inPlaceSketchSourceToken.isEmpty()||derived.inPlaceSketchSourceToken.size()>64||
                   (derived.inPlaceSketchOperation!="Fuse"&&derived.inPlaceSketchOperation!="Cut")||
                   !std::isfinite(derived.inPlaceSketchDepthMm)||derived.inPlaceSketchDepthMm<=1e-5||
                   derived.inPlaceSketchDepthMm>1e6||!std::isfinite(derived.inPlaceSketchOffsetMm)||
                   std::abs(derived.inPlaceSketchOffsetMm)>1e6||
                   !validShape(derived.inPlaceSketchBase,error)){
                    setError(error,"Invalid saved in-place sketch references or base geometry.");return false;
                }
                if(link.contains("follow_up")){
                    if(!link.value("follow_up").isObject()){
                        setError(error,"Malformed second in-place modifier.");return false;
                    }
                    const QJsonObject second=link.value("follow_up").toObject();
                    if(!second.value("source").isString()||
                       !second.value("source_token").isString()||
                       !second.value("operation").isString()||
                       !second.value("depth_mm").isDouble()||
                       !second.value("normal_offset_mm").isDouble()){
                        setError(error,"Incomplete second in-place modifier.");return false;
                    }
                    derived.hasInPlaceSketchFollowUp=true;
                    derived.inPlaceFollowSourceId=second.value("source").toString();
                    derived.inPlaceFollowSourceToken=second.value("source_token").toString();
                    derived.inPlaceFollowOperation=second.value("operation").toString();
                    derived.inPlaceFollowDepthMm=second.value("depth_mm").toDouble();
                    derived.inPlaceFollowOffsetMm=second.value("normal_offset_mm").toDouble();
                    if(derived.inPlaceFollowSourceId.isEmpty()||
                       derived.inPlaceFollowSourceId==derived.inPlaceSketchSourceId||
                       derived.inPlaceFollowSourceId==derived.id||
                       derived.inPlaceFollowSourceId.size()>64||
                       derived.inPlaceFollowSourceToken.isEmpty()||
                       derived.inPlaceFollowSourceToken.size()>64||
                       (derived.inPlaceFollowOperation!="Fuse"&&derived.inPlaceFollowOperation!="Cut")||
                       !std::isfinite(derived.inPlaceFollowDepthMm)||
                       derived.inPlaceFollowDepthMm<=1e-5||derived.inPlaceFollowDepthMm>1e6||
                       !std::isfinite(derived.inPlaceFollowOffsetMm)||
                       std::abs(derived.inPlaceFollowOffsetMm)>1e6){
                        setError(error,"Invalid saved second in-place modifier.");return false;
                    }
                }
                if(link.contains("extra_stages")){
                    if(!derived.hasInPlaceSketchFollowUp||!link.value("extra_stages").isArray()){
                        setError(error,"Additional stages require a valid second modifier.");return false;
                    }
                    const QJsonArray extra=link.value("extra_stages").toArray();
                    if(extra.isEmpty()||extra.size()>kMaxInPlaceSketchStages-2){
                        setError(error,"Invalid count of additional sketch modifiers.");return false;
                    }
                    QSet<QString> sources={derived.inPlaceSketchSourceId,derived.inPlaceFollowSourceId,derived.id};
                    for(const QJsonValue& value:extra){
                        if(!value.isObject()){
                            setError(error,"Additional in-place stage must be an object.");return false;
                        }
                        const QJsonObject record=value.toObject();
                        if(record.size()!=5||!record.value("source").isString()||
                           !record.value("source_token").isString()||!record.value("operation").isString()||
                           !record.value("depth_mm").isDouble()||!record.value("normal_offset_mm").isDouble()){
                            setError(error,"Malformed additional in-place modifier.");return false;
                        }
                        Part::AdditionalInPlaceSketchStage stage;
                        stage.sourceId=record.value("source").toString();
                        stage.sourceToken=record.value("source_token").toString();
                        stage.operation=record.value("operation").toString();
                        stage.depthMm=record.value("depth_mm").toDouble();
                        stage.offsetMm=record.value("normal_offset_mm").toDouble();
                        if(stage.sourceId.isEmpty()||stage.sourceId.size()>64||
                           stage.sourceToken.isEmpty()||stage.sourceToken.size()>64||
                           sources.contains(stage.sourceId)||
                           (stage.operation!="Fuse"&&stage.operation!="Cut")||
                           !std::isfinite(stage.depthMm)||stage.depthMm<=1e-5||stage.depthMm>1e6||
                           !std::isfinite(stage.offsetMm)||std::abs(stage.offsetMm)>1e6){
                            setError(error,"Invalid or duplicated additional in-place sketch modifier.");return false;
                        }
                        sources.insert(stage.sourceId);
                        derived.extraInPlaceSketchStages.append(std::move(stage));
                    }
                }
                derived.hasInPlaceSketchFeature=true;
            }
            candidate.last().materialName=materialName;
            // Version-23 feature-stack record; malformed stacks are rejected
            // before the open scene is touched.
            if(item.contains("shape_feature_tree")){
                if((schema!=kSchema23&&schema!=kSchema24)||!item.value("shape_feature_tree").isObject()){
                    setError(error,"Invalid shape feature tree schema.");return false;
                }
                const QJsonObject tree=item.value("shape_feature_tree").toObject();
                const QJsonValue rawNodes=tree.value("nodes");
                if(tree.size()!=4||tree.value("type").toString()!=QStringLiteral("shape-independent-modifiers-v1")||
                   !tree.value("base_brep").isString()||!tree.value("geometry_token").isString()||
                   !rawNodes.isArray()||rawNodes.toArray().size()>40||
                   tree.value("geometry_token").toString().isEmpty()||tree.value("geometry_token").toString().size()>64){
                    setError(error,"Invalid shape feature tree data.");return false;
                }
                QVector<ShapeFeatureNode> nodes;
                for(const QJsonValue& rawNode:rawNodes.toArray()){
                    if(!rawNode.isObject()){setError(error,"Invalid shape feature node.");return false;}
                    const QJsonObject record=rawNode.toObject();
                    if((record.size()!=4&&record.size()!=5)||!record.value("id").isString()||!record.value("operation").isString()||
                       !record.value("suppressed").isBool()||!record.value("parameters").isObject()||
                       (record.contains("inputs")&&(!record.value("inputs").isArray()||(schema!=kSchema24)))){
                        setError(error,"Invalid shape feature node fields.");return false;
                    }
                    ShapeFeatureNode node;node.id=record.value("id").toString();
                    node.type=record.value("operation").toString();
                    node.suppressed=record.value("suppressed").toBool();
                    const QJsonObject params=record.value("parameters").toObject();
                    for(auto it=params.begin();it!=params.end();++it){
                        if(!it.value().isDouble()){setError(error,"Non-numeric feature parameter.");return false;}
                        node.parameters.insert(it.key(),it.value().toDouble());
                    }
                    if(record.contains("inputs"))for(const auto& source:record.value("inputs").toArray()){
                        if(!source.isString()||source.toString().isEmpty()||source.toString().size()>64){
                            setError(error,"Invalid saved feature graph reference.");return false;
                        }
                        node.inputs.append(source.toString());
                    }
                    nodes.append(node);
                }
                if(!validateShapeFeatureNodes(nodes,error))return false;
                const TopoDS_Shape base=brepFromText(tree.value("base_brep").toString());
                TopoDS_Shape rebuilt;
                QVector<TopoDS_Shape> intermediate;
                if(!rebuildShapeFeatureTree(base,surfaceBody,nodes,&rebuilt,error,
                                             nullptr,&intermediate))return false;
                if(tree.value("geometry_token").toString()==item.value("geometry_token").toString()){
                    GProp_GProps expected,stored;
                    // Compare volume and model bounds to reject corrupted or
                    // mismatched geometry only when the saved tree is current.
                    if(!surfaceBody){
                        BRepGProp::VolumeProperties(rebuilt,expected);
                        BRepGProp::VolumeProperties(shape,stored);
                        if(std::abs(expected.Mass()-stored.Mass())>std::max(1e-5,expected.Mass()*1e-5)){
                            setError(error,"Saved feature tree does not match body volume.");return false;
                        }
                    }
                    Bnd_Box a,b;BRepBndLib::AddOptimal(rebuilt,a,false,false);BRepBndLib::AddOptimal(shape,b,false,false);
                    if(a.IsVoid()!=b.IsVoid()){setError(error,"Saved feature tree bounds are invalid.");return false;}
                    if(!a.IsVoid()){
                        double xyzA[6],xyzB[6];a.Get(xyzA[0],xyzA[1],xyzA[2],xyzA[3],xyzA[4],xyzA[5]);
                        b.Get(xyzB[0],xyzB[1],xyzB[2],xyzB[3],xyzB[4],xyzB[5]);
                        for(int coord=0;coord<6;++coord)if(std::abs(xyzA[coord]-xyzB[coord])>1e-4){
                            setError(error,"Saved feature tree does not match body bounds.");return false;
                        }
                    }
                }
                Part& loaded=candidate.last();
                loaded.hasShapeFeatureTree=true;
                loaded.shapeFeatureBase=base;
                loaded.shapeFeatureGeometryToken=tree.value("geometry_token").toString();
                loaded.shapeFeatureNodes=std::move(nodes);
                loaded.shapeFeatureCachedOutputs=std::move(intermediate);
            }

            if(item.contains("interleaved_history")){
                if(schema!=kSchema24||candidate.last().hasShapeFeatureTree||
                   candidate.last().hasInPlaceSketchFeature||
                   candidate.last().hasLinkedBoolean||candidate.last().hasLinkedSketchExtrusion||
                   candidate.last().hasConstrainedPolygonDefinition||
                   candidate.last().hasParametricDefinition||
                   candidate.last().hasFoldedSheetMetalDefinition||surfaceBody||
                   !item.value("interleaved_history").isObject()){
                    setError(error,"Interleaved feature history conflicts with the saved body definition.");return false;
                }
                const QJsonObject data=item.value("interleaved_history").toObject();
                const auto entries=data.value("steps");
                const QString historyType=data.value("type").toString();
                const bool nestedPatternHistory=historyType==QStringLiteral("dag-interleaved-history-v5");
                const bool patternHistory=nestedPatternHistory||historyType==QStringLiteral("dag-interleaved-history-v4");
                const bool topologyHistory=patternHistory||historyType==QStringLiteral("dag-interleaved-history-v3");
                const bool dagHistory=topologyHistory||historyType==QStringLiteral("dag-interleaved-history-v2");
                if(data.size()!=4||(!dagHistory&&historyType!=QStringLiteral("linear-interleaved-history-v1"))||
                   !data.value("base_brep").isString()||!data.value("geometry_token").isString()||
                   data.value("geometry_token").toString().isEmpty()||
                   data.value("geometry_token").toString().size()>64||
                   !entries.isArray()||entries.toArray().isEmpty()||entries.toArray().size()>40){
                    setError(error,"Invalid interleaved history record.");return false;
                }
                auto& saved=candidate.last();
                saved.interleavedBase=brepFromText(data.value("base_brep").toString());
                if(saved.interleavedBase.IsNull()||!validShape(saved.interleavedBase,error))return false;
                saved.interleavedGeometryToken=data.value("geometry_token").toString();
                QSet<QString> seen;
                QSet<QString> earlierPatternIds;
                for(const auto& value:entries.toArray()){
                    if(!value.isObject()){setError(error,"Invalid history step.");return false;}
                    const QJsonObject stepData=value.toObject();
                    InterleavedHistoryStep step;
                    if(!stepData.value("id").isString()||!stepData.value("kind").isString()){
                        setError(error,"Malformed feature history step identity.");return false;
                    }
                    step.id=stepData.value("id").toString();step.kind=stepData.value("kind").toString();
                    if(step.id.isEmpty()||step.id.size()>64||seen.contains(step.id)){
                        setError(error,"Duplicated or malformed history step ID.");return false;
                    }
                    // Validate inputs before adding this step to the valid
                    // set: this rejects cycles and later/unknown references.
                    const bool withInputs=stepData.contains("inputs");
                    if(withInputs){
                        if(!dagHistory||!stepData.value("inputs").isArray()||
                           stepData.value("inputs").toArray().isEmpty()||
                           stepData.value("inputs").toArray().size()>2){
                            setError(error,"Invalid mixed history input list.");return false;
                        }
                        QSet<QString> uniqueInputs;
                        for(const auto& link:stepData.value("inputs").toArray()){
                            if(!link.isString()||!seen.contains(link.toString())&&link.toString()!=QStringLiteral("@base")||
                               uniqueInputs.contains(link.toString())){
                                setError(error,"History references a duplicate or forward dependency.");return false;
                            }
                            uniqueInputs.insert(link.toString());step.inputs.append(link.toString());
                        }
                    }
                    seen.insert(step.id);
                    if(step.kind==QStringLiteral("Shape")){
                        const bool withReference=stepData.contains("reference_source");
                        const bool withOccurrence=stepData.contains("reference_occurrence");
                        const bool withOccurrences=stepData.contains("reference_occurrences");
                        if(stepData.size()!=(withInputs?6:5)+(withReference?1:0)+
                           (withOccurrence?1:0)+(withOccurrences?1:0)||
                           (withOccurrences&&(!nestedPatternHistory||!withReference||withOccurrence||
                             !stepData.value("reference_occurrences").isObject()||
                             stepData.value("reference_occurrences").toObject().isEmpty()||
                             stepData.value("reference_occurrences").toObject().size()>40))||
                           (withOccurrence&&(!patternHistory||!withReference||
                             !stepData.value("reference_occurrence").isDouble()||
                             stepData.value("reference_occurrence").toDouble()<1||
                             stepData.value("reference_occurrence").toDouble()>23||
                             std::floor(stepData.value("reference_occurrence").toDouble())!=
                             stepData.value("reference_occurrence").toDouble()))||
                           (withReference&&(!topologyHistory||!withInputs||
                             !stepData.value("reference_source").isString()||
                             stepData.value("reference_source").toString().isEmpty()||
                             stepData.value("reference_source").toString().size()>64))||
                           !stepData.value("operation").isString()||
                           !stepData.value("suppressed").isBool()||!stepData.value("parameters").isObject()){
                            setError(error,"Malformed general shape modifier.");return false;
                        }
                        step.feature.id=step.id;step.feature.type=stepData.value("operation").toString();
                        step.feature.suppressed=stepData.value("suppressed").toBool();
                        if(withReference)step.referenceSourceId=stepData.value("reference_source").toString();
                        if(withOccurrence)step.referencePatternOccurrence=stepData.value("reference_occurrence").toInt();
                        if(withOccurrences){
                            const auto occurrences=stepData.value("reference_occurrences").toObject();
                            for(auto it=occurrences.begin();it!=occurrences.end();++it){
                                if(!earlierPatternIds.contains(it.key())||!it.value().isDouble()||
                                   it.value().toDouble()<1||it.value().toDouble()>23||
                                   std::floor(it.value().toDouble())!=it.value().toDouble()){
                                    setError(error,"Invalid or forward nested pattern occurrence reference.");return false;
                                }
                                step.referencePatternOccurrences.insert(it.key(),it.value().toInt());
                            }
                        }
                        if(supportsPatternReferenceLineage(step.feature))
                            earlierPatternIds.insert(step.id);
                        const QJsonObject serializedParams=stepData.value("parameters").toObject();
                        for(auto it=serializedParams.begin();it!=serializedParams.end();++it){
                            if(!it.value().isDouble()){setError(error,"Invalid shape modifier number.");return false;}
                            step.feature.parameters.insert(it.key(),it.value().toDouble());
                        }
                        const bool combine=step.feature.type==QStringLiteral("Fuse branches")||
                            step.feature.type==QStringLiteral("Subtract branches")||
                            step.feature.type==QStringLiteral("Intersect branches");
                        if(combine){
                            if(step.inputs.size()!=2||!step.feature.parameters.isEmpty()||
                               step.feature.suppressed||withReference){
                                setError(error,"Invalid two-input Boolean history step.");return false;
                            }
                        }else if(step.inputs.size()>1||!validateShapeFeatureNodes({step.feature},error))return false;
                    }else if(step.kind==QStringLiteral("SketchFuse")||step.kind==QStringLiteral("SketchCut")){
                        if(stepData.size()!=(withInputs?7:6)||step.inputs.size()>1||!stepData.value("source").isString()||
                           !stepData.value("source_token").isString()||
                           !stepData.value("depth_mm").isDouble()||
                           !stepData.value("normal_offset_mm").isDouble()){
                            setError(error,"Malformed linked sketch history step.");return false;
                        }
                        step.sketchSourceId=stepData.value("source").toString();
                        step.depthMm=stepData.value("depth_mm").toDouble();
                        step.offsetMm=stepData.value("normal_offset_mm").toDouble();
                        const QString token=stepData.value("source_token").toString();
                        if(step.sketchSourceId.isEmpty()||step.sketchSourceId.size()>64||
                           token.isEmpty()||token.size()>64||
                           !std::isfinite(step.depthMm)||step.depthMm<=1e-5||step.depthMm>1e6||
                           !std::isfinite(step.offsetMm)||std::abs(step.offsetMm)>1e6){
                            setError(error,"Invalid saved sketch history parameters.");return false;
                        }
                        saved.interleavedSourceTokens.append(token);
                    }else{setError(error,"Unsupported interleaved history operation.");return false;}
                    if(step.kind==QStringLiteral("Shape"))saved.interleavedSourceTokens.append(QString());
                    saved.interleavedSteps.append(std::move(step));
                }
                if(std::none_of(saved.interleavedSteps.cbegin(),saved.interleavedSteps.cend(),
                    [](const InterleavedHistoryStep& step){
                        return step.kind==QStringLiteral("SketchFuse")||
                               step.kind==QStringLiteral("SketchCut");})){
                    setError(error,"Saved interleaved history has no sketch dependency.");return false;
                }
                saved.hasInterleavedHistory=true;
            }

            if((schema==kSchema8||schema==kSchema9||schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24)))))&&item.contains("geometry_token")){
                if(!item.value("geometry_token").isString()||item.value("geometry_token").toString().trimmed().isEmpty()||
                   item.value("geometry_token").toString().size()>64||item.value("geometry_token").toString().contains('\n')||
                   item.value("geometry_token").toString().contains('\r')){
                    setError(error,"Invalid body geometry token.");return false;
                }
                candidate.last().geometryToken=item.value("geometry_token").toString();
            }
            candidate.last().materialDensityKgPerM3=materialDensityKgPerM3;
            candidate.last().materialEngineering=materialEngineering;
            candidate.last().partNumber=partNumber;
            candidate.last().description=description;
            candidate.last().revision=revision;
            candidate.last().assemblyPath=assemblyPath;
            candidate.last().procurementType=procurementType;
            candidate.last().supplier=supplier;
            candidate.last().supplierPartNumber=supplierPartNumber;
            if (schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))) {
                const QJsonObject itemObject=value.toObject();
                Part& loaded=candidate.last();
                loaded.componentDefinitionId=itemObject.value("component_definition_id").toString().trimmed();
                loaded.componentName=itemObject.value("component_name").toString().trimmed();
                loaded.occurrenceId=itemObject.value("occurrence_id").toString().trimmed();
                loaded.placementX=itemObject.value("placement_x").toDouble(); loaded.placementY=itemObject.value("placement_y").toDouble(); loaded.placementZ=itemObject.value("placement_z").toDouble();
                const QJsonValue yawValue=itemObject.value("placement_yaw_degrees");
                const QJsonValue pitchValue=itemObject.value("placement_pitch_degrees");
                const QJsonValue rollValue=itemObject.value("placement_roll_degrees");
                if(!yawValue.isUndefined()){
                    // Older schemas do not define this optional v19 field.
                    if((schema!=kSchema19&&(schema!=kSchema20&&(schema!=kSchema21&&(schema!=kSchema22&&(schema!=kSchema23&&schema!=kSchema24)))))||!yawValue.isDouble()){
                        setError(error,"Invalid saved occurrence yaw record.");return false;
                    }
                    loaded.placementYawDegrees=yawValue.toDouble();
                }
                if(!pitchValue.isUndefined()||!rollValue.isUndefined()){
                    if((schema!=kSchema22&&(schema!=kSchema23&&schema!=kSchema24))||(!pitchValue.isUndefined()&&!pitchValue.isDouble())||
                       (!rollValue.isUndefined()&&!rollValue.isDouble())){
                        setError(error,"Invalid saved occurrence pitch or roll record.");return false;
                    }
                    loaded.placementPitchDegrees=pitchValue.toDouble();
                    loaded.placementRollDegrees=rollValue.toDouble();
                }
                const QJsonValue jointValue=itemObject.value("assembly_joint");
                if(jointValue.isObject()){
                    const QJsonObject joint=jointValue.toObject();
                    const QString type=joint.value("type").toString();
                    const QString parentId=joint.value("parent_body_id").toString().trimmed();
                    const QStringList numericFields{"origin_x","origin_y","origin_z","axis_x","axis_y","axis_z","angle_degrees","minimum_degrees","maximum_degrees"};
                    bool numeric=true;for(const QString& field:numericFields)numeric=numeric&&joint.value(field).isDouble();
                    loaded.assemblyJointType=type;loaded.assemblyJointParentBodyId=parentId;
                    loaded.jointOriginX=joint.value("origin_x").toDouble();loaded.jointOriginY=joint.value("origin_y").toDouble();loaded.jointOriginZ=joint.value("origin_z").toDouble();
                    loaded.jointAxisX=joint.value("axis_x").toDouble();loaded.jointAxisY=joint.value("axis_y").toDouble();loaded.jointAxisZ=joint.value("axis_z").toDouble();
                    loaded.jointAngleDegrees=joint.value("angle_degrees").toDouble();loaded.jointMinimumDegrees=joint.value("minimum_degrees").toDouble();
                    loaded.jointMaximumDegrees=joint.value("maximum_degrees").toDouble();
                    const double axisNorm=std::sqrt(loaded.jointAxisX*loaded.jointAxisX+loaded.jointAxisY*loaded.jointAxisY+loaded.jointAxisZ*loaded.jointAxisZ);
                    const bool prismatic=type=="Prismatic";
                    const bool linearNumeric=!prismatic||(joint.value("translation_mm").isDouble()&&joint.value("minimum_mm").isDouble()&&joint.value("maximum_mm").isDouble());
                    if((type!="Rigid"&&type!="Revolute"&&!prismatic)||parentId.isEmpty()||parentId.size()>64||!numeric||!linearNumeric||
                       !finiteOffset(loaded.jointOriginX)||!finiteOffset(loaded.jointOriginY)||!finiteOffset(loaded.jointOriginZ)||
                       !std::isfinite(axisNorm)||std::abs(axisNorm-1.0)>1e-8||!std::isfinite(loaded.jointAngleDegrees)||
                       !std::isfinite(loaded.jointMinimumDegrees)||!std::isfinite(loaded.jointMaximumDegrees)||
                       loaded.jointMinimumDegrees<-3600||loaded.jointMaximumDegrees>3600||loaded.jointMinimumDegrees>=loaded.jointMaximumDegrees||
                       loaded.jointAngleDegrees<loaded.jointMinimumDegrees||loaded.jointAngleDegrees>loaded.jointMaximumDegrees||
                       (type=="Rigid"&&(loaded.jointAngleDegrees!=0||loaded.jointMinimumDegrees!=-180||loaded.jointMaximumDegrees!=180))){
                        setError(error,"Invalid saved component joint record.");return false;
                    }
                    if(prismatic){
                        loaded.jointTranslationMm=joint.value("translation_mm").toDouble();
                        loaded.jointMinimumMm=joint.value("minimum_mm").toDouble();loaded.jointMaximumMm=joint.value("maximum_mm").toDouble();
                        if(!std::isfinite(loaded.jointTranslationMm)||!std::isfinite(loaded.jointMinimumMm)||!std::isfinite(loaded.jointMaximumMm)||
                           loaded.jointMinimumMm<-1e6||loaded.jointMaximumMm>1e6||loaded.jointMinimumMm>=0.0||loaded.jointMaximumMm<=0.0||
                           loaded.jointMinimumMm>=loaded.jointMaximumMm||loaded.jointTranslationMm<loaded.jointMinimumMm||
                           loaded.jointTranslationMm>loaded.jointMaximumMm||loaded.jointAngleDegrees!=0.0){
                            setError(error,"Invalid saved prismatic joint travel record.");return false;
                        }
                    }
                    const QJsonValue keyValue=joint.value("motion_keyframes");
                    if(!keyValue.isUndefined()&&!keyValue.isArray()){setError(error,"Invalid saved joint motion track.");return false;}
                    if(keyValue.isArray()){
                        const QJsonArray keys=keyValue.toArray();if((!keys.isEmpty()&&keys.size()<2)||keys.size()>200){
                            setError(error,"Invalid saved joint motion track length.");return false;
                        }
                        double lastTime=-1.0;
                        for(const QJsonValue& entry:keys){
                            if(!entry.isObject()){setError(error,"Invalid saved joint motion keyframe.");return false;}
                            const QJsonObject key=entry.toObject();const double time=key.value("time_seconds").toDouble(std::numeric_limits<double>::quiet_NaN());
                            const double target=key.value("value").toDouble(std::numeric_limits<double>::quiet_NaN());
                            const bool inRange=type=="Revolute"?target>=loaded.jointMinimumDegrees&&target<=loaded.jointMaximumDegrees:
                                (prismatic&&target>=loaded.jointMinimumMm&&target<=loaded.jointMaximumMm);
                            if(!std::isfinite(time)||time<0.0||time>3600.0||time<=lastTime||!std::isfinite(target)||!inRange){
                                setError(error,"Invalid saved joint motion keyframe time or target value.");return false;
                            }
                            loaded.jointMotionKeyframes.append({time,target});lastTime=time;
                        }
                    }
                }else if(!jointValue.isNull()&&!jointValue.isUndefined()) {setError(error,"Invalid saved component joint record.");return false;}
                const bool noComponent=loaded.componentDefinitionId.isEmpty()&&loaded.componentName.isEmpty()&&loaded.occurrenceId.isEmpty();
                const bool hasComponent=!loaded.componentDefinitionId.isEmpty()&&!loaded.componentName.isEmpty()&&!loaded.occurrenceId.isEmpty();
                if ((!noComponent&&!hasComponent) || loaded.componentDefinitionId.size()>64 || loaded.componentName.size()>120 || loaded.occurrenceId.size()>64 ||
                    (noComponent&&(loaded.placementX!=0.0||loaded.placementY!=0.0||loaded.placementZ!=0.0||
                                   !yawValue.isUndefined()||!pitchValue.isUndefined()||!rollValue.isUndefined())) ||
                    !std::isfinite(loaded.placementYawDegrees)||std::abs(loaded.placementYawDegrees)>36000.0 ||
                    !std::isfinite(loaded.placementPitchDegrees)||std::abs(loaded.placementPitchDegrees)>36000.0 ||
                    !std::isfinite(loaded.placementRollDegrees)||std::abs(loaded.placementRollDegrees)>36000.0 ||
                    loaded.componentDefinitionId.contains('\n')||loaded.componentDefinitionId.contains('\r') ||
                    loaded.componentName.contains('\n')||loaded.componentName.contains('\r') ||
                    loaded.occurrenceId.contains('\n')||loaded.occurrenceId.contains('\r') ||
                    !std::isfinite(loaded.placementX)||!std::isfinite(loaded.placementY)||!std::isfinite(loaded.placementZ)||
                    std::abs(loaded.placementX)>1e9||std::abs(loaded.placementY)>1e9||std::abs(loaded.placementZ)>1e9) {
                    setError(error,"Invalid component occurrence identity or fixed placement."); return false;
                }
                if (hasComponent) {
                    if (occurrenceIds.contains(loaded.occurrenceId) ||
                        (componentNames.contains(loaded.componentDefinitionId) && componentNames.value(loaded.componentDefinitionId) != loaded.componentName)) {
                        setError(error,"Duplicate occurrence identity or inconsistent component definition name."); return false;
                    }
                    occurrenceIds.insert(loaded.occurrenceId);
                    componentNames.insert(loaded.componentDefinitionId,loaded.componentName);
                }
            }
            if(schema==kSchema7||schema==kSchema8||schema==kSchema9||schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))){
                const QJsonValue raw=value.toObject().value("drawing_settings");
                if(raw.isObject()){
                    const QJsonObject drawing=raw.toObject();
                    const QJsonArray notes=drawing.value("general_notes").toArray();
                    if(!drawing.value("projection").isString()||!drawing.value("sheet_size").isString()||!drawing.value("auxiliary_view").isString()||
                       !drawing.value("section_depth_fraction").isDouble()||!drawing.value("drawing_number").isString()||!drawing.value("drawn_by").isString()||
                       !drawing.value("checked_by").isString()||!drawing.value("issue_date").isString()||!drawing.value("revision").isString()||
                       !drawing.value("general_notes").isArray()||notes.size()>5){setError(error,"Invalid persistent drawing settings.");return false;}
                    DrawingSheetSettings settings;settings.projection=drawing.value("projection").toString();settings.sheetSize=drawing.value("sheet_size").toString();
                    settings.auxiliaryView=drawing.value("auxiliary_view").toString();settings.sectionDepthFraction=drawing.value("section_depth_fraction").toDouble();
                    for(const QJsonValue& note:notes){if(!note.isString()){setError(error,"Invalid persistent drawing note.");return false;}settings.generalNotes.append(note.toString());}
                    settings.drawingNumber=drawing.value("drawing_number").toString();settings.drawnBy=drawing.value("drawn_by").toString();settings.checkedBy=drawing.value("checked_by").toString();
                    settings.issueDate=drawing.value("issue_date").toString();settings.revision=drawing.value("revision").toString();
                    if(schema==kSchema9||schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))){
                        const QJsonObject rawIds=drawing.value("view_ids").toObject();const QJsonObject rawCenters=drawing.value("view_centers").toObject();
                        if(!drawing.value("sheet_id").isString()||!drawing.value("source_geometry_token").isString()||
                           (!drawing.value("source_data_token").isUndefined()&&!drawing.value("source_data_token").isString())||
                           !drawing.value("view_ids").isObject()||rawIds.size()!=4||!drawing.value("view_centers").isObject()||rawCenters.size()>4){
                            setError(error,"Invalid persistent drawing sheet identity or view layout.");return false;
                        }
                        const QJsonArray annotations=drawing.value("note_annotations").toArray();
                        if(!drawing.value("note_annotations").isArray()||annotations.size()>5){setError(error,"Invalid persistent drawing note annotations.");return false;}
                        settings.sheetId=drawing.value("sheet_id").toString();settings.sourceGeometryToken=drawing.value("source_geometry_token").toString();
                        settings.sourceDataToken=drawing.value("source_data_token").toString();
                        const QJsonValue rawSourceBodies=drawing.value("source_body_ids");
                        if(rawSourceBodies.isArray()){
                            const QJsonArray sourceBodies=rawSourceBodies.toArray();
                            if(sourceBodies.size()>50){setError(error,"A drawing sheet can reference at most 50 source bodies.");return false;}
                            for(const QJsonValue& sourceId:sourceBodies){if(!sourceId.isString()){setError(error,"Invalid persistent drawing source body identity.");return false;}settings.sourceBodyIds.append(sourceId.toString());}
                        }else if(!rawSourceBodies.isUndefined()&&!rawSourceBodies.isNull()){
                            setError(error,"Invalid persistent drawing source body list.");return false;
                        }
                        for(auto it=rawIds.begin();it!=rawIds.end();++it){if(!it.value().isString()){setError(error,"Invalid persistent drawing view identity.");return false;}settings.viewIds.insert(it.key(),it.value().toString());}
                        for(auto it=rawCenters.begin();it!=rawCenters.end();++it){
                            if(!it.value().isObject()){setError(error,"Invalid persisted drawing view center.");return false;}
                            const QJsonObject center=it.value().toObject();if(!center.value("x").isDouble()||!center.value("y").isDouble()){setError(error,"Invalid persisted drawing view coordinates.");return false;}
                            settings.viewCenters.insert(it.key(),QPointF(center.value("x").toDouble(),center.value("y").toDouble()));
                        }
                        for(const QJsonValue& annotationValue:annotations){
                            if(!annotationValue.isObject()){setError(error,"Invalid persistent drawing note annotation.");return false;}
                            const QJsonObject annotation=annotationValue.toObject();
                            if(!annotation.value("id").isString()||!annotation.value("text").isString()||
                               !annotation.value("x_mm").isDouble()||!annotation.value("y_mm").isDouble()){
                                setError(error,"Invalid persistent drawing note annotation fields.");return false;
                            }
                            DrawingNoteAnnotation note;note.id=annotation.value("id").toString();note.text=annotation.value("text").toString();
                            note.xMm=annotation.value("x_mm").toDouble();note.yMm=annotation.value("y_mm").toDouble();
                            settings.noteAnnotations.append(note);
                        }
                        if(schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))){
                            const QJsonArray rawDimensions=drawing.value("dimension_annotations").toArray();
                            if(!drawing.value("dimension_annotations").isArray()||rawDimensions.size()!=3){setError(error,"Invalid persistent drawing dimension annotations.");return false;}
                            for(const QJsonValue& dimensionValue:rawDimensions){
                                if(!dimensionValue.isObject()){setError(error,"Invalid persistent drawing dimension annotation.");return false;}
                                const QJsonObject dimension=dimensionValue.toObject();
                                if(!dimension.value("id").isString()||!dimension.value("axis").isString()||!dimension.value("offset_mm").isDouble()){
                                    setError(error,"Invalid persistent drawing dimension annotation fields.");return false;
                                }
                                settings.dimensionAnnotations.append({dimension.value("id").toString(),dimension.value("axis").toString(),dimension.value("offset_mm").toDouble()});
                            }
                        }else{
                            for(const QString& axis:{QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")})
                                settings.dimensionAnnotations.append({QUuid::createUuid().toString(QUuid::WithoutBraces),axis,9.0});
                        }
                        if((schema==kSchema22||(schema==kSchema23||schema==kSchema24))){
                            const QJsonValue rawEdgeDimensions=drawing.value("edge_dimension_annotations");
                            if(!rawEdgeDimensions.isUndefined()&&!rawEdgeDimensions.isArray()){setError(error,"Invalid selected-edge drawing dimensions.");return false;}
                            for(const QJsonValue& dimensionValue:rawEdgeDimensions.toArray()){
                                if(!dimensionValue.isObject()){setError(error,"Invalid selected-edge drawing dimension.");return false;}
                                const QJsonObject dimension=dimensionValue.toObject();
                                if(!dimension.value("id").isString()||!dimension.value("source_body_id").isString()||
                                   !dimension.value("source_geometry_token").isString()||!dimension.value("edge_index").isDouble()||
                                   !dimension.value("view").isString()||!dimension.value("offset_mm").isDouble()){
                                    setError(error,"Invalid selected-edge drawing dimension fields.");return false;
                                }
                                const double edgeIndex=dimension.value("edge_index").toDouble();
                                if(edgeIndex<1||edgeIndex>std::numeric_limits<int>::max()||std::floor(edgeIndex)!=edgeIndex){setError(error,"Invalid selected-edge drawing edge index.");return false;}
                                QMap<QString,double> witness;
                                if(dimension.contains("edge_anchor")){
                                    if(!dimension.value("edge_anchor").isObject()){
                                        setError(error,"Invalid persisted drawing edge witness object.");return false;
                                    }
                                    const QJsonObject fields=dimension.value("edge_anchor").toObject();
                                    for(auto it=fields.begin();it!=fields.end();++it){
                                        if(!it.value().isDouble()){
                                            setError(error,"Drawing edge witness contains a nonnumeric field.");return false;
                                        }
                                        witness.insert(it.key(),it.value().toDouble());
                                    }
                                    if(!validDrawingEdgeWitness(witness)){
                                        setError(error,"Persisted drawing edge witness is invalid.");return false;
                                    }
                                }
                                const QString kind=dimension.value("kind").toString(QStringLiteral("LENGTH"));
                                const QJsonValue secondIndexJson=dimension.value("second_edge_index");
                                const double secondIndex=secondIndexJson.isUndefined()?0.0:secondIndexJson.toDouble(-1);
                                const double upper=dimension.value("upper_tolerance_mm").isUndefined()?0.0:
                                    dimension.value("upper_tolerance_mm").toDouble(-1);
                                const double lower=dimension.value("lower_tolerance_mm").isUndefined()?0.0:
                                    dimension.value("lower_tolerance_mm").toDouble(-1);
                                if((!secondIndexJson.isUndefined()&&!secondIndexJson.isDouble())||
                                   (!dimension.value("upper_tolerance_mm").isUndefined()&&!dimension.value("upper_tolerance_mm").isDouble())||
                                   (!dimension.value("lower_tolerance_mm").isUndefined()&&!dimension.value("lower_tolerance_mm").isDouble())||
                                   secondIndex<0||secondIndex>std::numeric_limits<int>::max()||std::floor(secondIndex)!=secondIndex||
                                   !std::isfinite(upper)||!std::isfinite(lower)||upper<0||lower<0||upper>1000||lower>1000||
                                   !QSet<QString>{"LENGTH","DIAMETER","CENTER_DISTANCE","CENTER_X","CENTER_Y","CENTER_Z"}.contains(kind)){
                                    setError(error,"Invalid drawing manufacturing dimension settings.");return false;
                                }
                                QMap<QString,double> secondWitness;
                                if(dimension.contains("second_edge_anchor")){
                                    if(!dimension.value("second_edge_anchor").isObject()){
                                        setError(error,"Second circular-edge witness must be an object.");return false;
                                    }
                                    const auto values=dimension.value("second_edge_anchor").toObject();
                                    for(auto it=values.begin();it!=values.end();++it){
                                        if(!it.value().isDouble()){
                                            setError(error,"Second circular-edge witness must be numeric.");return false;
                                        }
                                        secondWitness.insert(it.key(),it.value().toDouble());
                                    }
                                    if(!validDrawingEdgeWitness(secondWitness)){
                                        setError(error,"Invalid second circular-edge witness.");return false;
                                    }
                                }
                                DrawingEdgeDimensionAnnotation linked{dimension.value("id").toString(),dimension.value("source_body_id").toString(),
                                    dimension.value("source_geometry_token").toString(),static_cast<int>(edgeIndex),dimension.value("view").toString(),
                                    dimension.value("offset_mm").toDouble(),witness};
                                linked.kind=kind;linked.secondEdgeIndex=static_cast<int>(secondIndex);
                                linked.secondEdgeAnchor=secondWitness;
                                linked.upperToleranceMm=upper;linked.lowerToleranceMm=lower;
                                settings.edgeDimensionAnnotations.append(linked);
                            }
                        }
                        // Optional in v24. Absent fields keep pre-RC4 scenes readable.
                        // Reject malformed optional annotations instead of ignoring them.
                        const QJsonValue rawGdtFrames=drawing.value("gdt_frame_annotations");
                        if(!rawGdtFrames.isUndefined()&&!rawGdtFrames.isArray()){
                            setError(error,"Invalid geometric tolerance annotation list.");return false;
                        }
                        if(rawGdtFrames.toArray().size()>8){setError(error,"Too many geometric tolerance frames.");return false;}
                        for(const QJsonValue& rawFrame:rawGdtFrames.toArray()){
                            if(!rawFrame.isObject()){setError(error,"Malformed geometric tolerance frame.");return false;}
                            const QJsonObject frame=rawFrame.toObject();
                            if(!frame.value("id").isString()||!frame.value("source_body_id").isString()||
                               !frame.value("source_geometry_token").isString()||!frame.value("characteristic").isString()||
                               !frame.value("tolerance_mm").isDouble()||!frame.value("datum_a").isString()||
                               !frame.value("datum_b").isString()||!frame.value("datum_c").isString()||
                               !frame.value("x_mm").isDouble()||!frame.value("y_mm").isDouble()){
                                setError(error,"Invalid geometric tolerance frame field types.");return false;
                            }
                            settings.gdtFrames.append({frame.value("id").toString(),
                                frame.value("source_body_id").toString(),frame.value("source_geometry_token").toString(),
                                frame.value("characteristic").toString(),frame.value("tolerance_mm").toDouble(),
                                frame.value("datum_a").toString(),frame.value("datum_b").toString(),
                                frame.value("datum_c").toString(),frame.value("x_mm").toDouble(),
                                frame.value("y_mm").toDouble()});
                        }
                        if(schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))){
                            const QJsonValue rawPartsList=drawing.value("linked_parts_list");
                            if(!rawPartsList.isObject()){setError(error,"Invalid linked drawing parts-list settings.");return false;}
                            const QJsonObject table=rawPartsList.toObject();
                            if(!table.value("id").isString()||!table.value("visible").isBool()||!table.value("x_mm").isDouble()||!table.value("y_mm").isDouble()){
                                setError(error,"Invalid linked drawing parts-list fields.");return false;
                            }
                            settings.partsListAnnotation={table.value("id").toString(),table.value("visible").toBool(),table.value("x_mm").toDouble(),table.value("y_mm").toDouble()};
                        }
                        if(schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))){
                            const QJsonValue rawBalloons=drawing.value("balloon_annotations");
                            if(!rawBalloons.isArray()||rawBalloons.toArray().size()>50){setError(error,"Invalid persistent drawing balloon list.");return false;}
                            for(const QJsonValue& balloonValue:rawBalloons.toArray()){
                                if(!balloonValue.isObject()){setError(error,"Invalid persistent drawing balloon annotation.");return false;}
                                const QJsonObject balloon=balloonValue.toObject();
                                if(!balloon.value("id").isString()||!balloon.value("source_body_id").isString()||
                                   !balloon.value("x_mm").isDouble()||!balloon.value("y_mm").isDouble()){
                                    setError(error,"Invalid persistent drawing balloon fields.");return false;
                                }
                                settings.balloonAnnotations.append({balloon.value("id").toString(),balloon.value("source_body_id").toString(),
                                    balloon.value("x_mm").toDouble(),balloon.value("y_mm").toDouble()});
                            }
                        }
                        if(schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))){
                            const QJsonValue rawDetails=drawing.value("detail_views");
                            if(!rawDetails.isArray()||rawDetails.toArray().size()>4){setError(error,"Invalid persistent drawing detail-view list.");return false;}
                            for(const QJsonValue& detailValue:rawDetails.toArray()){
                                if(!detailValue.isObject()){setError(error,"Invalid persistent drawing detail view.");return false;}
                                const QJsonObject detail=detailValue.toObject();
                                if(!detail.value("id").isString()||!detail.value("source_view").isString()||
                                   !detail.value("source_x_mm").isDouble()||!detail.value("source_y_mm").isDouble()||
                                   !detail.value("capture_radius_mm").isDouble()||!detail.value("x_mm").isDouble()||
                                   !detail.value("y_mm").isDouble()||!detail.value("radius_mm").isDouble()){
                                    setError(error,"Invalid persistent drawing detail-view fields.");return false;
                                }
                                DrawingDetailAnnotation parsed;parsed.id=detail.value("id").toString();parsed.sourceView=detail.value("source_view").toString();
                                parsed.sourceXmm=detail.value("source_x_mm").toDouble();parsed.sourceYmm=detail.value("source_y_mm").toDouble();
                                parsed.captureRadiusMm=detail.value("capture_radius_mm").toDouble();parsed.xMm=detail.value("x_mm").toDouble();
                                parsed.yMm=detail.value("y_mm").toDouble();parsed.radiusMm=detail.value("radius_mm").toDouble();
                                settings.detailAnnotations.append(parsed);
                            }
                        }
                        if(schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))){
                            const QJsonValue rawSections=drawing.value("section_views");
                            if(!rawSections.isArray()||rawSections.toArray().size()>4){setError(error,"Invalid persistent drawing section-view list.");return false;}
                            for(const QJsonValue& sectionValue:rawSections.toArray()){
                                if(!sectionValue.isObject()){setError(error,"Invalid persistent drawing section view.");return false;}
                                const QJsonObject section=sectionValue.toObject();
                                if(!section.value("id").isString()||!section.value("plane_axis").isString()||!section.value("depth_fraction").isDouble()||
                                   !section.value("x_mm").isDouble()||!section.value("y_mm").isDouble()){
                                    setError(error,"Invalid persistent drawing section-view fields.");return false;
                                }
                                settings.sectionAnnotations.append({section.value("id").toString(),section.value("plane_axis").toString(),section.value("depth_fraction").toDouble(),
                                    section.value("x_mm").toDouble(),section.value("y_mm").toDouble()});
                            }
                        }
                    }else{
                        settings.sheetId=QUuid::createUuid().toString(QUuid::WithoutBraces);settings.sourceGeometryToken=candidate.last().geometryToken;
                        for(const QString& view:{QStringLiteral("TOP"),QStringLiteral("FRONT"),QStringLiteral("RIGHT"),QStringLiteral("AUXILIARY")})settings.viewIds.insert(view,QUuid::createUuid().toString(QUuid::WithoutBraces));
                        const double paperScale=settings.sheetSize=="A3"?420.0/297.0:1.0;
                        const double notesX=(settings.projection=="Third-angle"?12.0:151.0)*paperScale;
                        for(int noteIndex=0;noteIndex<settings.generalNotes.size();++noteIndex)
                            settings.noteAnnotations.append({QUuid::createUuid().toString(QUuid::WithoutBraces),settings.generalNotes[noteIndex],notesX,(180.0+4.0*noteIndex)*paperScale});
                        for(const QString& axis:{QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")})
                            settings.dimensionAnnotations.append({QUuid::createUuid().toString(QUuid::WithoutBraces),axis,9.0});
                    }
                    if(settings.partsListAnnotation.id.isEmpty())settings.partsListAnnotation.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
                    if(!validDrawingSettings(settings,error))return false;candidate.last().drawingSettings=settings;candidate.last().hasDrawingSettings=true;
                }else if(!raw.isNull()){setError(error,"Invalid persistent drawing settings record.");return false;}
            }
            if(schema==kSchema8||schema==kSchema9||schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || (schema == kSchema22 || (schema==kSchema23||schema==kSchema24))))){
                const QJsonValue raw=item.value("parametric_definition");
                if(raw.isObject()){
                    const QJsonObject definition=raw.toObject();
                    if(definition.value("type").toString()==QStringLiteral("constrained-polygon-extrusion-v1")){
                        const QJsonArray boundary=definition.value("boundary").toArray();
                        if((schema!=kSchema21&&(schema!=kSchema22&&(schema!=kSchema23&&schema!=kSchema24)))||(definition.size()!=5&&definition.size()!=6)||
                           (definition.size()==6&&(schema!=kSchema24||!definition.value("plane").isObject()))||
                           !definition.value("sketch").isObject()||
                           !definition.value("boundary").isArray()||boundary.size()<3||boundary.size()>64||
                           !definition.value("depth_mm").isDouble()||!finitePositive(definition.value("depth_mm").toDouble())||
                           !definition.value("geometry_token").isString()||
                           definition.value("geometry_token").toString().isEmpty()||
                           definition.value("geometry_token").toString().size()>64){
                            setError(error,"Invalid saved constrained polygon definition.");return false;
                        }
                        SketchPlane parsedPlane;
                        if(definition.contains("plane")&&
                           !sketchPlaneFromJson(definition.value("plane").toObject(),&parsedPlane,error))return false;
                        SketchModel parsedSketch;
                        if(!sketchModelFromJson(definition.value("sketch").toObject(),&parsedSketch,error))return false;
                        QStringList boundaryIds;
                        for(const QJsonValue& entry:boundary){
                            if(!entry.isString()||entry.toString().isEmpty()||entry.toString().size()>64){
                                setError(error,"Invalid constrained polygon boundary ID.");return false;
                            }
                            boundaryIds.append(entry.toString());
                        }
                        SceneDocument rebuilt;
                        const double depth=definition.value("depth_mm").toDouble();
                        if(!rebuilt.addConstrainedPolygonExtrusion(parsedSketch,boundaryIds,depth,error,parsedPlane))return false;
                        const QString token=definition.value("geometry_token").toString();
                        if(candidate.last().geometryToken==token){
                            GProp_GProps storedMass,rebuiltMass;
                            BRepGProp::VolumeProperties(candidate.last().hasShapeFeatureTree?
                                candidate.last().shapeFeatureBase:candidate.last().shape,storedMass);
                            BRepGProp::VolumeProperties(rebuilt.parts().first().shape,rebuiltMass);
                            Bnd_Box storedBox,rebuiltBox;
                            BRepBndLib::AddOptimal(candidate.last().hasShapeFeatureTree?
                                candidate.last().shapeFeatureBase:candidate.last().shape,storedBox,false,false);
                            BRepBndLib::AddOptimal(rebuilt.parts().first().shape,rebuiltBox,false,false);
                            double a[6],b[6];storedBox.Get(a[0],a[1],a[2],a[3],a[4],a[5]);
                            rebuiltBox.Get(b[0],b[1],b[2],b[3],b[4],b[5]);
                            const double tolerance=std::max(1e-5,rebuiltMass.Mass()*1e-5);
                            if(std::abs(storedMass.Mass()-rebuiltMass.Mass())>tolerance){
                                setError(error,"Saved constrained sketch does not match its solid volume.");return false;
                            }
                            for(int axis=0;axis<6;++axis)if(std::abs(a[axis]-b[axis])>1e-4){
                                setError(error,"Saved constrained sketch does not match its solid bounds.");return false;
                            }
                        }
                        candidate.last().hasConstrainedPolygonDefinition=true;
                        candidate.last().constrainedPolygonSketch=std::move(parsedSketch);
                        candidate.last().constrainedPolygonBoundary=std::move(boundaryIds);
                        candidate.last().constrainedPolygonDepthMm=depth;
                        candidate.last().constrainedPolygonPlane=parsedPlane;
                        candidate.last().constrainedPolygonGeometryToken=token;
                    }else{
                    const QJsonObject parameters=definition.value("parameters").toObject();
                    const QJsonArray constraints=definition.value("sketch_constraints").toArray();
                    const QJsonArray history=definition.value("feature_history").toArray();
                    const QString type=definition.value("type").toString();
                    const bool blindPattern=type==QStringLiteral("rectangle-extrusion-blind-hole-pattern-v1")||
                        type==QStringLiteral("rectangle-extrusion-offset-blind-hole-pattern-v1");
                    const bool throughPattern=type==QStringLiteral("rectangle-extrusion-through-hole-pattern-v1")||
                        type==QStringLiteral("rectangle-extrusion-offset-through-hole-pattern-v1");
                    const bool hasHolePattern=throughPattern||blindPattern;
                    const bool hasChamfer=parameters.contains(QStringLiteral("chamfer_distance"));
                    const bool hasFillet=parameters.contains(QStringLiteral("fillet_radius"));
                    QString patternAxis=QStringLiteral("X");
                    if(hasHolePattern&&(schema==kSchema19||schema==kSchema20||(schema==kSchema21||(schema==kSchema22||(schema==kSchema23||schema==kSchema24))))){
                        if(!definition.value("pattern_axis").isString()||
                           (definition.value("pattern_axis").toString()!=QStringLiteral("X")&&definition.value("pattern_axis").toString()!=QStringLiteral("Y"))){
                            setError(error,"Invalid parametric hole-pattern axis.");return false;
                        }
                        patternAxis=definition.value("pattern_axis").toString();
                    }else if(definition.contains("pattern_axis")){
                        setError(error,"Unexpected parametric hole-pattern axis field.");return false;
                    }
                    const bool hasBlindHole=blindPattern||type==QStringLiteral("rectangle-extrusion-blind-hole-v1")||type==QStringLiteral("rectangle-extrusion-offset-blind-hole-v1");
                    const bool hasThroughHole=throughPattern||type==QStringLiteral("rectangle-extrusion-through-hole-v2")||type==QStringLiteral("rectangle-extrusion-offset-through-hole-v1");
                    const bool hasHole=hasThroughHole||hasBlindHole;
                    const bool offsetProfile=type==QStringLiteral("rectangle-extrusion-offset-v1")||type==QStringLiteral("rectangle-extrusion-offset-through-hole-v1")||type==QStringLiteral("rectangle-extrusion-offset-through-hole-pattern-v1")||type==QStringLiteral("rectangle-extrusion-offset-blind-hole-v1")||type==QStringLiteral("rectangle-extrusion-offset-blind-hole-pattern-v1");
                    const bool supportedType=type==QStringLiteral("rectangle-extrusion-v1")||type==QStringLiteral("rectangle-extrusion-through-hole-v2")||
                        type==QStringLiteral("rectangle-extrusion-through-hole-pattern-v1")||
                        type==QStringLiteral("rectangle-extrusion-offset-v1")||
                        type==QStringLiteral("rectangle-extrusion-offset-through-hole-v1")||
                        type==QStringLiteral("rectangle-extrusion-offset-through-hole-pattern-v1")||
                        ((schema==kSchema21||(schema==kSchema22||(schema==kSchema23||schema==kSchema24)))&&(type==QStringLiteral("rectangle-extrusion-blind-hole-v1")||
                         type==QStringLiteral("rectangle-extrusion-offset-blind-hole-v1")))||
                        ((schema==kSchema22||(schema==kSchema23||schema==kSchema24))&&blindPattern);
                    const int expectedParameterCount=(hasHolePattern?(hasBlindHole?9:8):(hasBlindHole?7:(hasThroughHole?6:3)))+(offsetProfile?2:0)+(hasChamfer?1:0)+(hasFillet?1:0);
                    const int expectedConstraintCount=(hasHolePattern?(hasBlindHole?11:10):(hasBlindHole?9:(hasThroughHole?8:5)))+(offsetProfile?1:0)+(hasChamfer?1:0)+(hasFillet?1:0);
                    if(!supportedType||!definition.value("parameters").isObject()||parameters.size()!=expectedParameterCount||
                       !parameters.value("width").isDouble()||!parameters.value("height").isDouble()||!parameters.value("depth").isDouble()||
                       (hasHole&&(!parameters.value("hole_diameter").isDouble()||!parameters.value("hole_x").isDouble()||!parameters.value("hole_y").isDouble()))||
                       (hasBlindHole&&!parameters.value("hole_depth").isDouble())||
                       (hasHolePattern&&(!parameters.value("hole_pattern_count").isDouble()||!parameters.value("hole_pattern_spacing").isDouble()))||
                       (offsetProfile&&(!parameters.value("origin_x").isDouble()||!parameters.value("origin_y").isDouble()))||
                       !definition.value("sketch_constraints").isArray()||constraints.size()!=expectedConstraintCount||
                       !definition.value("feature_history").isArray()||history.size()!=(hasHolePattern?5:(hasHole?4:2))+(hasChamfer?1:0)+(hasFillet?1:0)||
                       !definition.value("geometry_token").isString()||definition.value("geometry_token").toString().trimmed().isEmpty()||
                       definition.value("geometry_token").toString().size()>64){
                        setError(error,"Invalid parametric sketch or feature-history record.");return false;
                    }
                    QMap<QString,double> parsedParameters;
                    QStringList parameterNames{QStringLiteral("width"),QStringLiteral("height"),QStringLiteral("depth")};
                    if(offsetProfile)parameterNames.append({QStringLiteral("origin_x"),QStringLiteral("origin_y")});
                    if(hasHole)parameterNames.append({QStringLiteral("hole_diameter"),QStringLiteral("hole_x"),QStringLiteral("hole_y")});
                    if(hasBlindHole)parameterNames.append(QStringLiteral("hole_depth"));
                    if(hasHolePattern)parameterNames.append({QStringLiteral("hole_pattern_count"),QStringLiteral("hole_pattern_spacing")});
                    if(hasChamfer)parameterNames.append(QStringLiteral("chamfer_distance"));
                    if(hasFillet)parameterNames.append(QStringLiteral("fillet_radius"));
                    for(const QString& key:parameterNames){
                        const double parameterValue=parameters.value(key).toDouble();
                        if(key==QStringLiteral("hole_pattern_count")?
                           (!std::isfinite(parameterValue)||parameterValue<2||parameterValue>100||std::floor(parameterValue)!=parameterValue):
                           ((key==QStringLiteral("hole_x")||key==QStringLiteral("hole_y")||key==QStringLiteral("origin_x")||key==QStringLiteral("origin_y"))?!finiteOffset(parameterValue):!finitePositive(parameterValue))){
                            setError(error,"Invalid named sketch parameter value.");return false;
                        }
                        parsedParameters.insert(key,parameterValue);
                    }
                    // Schema-v19 introduced dimension links; schema-v20 adds their optional affine offset.
                    // Old files have no links; malformed or cyclic rules fail atomically.
                    QMap<QString,ParameterLink> parsedLinks;
                    if(definition.contains("parameter_links")){
                        if(schema!=kSchema19&&(schema!=kSchema20&&(schema!=kSchema21&&(schema!=kSchema22&&(schema!=kSchema23&&schema!=kSchema24))))){setError(error,"Unexpected parameter dependency graph in older scene schema.");return false;}
                        if(!definition.value("parameter_links").isObject()){
                            setError(error,"Invalid saved parameter dependency graph.");return false;
                        }
                        const QJsonObject jsonLinks=definition.value("parameter_links").toObject();
                        if(jsonLinks.size()>4){setError(error,"Too many saved parameter dependencies.");return false;}
                        for(auto it=jsonLinks.begin();it!=jsonLinks.end();++it){
                            if(!it.value().isObject()){setError(error,"Invalid saved parameter dependency rule.");return false;}
                            const QJsonObject rule=it.value().toObject();
                            if((rule.size()!=2&&(rule.size()!=3||!rule.contains("offset")))||
                               !rule.value("source").isString()||!rule.value("scale").isDouble()||
                               (rule.contains("offset")&&(schema!=kSchema20&&(schema!=kSchema21&&(schema!=kSchema22&&(schema!=kSchema23&&schema!=kSchema24)))))||
                               (rule.contains("offset")&&!rule.value("offset").isDouble())){
                                setError(error,"Invalid saved parameter dependency rule.");return false;
                            }
                            parsedLinks.insert(it.key(),{rule.value("source").toString(),rule.value("scale").toDouble(),
                                rule.value("offset").isUndefined()?0.0:rule.value("offset").toDouble()});
                        }
                        QMap<QString,double> evaluated=parsedParameters;
                        if(!resolveParameterLinks(parsedLinks,evaluated,error))return false;
                        for(auto it=parsedLinks.cbegin();it!=parsedLinks.cend();++it){
                            const double actual=parsedParameters.value(it.key());
                            if(std::abs(evaluated.value(it.key())-actual)>1e-10*std::max(1.0,std::abs(actual))){
                                setError(error,"Saved dependent named parameters disagree with their sources.");return false;
                            }
                        }
                    }
                    QMap<QString,QString> parsedFormulas;
                    if(definition.contains("dimension_formulas")){
                        if(!definition.value("dimension_formulas").isObject()){
                            setError(error,"Invalid saved unit-aware dimension formula map.");return false;
                        }
                        const QJsonObject formulas=definition.value("dimension_formulas").toObject();
                        if(formulas.size()>64){setError(error,"Too many saved dimension formulas.");return false;}
                        for(auto it=formulas.begin();it!=formulas.end();++it){
                            if(!it.value().isString()||parsedLinks.contains(it.key())){
                                setError(error,"A saved dimension formula is not text or targets a driven parameter.");return false;
                            }
                            parsedFormulas.insert(it.key(),it.value().toString());
                        }
                        QMap<QString,double> evaluated;
                        if(!evaluatePersistentDimensionFormulas(parsedParameters,parsedFormulas,&evaluated,error))return false;
                        for(auto it=evaluated.cbegin();it!=evaluated.cend();++it){
                            const double actual=parsedParameters.value(it.key());
                            if(!std::isfinite(it.value())||
                               std::abs(it.value()-actual)>1e-9*std::max(1.0,std::abs(actual))){
                                setError(error,"Persisted dimension formula values disagree with saved geometry parameters.");return false;
                            }
                        }
                    }
                    // Enforce the same supported sketch extents on load as on creation
                    // and parameter edits. Stale histories are still validated: they
                    // remain inspectable, even though they cannot be rebuilt.
                    const double sketchOriginX=parsedParameters.value(QStringLiteral("origin_x"));
                    const double sketchOriginY=parsedParameters.value(QStringLiteral("origin_y"));
                    if(!finiteOffset(sketchOriginX+parsedParameters.value(QStringLiteral("width")))||
                       !finiteOffset(sketchOriginY+parsedParameters.value(QStringLiteral("height")))){
                        setError(error,"Saved parametric sketch rectangle exceeds the supported coordinate range.");return false;
                    }
                    if(hasChamfer&&parsedParameters.value(QStringLiteral("chamfer_distance"))>=0.5*std::min({
                        parsedParameters.value(QStringLiteral("width")),parsedParameters.value(QStringLiteral("height")),
                        parsedParameters.value(QStringLiteral("depth"))})){
                        setError(error,"Saved parametric chamfer exceeds the supported distance for its base extrusion.");return false;
                    }
                    if(hasFillet&&parsedParameters.value(QStringLiteral("fillet_radius"))>=0.5*std::min({
                        parsedParameters.value(QStringLiteral("width")),parsedParameters.value(QStringLiteral("height")),
                        parsedParameters.value(QStringLiteral("depth"))})){
                        setError(error,"Saved parametric fillet exceeds the supported radius for its base extrusion.");return false;
                    }
                    if(hasBlindHole&&parsedParameters.value(QStringLiteral("hole_depth"))>=
                       parsedParameters.value(QStringLiteral("depth"))-std::max(0.01,parsedParameters.value(QStringLiteral("depth"))*1e-5)){
                        setError(error,"Saved parametric blind-hole depth must leave at least 0.01 mm of floor thickness.");return false;
                    }
                    QStringList parsedConstraints,parsedHistory;
                    for(const QJsonValue& entry:constraints){if(!entry.isString()||entry.toString().size()>160){setError(error,"Invalid parametric sketch constraint.");return false;}parsedConstraints.append(entry.toString());}
                    for(const QJsonValue& entry:history){if(!entry.isString()||entry.toString().size()>160){setError(error,"Invalid editable feature-history item.");return false;}parsedHistory.append(entry.toString());}
                    QStringList expectedConstraints=offsetProfile?
                        QStringList{"Dimension: origin X = origin_x parameter","Dimension: origin Y = origin_y parameter","Horizontal: lower rectangle edge","Vertical: left rectangle edge","Dimension: width = width parameter","Dimension: height = height parameter"}:
                        QStringList{"Coincident: rectangle origin at sketch origin","Horizontal: lower rectangle edge","Vertical: left rectangle edge","Dimension: width = width parameter","Dimension: height = height parameter"};
                    QStringList expectedHistory=offsetProfile?
                        QStringList{"Sketch: XY rectangle with editable origin (fully constrained)","Extrude: Base solid (depth parameter)"}:
                        QStringList{"Sketch: XY rectangle (fully constrained)","Extrude: Base solid (depth parameter)"};
                    if(hasHole){
                        expectedConstraints.append({"Coincident: circle center = named hole X/Y parameters","Dimension: circle diameter = hole_diameter parameter","Containment: circle remains inside the rectangular profile"});
                        if(hasBlindHole){
                            expectedConstraints.insert(expectedConstraints.size()-1,"Dimension: blind cut depth = hole_depth parameter");
                            expectedHistory.append({"Sketch: XY circle (fully constrained by center and diameter)","Cut: Parametric blind hole"});
                        }else expectedHistory.append({"Sketch: XY circle (fully constrained by center and diameter)","Cut: Parametric through-hole"});
                    }
                    if(hasHolePattern){
                        expectedConstraints.append(QStringLiteral("Pattern: hole centers spaced by hole_pattern_spacing along ")+patternAxis);
                        expectedConstraints.append("Dimension: hole_pattern_count is an integer from 2 to 100");
                        expectedHistory.append(hasBlindHole?QStringLiteral("Pattern: Linear blind-hole array along ")+patternAxis:
                                                             QStringLiteral("Pattern: Linear through-hole array along ")+patternAxis);
                    }
                    if(hasChamfer){
                        expectedConstraints.append("Dimension: all-edge chamfer = chamfer_distance parameter");
                        expectedHistory.append("Chamfer: All edges");
                    }
                    if(hasFillet){
                        expectedConstraints.append("Dimension: all-edge fillet = fillet_radius parameter");
                        expectedHistory.append("Fillet: All edges");
                    }
                    if(parsedConstraints!=expectedConstraints||parsedHistory!=expectedHistory){
                        setError(error,"Unsupported parametric feature-history operations.");return false;
                    }
                    QStringList parsedSuppressedFeatures;
                    if(definition.contains("suppressed_features")){
                        if((schema!=kSchema19&&(schema!=kSchema20&&(schema!=kSchema21&&(schema!=kSchema22&&(schema!=kSchema23&&schema!=kSchema24)))))||!definition.value("suppressed_features").isArray()){
                            setError(error,"Invalid saved parametric feature suppression list.");return false;
                        }
                        const QJsonArray suppressed=definition.value("suppressed_features").toArray();
                        QSet<QString> uniqueSuppressed;
                        for(const QJsonValue& entry:suppressed){
                            if(!entry.isString()){setError(error,"Invalid saved parametric feature suppression key.");return false;}
                            const QString featureId=entry.toString();
                            const bool known=(featureId==QStringLiteral("through_hole")&&hasThroughHole)||
                                (featureId==QStringLiteral("blind_hole")&&hasBlindHole)||
                                (featureId==QStringLiteral("linear_hole_pattern")&&hasHolePattern)||
                                (featureId==QStringLiteral("edge_chamfer")&&hasChamfer)||
                                (featureId==QStringLiteral("edge_fillet")&&hasFillet);
                            if(!known||uniqueSuppressed.contains(featureId)){
                                setError(error,"Saved parametric feature suppression refers to an unavailable or duplicate operation.");return false;
                            }
                            uniqueSuppressed.insert(featureId);parsedSuppressedFeatures.append(featureId);
                        }
                        parsedSuppressedFeatures.sort();
                    }
                    if(hasHole){
                        const double r=parsedParameters.value("hole_diameter")/2.0,x=parsedParameters.value("hole_x"),y=parsedParameters.value("hole_y");
                        const double originX=parsedParameters.value("origin_x"),originY=parsedParameters.value("origin_y");
                        const int patternCount=hasHolePattern?static_cast<int>(parsedParameters.value("hole_pattern_count")):1;
                        const double spacing=parsedParameters.value("hole_pattern_spacing");
                        const double lastX=x+(patternAxis==QStringLiteral("X")?spacing*(patternCount-1):0.0);
                        const double lastY=y+(patternAxis==QStringLiteral("Y")?spacing*(patternCount-1):0.0);
                        // The stored sketch may contain deliberately suppressed hole
                        // features whose original centers are outside the NEW base
                        // dimensions. Such a feature is not part of the committed
                        // solid and must not invalidate a legitimate final-state
                        // coordinated dimension + suppression edit on reload.
                        // Still validate every ACTIVE hole and ACTIVE pattern against
                        // the saved base and the serialized solid below.
                        const QString holeKey=hasBlindHole?QStringLiteral("blind_hole"):QStringLiteral("through_hole");
                        const bool activeHole=!parsedSuppressedFeatures.contains(holeKey);
                        const bool activePattern=activeHole&&hasHolePattern&&
                            !parsedSuppressedFeatures.contains(QStringLiteral("linear_hole_pattern"));
                        const double furthestX=activePattern?lastX:x;
                        const double furthestY=activePattern?lastY:y;
                        if(activeHole&&(x-r<=originX||y-r<=originY||
                           furthestX+r>=originX+parsedParameters.value("width")||
                           furthestY+r>=originY+parsedParameters.value("height")||
                           (activePattern&&spacing<parsedParameters.value("hole_diameter")))){
                            setError(error,"Saved active parametric hole is outside its rectangular sketch profile.");return false;
                        }
                    }
                    candidate.last().hasParametricDefinition=true;candidate.last().namedParameters=std::move(parsedParameters);
                    candidate.last().parameterLinks=std::move(parsedLinks);
                    candidate.last().namedParameterFormulas=std::move(parsedFormulas);
                    candidate.last().sketchConstraints=std::move(parsedConstraints);candidate.last().featureHistory=std::move(parsedHistory);
                    candidate.last().suppressedParametricFeatures=std::move(parsedSuppressedFeatures);
                    candidate.last().parametricGeometryToken=definition.value("geometry_token").toString();
                    candidate.last().holePatternAxis=patternAxis;
                    if(candidate.last().geometryToken==candidate.last().parametricGeometryToken){
                        Bnd_Box box;BRepBndLib::AddOptimal(candidate.last().shape,box,false,false);
                        double minX=0,minY=0,minZ=0,maxX=0,maxY=0,maxZ=0;box.Get(minX,minY,minZ,maxX,maxY,maxZ);
                        GProp_GProps properties;BRepGProp::VolumeProperties(candidate.last().shape,properties);
                        const QString holeSuppressionId=hasBlindHole?QStringLiteral("blind_hole"):QStringLiteral("through_hole");
                        const bool holeActive=hasHole&&!candidate.last().suppressedParametricFeatures.contains(holeSuppressionId);
                        const bool patternActive=holeActive&&hasHolePattern&&
                            !candidate.last().suppressedParametricFeatures.contains(QStringLiteral("linear_hole_pattern"));
                        const double activeHoleCount=holeActive?(patternActive?
                            candidate.last().namedParameters.value("hole_pattern_count"):1.0):0.0;
                        double expectedVolume=0.0;
                        if(hasChamfer||hasFillet){
                            TopoDS_Shape rebuilt;
                            if(!rebuildParametricSolid(candidate.last(),candidate.last().namedParameters,
                                candidate.last().suppressedParametricFeatures,&rebuilt,error))return false;
                            GProp_GProps rebuiltProperties;BRepGProp::VolumeProperties(rebuilt,rebuiltProperties);
                            expectedVolume=rebuiltProperties.Mass();
                        }else{
                            const double cutDepth=hasBlindHole?candidate.last().namedParameters.value("hole_depth"):
                                candidate.last().namedParameters.value("depth");
                            expectedVolume=candidate.last().namedParameters.value("width")*
                                candidate.last().namedParameters.value("height")*candidate.last().namedParameters.value("depth")-
                                activeHoleCount*std::numbers::pi*std::pow(candidate.last().namedParameters.value("hole_diameter")/2.0,2)*cutDepth;
                        }
                        const double volumeTolerance=std::max(1e-18,std::abs(expectedVolume)*1e-9);
                        // OCCT's exact bounds for blended edges may extend a few
                        // hundredths beyond the nominal prism because of the
                        // tolerances carried by the generated surfaces. Keep the
                        // check scale-aware while still rejecting materially
                        // different saved geometry.
                        const double boundsTolerance=std::max(1e-6,
                            std::max({candidate.last().namedParameters.value("width"),
                                      candidate.last().namedParameters.value("height"),
                                      candidate.last().namedParameters.value("depth")})*5e-4);
                        if(std::abs(minX-candidate.last().namedParameters.value("origin_x"))>boundsTolerance||
                           std::abs(minY-candidate.last().namedParameters.value("origin_y"))>boundsTolerance||std::abs(minZ)>boundsTolerance||
                           std::abs((maxX-minX)-candidate.last().namedParameters.value("width"))>boundsTolerance||
                           std::abs((maxY-minY)-candidate.last().namedParameters.value("height"))>boundsTolerance||
                           std::abs((maxZ-minZ)-candidate.last().namedParameters.value("depth"))>boundsTolerance||
                           !std::isfinite(expectedVolume)||!std::isfinite(properties.Mass())||properties.Mass()<=0.0||
                           std::abs(properties.Mass()-expectedVolume)>volumeTolerance){
                            setError(error,QStringLiteral("Parametric feature history does not match its saved solid geometry (saved volume %1 mm³, rebuilt volume %2 mm³, saved extents %3 × %4 × %5 mm, expected %6 × %7 × %8 mm).")
                                .arg(properties.Mass(),0,'g',12).arg(expectedVolume,0,'g',12)
                                .arg(maxX-minX,0,'g',10).arg(maxY-minY,0,'g',10).arg(maxZ-minZ,0,'g',10)
                                .arg(candidate.last().namedParameters.value("width"),0,'g',10)
                                .arg(candidate.last().namedParameters.value("height"),0,'g',10)
                                .arg(candidate.last().namedParameters.value("depth"),0,'g',10));return false;
                        }
                        // A matching bounding box and volume cannot establish that
                        // each saved through-hole is actually at its named center.
                        // Check the bore interiors before treating this history as editable.
                        if(holeActive){
                            BRepClass3d_SolidClassifier classifier(candidate.last().shape);
                            const double holeX=candidate.last().namedParameters.value("hole_x");
                            const double holeY=candidate.last().namedParameters.value("hole_y");
                            const double spacing=candidate.last().namedParameters.value("hole_pattern_spacing");
                            const int count=patternActive?static_cast<int>(candidate.last().namedParameters.value("hole_pattern_count")):1;
                            const double featureDepth=hasBlindHole?candidate.last().namedParameters.value("hole_depth"):
                                candidate.last().namedParameters.value("depth");
                            const double baseDepth=candidate.last().namedParameters.value("depth");
                            const double cutStartZ=hasBlindHole?baseDepth-featureDepth:0.0;
                            const double depthProbes[]={cutStartZ+featureDepth*0.1,cutStartZ+featureDepth*0.5,cutStartZ+featureDepth*0.9};
                            // The interior probe checks location; a second, off-axis
                            // near-wall probe also checks the claimed bore radius.
                            // Keep both away from tangent edges between pattern bores.
                            const double boreRadius=candidate.last().namedParameters.value("hole_diameter")/2.0;
                            const bool patternAlongY=patternAxis==QStringLiteral("Y");
                            for(int holeIndex=0;holeIndex<count;++holeIndex){
                                const double centerX=holeX+(patternAxis==QStringLiteral("X")?spacing*holeIndex:0.0);
                                const double centerY=holeY+(patternAlongY?spacing*holeIndex:0.0);
                                // Check bore position and radius over its full defined cut depth.
                                for(double sampleZ:depthProbes){
                                    const gp_Pnt probes[]={
                                        gp_Pnt(centerX+0.2*boreRadius,centerY+0.2*boreRadius,sampleZ),
                                        gp_Pnt(centerX+(patternAlongY?0.9:0.1)*boreRadius,
                                               centerY+(patternAlongY?0.1:0.9)*boreRadius,sampleZ)
                                    };
                                    for(const gp_Pnt& probe:probes){
                                        classifier.Perform(probe,1e-7);
                                        if(classifier.State()!=TopAbs_OUT){
                                            setError(error,"Saved through-hole centers, bore radii, or cut depths do not match the solid geometry.");return false;
                                        }
                                    }
                                }
                                if(hasBlindHole){
                                    classifier.Perform(gp_Pnt(centerX,centerY,baseDepth-featureDepth-std::max(1e-5,baseDepth*1e-6)),1e-7);
                                    if(classifier.State()!=TopAbs_IN){
                                        setError(error,"Saved blind-hole floor does not match its named depth.");return false;
                                    }
                                }
                            }
                        }
                    }
                    }
                }else if(!raw.isNull()){setError(error,"Invalid parametric definition record.");return false;}
            }
            // Project-native configurations are optional for all existing v24
            // projects. Older scenes without this field remain readable.
            if(item.contains("design_configurations")||item.contains("active_design_configuration")){
                if(schema!=kSchema24||!item.value("design_configurations").isArray()||
                   item.value("design_configurations").toArray().size()>32||
                   !candidate.last().hasParametricDefinition){
                    setError(error,"Invalid project-native configuration catalog.");return false;
                }
                QVector<SavedDesignConfiguration> parsed;
                for(const QJsonValue& rawEntry:item.value("design_configurations").toArray()){
                    if(!rawEntry.isObject()){
                        setError(error,"Invalid project design configuration entry.");return false;
                    }
                    const QJsonObject entry=rawEntry.toObject();
                    if(entry.size()!=2||!entry.value("name").isString()||
                       !entry.value("expressions").isObject()){
                        setError(error,"Invalid project design configuration fields.");return false;
                    }
                    SavedDesignConfiguration value;value.name=entry.value("name").toString();
                    const QJsonObject expressions=entry.value("expressions").toObject();
                    if(expressions.isEmpty()||expressions.size()>16){
                        setError(error,"Invalid number of configuration dimensions.");return false;
                    }
                    for(auto it=expressions.begin();it!=expressions.end();++it){
                        if(!it.value().isString()){
                            setError(error,"Invalid stored dimension expression.");return false;
                        }
                        value.expressions.insert(it.key(),it.value().toString());
                    }
                    parsed.append(std::move(value));
                }
                if(!validateDesignConfigurations(candidate.last(),parsed,error))return false;
                const QJsonValue active=item.value("active_design_configuration");
                if(!active.isUndefined()){
                    if(!active.isString()){
                        setError(error,"Invalid active design configuration name.");return false;
                    }
                    const QString selected=active.toString();
                    if(selected.isEmpty()||std::none_of(parsed.cbegin(),parsed.cend(),
                       [&](const SavedDesignConfiguration& value){return value.name==selected;})){
                        setError(error,"The active design configuration is missing from its saved catalog.");return false;
                    }
                    candidate.last().activeDesignConfiguration=selected;
                }
                candidate.last().designConfigurations=std::move(parsed);
            }
            const QJsonValue rawFolded=item.value("folded_sheet_definition");
            if(rawFolded.isObject()){
                if(schema!=kSchema19&&(schema!=kSchema20&&(schema!=kSchema21&&(schema!=kSchema22&&(schema!=kSchema23&&schema!=kSchema24))))){setError(error,"Unexpected folded sheet-metal definition in an older scene schema.");return false;}
                const QJsonObject definition=rawFolded.toObject();
                const QJsonValue rawFlanges=definition.value("straight_flanges_mm"),rawBends=definition.value("bends");
                if(definition.size()!=6||definition.value("type").toString()!=QStringLiteral("faceted-folded-sheet-v1")||
                   !definition.value("geometry_token").isString()||definition.value("geometry_token").toString().trimmed().isEmpty()||
                   definition.value("geometry_token").toString().size()>64||!definition.value("sheet_width_mm").isDouble()||
                   !definition.value("thickness_mm").isDouble()||!rawFlanges.isArray()||!rawBends.isArray()||
                   rawBends.toArray().isEmpty()||rawBends.toArray().size()>50||rawFlanges.toArray().size()!=rawBends.toArray().size()+1){
                    setError(error,"Invalid saved folded sheet-metal definition.");return false;
                }
                QVector<double> flanges;for(const QJsonValue& entry:rawFlanges.toArray()){
                    if(!entry.isDouble()||!finitePositive(entry.toDouble())){setError(error,"Invalid saved folded sheet flange length.");return false;}
                    flanges.append(entry.toDouble());
                }
                QVector<FoldedSheetBend> bends;for(const QJsonValue& entry:rawBends.toArray()){
                    if(!entry.isObject()||entry.toObject().size()!=2||!entry.toObject().value("angle_degrees").isDouble()||
                       !entry.toObject().value("inside_radius_mm").isDouble()){
                        setError(error,"Invalid saved folded sheet bend.");return false;
                    }
                    bends.append({entry.toObject().value("angle_degrees").toDouble(),entry.toObject().value("inside_radius_mm").toDouble()});
                }
                const double width=definition.value("sheet_width_mm").toDouble(),thickness=definition.value("thickness_mm").toDouble();
                TopoDS_Shape expected=makeFoldedSheetSolid(flanges,width,thickness,bends,error);
                if(expected.IsNull()||!validShape(expected,error))return false;
                Part& loaded=candidate.last();
                const QString definitionToken=definition.value("geometry_token").toString();
                if(loaded.geometryToken==definitionToken){
                    BRepAlgoAPI_Cut firstDifference(loaded.shape,expected);firstDifference.Build();
                    BRepAlgoAPI_Cut secondDifference(expected,loaded.shape);secondDifference.Build();
                    if(!firstDifference.IsDone()||!secondDifference.IsDone()){
                        setError(error,"Could not validate folded sheet-metal geometry against its editable definition.");return false;
                    }
                    GProp_GProps firstProps,secondProps,expectedProps;BRepGProp::VolumeProperties(firstDifference.Shape(),firstProps);
                    BRepGProp::VolumeProperties(secondDifference.Shape(),secondProps);BRepGProp::VolumeProperties(expected,expectedProps);
                    const double tolerance=std::max(1e-9,std::abs(expectedProps.Mass())*1e-9);
                    if(!std::isfinite(firstProps.Mass())||!std::isfinite(secondProps.Mass())||
                       firstProps.Mass()>tolerance||secondProps.Mass()>tolerance){
                        setError(error,"Folded sheet-metal definition does not match its saved solid geometry.");return false;
                    }
                }
                loaded.hasFoldedSheetMetalDefinition=true;loaded.foldedSheetGeometryToken=definitionToken;
                loaded.foldedSheetFlangesMm=std::move(flanges);loaded.foldedSheetWidthMm=width;
                loaded.foldedSheetThicknessMm=thickness;loaded.foldedSheetBends=std::move(bends);
            }else if(!rawFolded.isNull()&&!rawFolded.isUndefined()){
                setError(error,"Invalid saved folded sheet-metal definition record.");return false;
            }
            if(item.contains("linked_folded_flat_blank")){
                const QJsonValue rawLink=item.value("linked_folded_flat_blank");
                if(schema!=kSchema24||!rawLink.isObject()){
                    setError(error,"Invalid linked folded flat-blank record or scene version.");return false;
                }
                const QJsonObject link=rawLink.toObject();
                const bool legacy=link.value("type").toString()==QStringLiteral("neutral-flat-blank-v1");
                const bool developed=link.value("type").toString()==QStringLiteral("developed-flat-blank-v2");
                if((!legacy&&!developed)||link.size()!=(legacy?3:6)||
                   !link.value("source_id").isString()||!link.value("source_token").isString()||
                   link.value("source_id").toString().isEmpty()||
                   link.value("source_token").toString().isEmpty()||
                   link.value("source_id").toString().size()>64||
                   link.value("source_token").toString().size()>64||
                   candidate.last().hasFoldedSheetMetalDefinition){
                    setError(error,"Malformed associative folded blank link.");return false;
                }
                Part& loaded=candidate.last();
                loaded.linkedFoldedFlatSourceId=link.value("source_id").toString();
                loaded.linkedFoldedFlatSourceToken=link.value("source_token").toString();
                if(developed){
                    if(!link.value("k_factors").isArray()||
                       !link.value("relief_width_mm").isDouble()||
                       !link.value("relief_depth_mm").isDouble()||
                       link.value("k_factors").toArray().size()>50){
                        setError(error,"Malformed saved flat blank bend development.");return false;
                    }
                    for(const QJsonValue& factor:link.value("k_factors").toArray()){
                        if(!factor.isDouble()||!std::isfinite(factor.toDouble())||
                           factor.toDouble()<=0.0||factor.toDouble()>1.0){
                            setError(error,"Invalid saved flat blank K-factor.");return false;
                        }
                        loaded.linkedFoldedFlatKFactors.append(factor.toDouble());
                    }
                    loaded.linkedFoldedFlatRelief={link.value("relief_width_mm").toDouble(),
                                                  link.value("relief_depth_mm").toDouble()};
                    if(!std::isfinite(loaded.linkedFoldedFlatRelief.widthMm)||
                       !std::isfinite(loaded.linkedFoldedFlatRelief.depthMm)){
                        setError(error,"Invalid saved flat blank relief values.");return false;
                    }
                }
            }
            // Parse inspection notes after feature trees so source identities
            // can be checked on first edit. Orphans remain loadable, but the
            // report marks them MISSING_FEATURE rather than CURRENT.
            if(item.contains("feature_inspection_notes")){
                if(schema!=kSchema24||!item.value("feature_inspection_notes").isArray()||
                   item.value("feature_inspection_notes").toArray().size()>100){
                    setError(error,"Malformed saved feature inspection notes.");return false;
                }
                QVector<FeatureInspectionNote> notes;
                for(const auto& raw:item.value("feature_inspection_notes").toArray()){
                    if(!raw.isObject()){
                        setError(error,"Feature inspection note must be an object.");return false;
                    }
                    const QJsonObject entry=raw.toObject();
                    if(entry.size()!=5||!entry.value("id").isString()||
                       !entry.value("feature_id").isString()||
                       !entry.value("category").isString()||
                       !entry.value("note").isString()||
                       !entry.value("reviewed_geometry_token").isString()){
                        setError(error,"Invalid feature inspection note fields.");return false;
                    }
                    notes.append({entry.value("id").toString(),entry.value("feature_id").toString(),
                                  entry.value("category").toString(),entry.value("note").toString(),
                                  entry.value("reviewed_geometry_token").toString()});
                }
                if(!validFeatureNotes(notes,error))return false;
                candidate.last().featureInspectionNotes=std::move(notes);
            }
            ids.insert(id); names.insert(name.toLower());
        }
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("Invalid B-rep: %1").arg(e.what())); return false; }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); return false; }
    // A saved document may contain retargeted links to later independent
    // bodies. Validate every dependency by stable ID, then verify committed
    // geometry in graph order rather than requiring creation order.
    QVector<int> savedOrder;
    if(!planLinkedBodyOrder(candidate,&savedOrder,error))return false;
    QMap<QString,int> savedIndices;
    for(int i=0;i<candidate.size();++i)savedIndices.insert(candidate[i].id,i);
    for(int i:savedOrder){
        const Part& child=candidate[i];
        if(!child.linkedFoldedFlatSourceId.isEmpty()){
            const int sourceIndex=savedIndices.value(child.linkedFoldedFlatSourceId,-1);
            if(sourceIndex<0||sourceIndex==i){
                setError(error,"Saved linked flat blank source does not exist.");return false;
            }
            const Part& source=candidate[sourceIndex];
            if(!source.hasFoldedSheetMetalDefinition||
               source.geometryToken!=source.foldedSheetGeometryToken||
               child.linkedFoldedFlatSourceToken!=source.geometryToken){
                setError(error,"Saved linked flat blank has a stale or invalid folded source.");return false;
            }
            TopoDS_Shape expected;
            if(!createLinkedFlatBlank(source,child.linkedFoldedFlatKFactors,child.linkedFoldedFlatRelief,&expected,error))return false;
            BRepAlgoAPI_Cut a(child.shape,expected);a.Build();
            BRepAlgoAPI_Cut b(expected,child.shape);b.Build();
            if(!a.IsDone()||!b.IsDone()){
                setError(error,"Saved flat blank geometry could not be verified.");return false;
            }
            GProp_GProps pa,pb,pe;
            BRepGProp::VolumeProperties(a.Shape(),pa);
            BRepGProp::VolumeProperties(b.Shape(),pb);
            BRepGProp::VolumeProperties(expected,pe);
            const double threshold=std::max(1e-7,pe.Mass()*1e-8);
            if(!std::isfinite(pa.Mass())||!std::isfinite(pb.Mass())||
               pa.Mass()>threshold||pb.Mass()>threshold){
                setError(error,"Saved flat blank no longer matches folded source development.");return false;
            }
        }
        if(child.hasInterleavedHistory){
            if(child.interleavedGeometryToken!=child.geometryToken){
                setError(error,"Saved interleaved history is stale.");return false;
            }
            TopoDS_Shape rebuilt;QStringList sourceTokens;
            if(!replayInterleavedHistory(child,candidate,savedIndices,&rebuilt,&sourceTokens,error)||
               sourceTokens!=child.interleavedSourceTokens)return false;
            BRepAlgoAPI_Cut forward(rebuilt,child.shape);forward.Build();
            BRepAlgoAPI_Cut reverse(child.shape,rebuilt);reverse.Build();
            if(!forward.IsDone()||!reverse.IsDone()){
                setError(error,"Unable to verify saved interleaved geometry.");return false;
            }
            GProp_GProps a,b,c;BRepGProp::VolumeProperties(forward.Shape(),a);
            BRepGProp::VolumeProperties(reverse.Shape(),b);
            BRepGProp::VolumeProperties(rebuilt,c);
            const double tolerance=std::max(1e-7,std::abs(c.Mass())*1e-7);
            if(!std::isfinite(a.Mass())||!std::isfinite(b.Mass())||
               a.Mass()>tolerance||b.Mass()>tolerance){
                setError(error,"Saved interleaved steps do not reproduce the committed body.");return false;
            }
        }
        if(child.hasInPlaceSketchFeature){
            const int sourceIndex=savedIndices.value(child.inPlaceSketchSourceId,-1);
            if(sourceIndex<0||sourceIndex==i){setError(error,"Saved in-place sketch source is missing.");return false;}
            const Part& source=candidate[sourceIndex];
            if(child.inPlaceSketchSourceToken!=source.geometryToken||
               source.constrainedPolygonGeometryToken!=source.geometryToken){
                setError(error,"Saved in-place sketch source token is stale.");return false;
            }
            TopoDS_Shape tool,expected;
            if(!rebuildLinkedSketchSolid(source,child.inPlaceSketchDepthMm,child.inPlaceSketchOffsetMm,&tool,error)||
               !linkedBooleanResult(child.inPlaceSketchBase,tool,child.inPlaceSketchOperation,&expected,error))return false;
            if(child.inPlaceSketchOperation=="Cut"&&!linkedSketchCutEffective(child.inPlaceSketchBase,expected,error))return false;
            if(child.inPlaceSketchOperation=="Fuse"&&!linkedSketchFuseEffective(child.inPlaceSketchBase,expected,error))return false;
            if(child.hasInPlaceSketchFollowUp){
                const int secondIndex=savedIndices.value(child.inPlaceFollowSourceId,-1);
                if(secondIndex<0||secondIndex==i||secondIndex==sourceIndex){
                    setError(error,"Saved second sketch source is missing or duplicated.");return false;
                }
                const Part& second=candidate[secondIndex];
                if(!second.hasConstrainedPolygonDefinition||
                   second.constrainedPolygonGeometryToken!=second.geometryToken||
                   child.inPlaceFollowSourceToken!=second.geometryToken){
                    setError(error,"Saved second sketch source is stale.");return false;
                }
                TopoDS_Shape cutter,finalShape;
                if(!rebuildLinkedSketchSolid(second,child.inPlaceFollowDepthMm,
                                             child.inPlaceFollowOffsetMm,&cutter,error)||
                   !linkedBooleanResult(expected,cutter,child.inPlaceFollowOperation,&finalShape,error))return false;
                if(child.inPlaceFollowOperation=="Cut"&&!linkedSketchCutEffective(expected,finalShape,error))return false;
                if(child.inPlaceFollowOperation=="Fuse"&&!linkedSketchFuseEffective(expected,finalShape,error))return false;
                expected=finalShape;
            }
            for(const auto& stage:child.extraInPlaceSketchStages){
                const int extraIndex=savedIndices.value(stage.sourceId,-1);
                if(extraIndex<0||extraIndex==i){
                    setError(error,"Saved ordered sketch source is missing or circular.");return false;
                }
                const Part& extraSource=candidate[extraIndex];
                if(!extraSource.hasConstrainedPolygonDefinition||
                   extraSource.constrainedPolygonGeometryToken!=extraSource.geometryToken||
                   stage.sourceToken!=extraSource.geometryToken){
                    setError(error,"Saved ordered sketch stage is stale.");return false;
                }
                TopoDS_Shape tool,following;
                if(!rebuildLinkedSketchSolid(extraSource,stage.depthMm,stage.offsetMm,&tool,error)||
                   !linkedBooleanResult(expected,tool,stage.operation,&following,error))return false;
                if(stage.operation=="Cut"&&!linkedSketchCutEffective(expected,following,error))return false;
                if(stage.operation=="Fuse"&&!linkedSketchFuseEffective(expected,following,error))return false;
                expected=following;
            }
            // The downstream general feature tree consumes the completed
            // external-sketch stack. Compare against its persisted input
            // rather than the final shape when both layers are present.
            const TopoDS_Shape savedInPlaceResult=
                child.hasShapeFeatureTree?child.shapeFeatureBase:child.shape;
            GProp_GProps x,y;BRepGProp::VolumeProperties(expected,x);BRepGProp::VolumeProperties(savedInPlaceResult,y);
            if(std::abs(x.Mass()-y.Mass())>std::max(1e-5,x.Mass()*1e-5)){
                setError(error,"Saved in-place sketch feature does not match its original geometry.");return false;
            }
            // Equal volume alone cannot detect a malicious or corrupted
            // translated feature. Compare the expected and saved B-rep bounds
            // as a second, independent persistence invariant.
            Bnd_Box expectedBounds,savedBounds;
            BRepBndLib::Add(expected,expectedBounds);
            BRepBndLib::Add(savedInPlaceResult,savedBounds);
            if(expectedBounds.IsVoid()||savedBounds.IsVoid()){
                setError(error,"Saved in-place sketch geometry has invalid bounds.");return false;
            }
            double ax0,ay0,az0,ax1,ay1,az1,bx0,by0,bz0,bx1,by1,bz1;
            expectedBounds.Get(ax0,ay0,az0,ax1,ay1,az1);
            savedBounds.Get(bx0,by0,bz0,bx1,by1,bz1);
            const double expectedCoords[]={ax0,ay0,az0,ax1,ay1,az1};
            const double savedCoords[]={bx0,by0,bz0,bx1,by1,bz1};
            for(int coord=0;coord<6;++coord){
                if(std::abs(expectedCoords[coord]-savedCoords[coord])>1e-4){
                    setError(error,"Saved in-place sketch geometry bounds differ from its source feature.");return false;
                }
            }
            continue;
        }
        if(child.hasLinkedSketchExtrusion){
            const int sourceIndex=savedIndices.value(child.linkedSketchSourceId,-1);
            if(sourceIndex<0){setError(error,"Saved linked sketch source is missing.");return false;}
            const Part& source=candidate[sourceIndex];
            if(source.geometryToken!=child.linkedSketchSourceToken){
                setError(error,"Saved linked sketch is stale; source token does not match.");return false;
            }
            TopoDS_Shape expected;
            if(!rebuildLinkedSketchSolid(source,child.linkedSketchDepthMm,
                                         child.linkedSketchOffsetMm,&expected,error)||
               !validShape(expected,error))return false;
            GProp_GProps expectedProps,savedProps;
            BRepGProp::VolumeProperties(expected,expectedProps);
            BRepGProp::VolumeProperties(child.shape,savedProps);
            if(std::abs(expectedProps.Mass()-savedProps.Mass())>
               std::max(1e-5,expectedProps.Mass()*1e-5)){
                setError(error,"Saved linked sketch solid differs from its source sketch.");return false;
            }
            Bnd_Box expectedBounds,actualBounds;
            BRepBndLib::AddOptimal(expected,expectedBounds,false,false);
            BRepBndLib::AddOptimal(child.shape,actualBounds,false,false);
            if(expectedBounds.IsVoid()!=actualBounds.IsVoid()){
                setError(error,"Invalid linked sketch bounds.");return false;
            }
            if(!expectedBounds.IsVoid()){
                double a[6],b[6];
                expectedBounds.Get(a[0],a[1],a[2],a[3],a[4],a[5]);
                actualBounds.Get(b[0],b[1],b[2],b[3],b[4],b[5]);
                for(int k=0;k<6;++k)if(std::abs(a[k]-b[k])>1e-4){
                    setError(error,"Saved linked sketch placement differs from its source.");return false;
                }
            }
            continue;
        }
        if(!child.hasLinkedBoolean)continue;
        const int a=savedIndices.value(child.linkedPrimaryId,-1);
        const int b=savedIndices.value(child.linkedToolId,-1);
        if(a<0||b<0||a==b||candidate[a].isSurfaceBody||candidate[b].isSurfaceBody){
            setError(error,"Saved linked Boolean refers to invalid, cyclic, or missing sources.");return false;
        }
        if(child.linkedPrimaryToken==candidate[a].geometryToken&&child.linkedToolToken==candidate[b].geometryToken){
            TopoDS_Shape expected;
            if(!linkedBooleanResult(candidate[a].shape,candidate[b].shape,child.linkedOperation,&expected,error))return false;
            if(child.linkedOperation==QStringLiteral("Fuse")&&candidate[b].hasLinkedSketchExtrusion&&
               !linkedSketchFuseEffective(candidate[a].shape,expected,error))return false;
            if(child.linkedOperation==QStringLiteral("Cut")&&candidate[b].hasLinkedSketchExtrusion&&
               !linkedSketchCutEffective(candidate[a].shape,expected,error))return false;
            GProp_GProps x,y;BRepGProp::VolumeProperties(expected,x);BRepGProp::VolumeProperties(child.shape,y);
            if(std::abs(x.Mass()-y.Mass())>std::max(1e-5,x.Mass()*1e-5)){
                setError(error,"Saved linked Boolean geometry is inconsistent with its sources.");return false;
            }
        }
    }
    for(const Part& child:candidate){
        if(child.assemblyJointType.isEmpty())continue;
        if(child.componentDefinitionId.isEmpty()){
            setError(error,"Saved assembly joint child is not a component occurrence.");return false;
        }
        const auto parentIt=std::find_if(candidate.cbegin(),candidate.cend(),[&](const Part& part){return part.id==child.assemblyJointParentBodyId;});
        if(parentIt==candidate.cend()||parentIt->id==child.id||parentIt->componentDefinitionId.isEmpty()){
            setError(error,"Saved assembly joint refers to a missing or invalid parent occurrence.");return false;
        }
        QSet<QString> chain;const Part* cursor=&child;
        while(cursor&&!cursor->assemblyJointParentBodyId.isEmpty()){
            if(chain.contains(cursor->id)){setError(error,"Saved assembly joints contain a cycle.");return false;}
            chain.insert(cursor->id);
            const auto next=std::find_if(candidate.cbegin(),candidate.cend(),[&](const Part& part){return part.id==cursor->assemblyJointParentBodyId;});
            if(next==candidate.cend()){setError(error,"Saved assembly joint refers to a missing parent occurrence.");return false;}
            cursor=&*next;
        }
    }
    // Older scene files did not store a BOM/material revision for linked
    // drawings. Establish a baseline after every source's metadata is loaded.
    for(Part& sheet:candidate)if(sheet.hasDrawingSettings&&sheet.drawingSettings.sourceDataToken.isEmpty()){
        QStringList sourceIds=sheet.drawingSettings.sourceBodyIds;if(sourceIds.isEmpty())sourceIds.append(sheet.id);
        QVector<const Part*> sources;sources.reserve(sourceIds.size());
        for(const QString& sourceId:sourceIds){
            const auto source=std::find_if(candidate.cbegin(),candidate.cend(),[&](const Part& part){return part.id==sourceId;});
            if(source!=candidate.cend())sources.append(&*source);
        }
        sheet.drawingSettings.sourceDataToken=drawingSourceDataFingerprint(sources);
    }
    for(const Part& sheet:candidate)if(sheet.hasDrawingSettings&&!validDrawingSettings(sheet.drawingSettings,error))return false;
    // Validate cross-sheet identities after parsing all bodies, before the
    // transactional commit. A corrupted scene must not replace the open model.
    QSet<QString> drawingSheetIds;
    for(const Part& part:candidate){
        if(!part.hasDrawingSettings)continue;
        if(drawingSheetIds.contains(part.drawingSettings.sheetId)){
            setError(error,"Saved project contains duplicate persistent drawing sheet identities.");return false;
        }
        drawingSheetIds.insert(part.drawingSettings.sheetId);
    }
    // Optional v24 local-frame rigid network. Validate the entire serialized
    // graph before committing any scene state, including on malformed input.
    QString loadedMateFixed;
    QVector<AssemblyNetworkMate> loadedMates;
    if(root.contains("live_rigid_mates")){
        if(schema!=kSchema24||!root.value("live_rigid_mates").isObject()){
            setError(error,"Invalid saved live-mate network format.");return false;
        }
        const QJsonObject archive=root.value("live_rigid_mates").toObject();
        if(archive.size()!=3||archive.value("version").toInt(-1)!=1||
           !archive.value("fixed").isString()||!archive.value("connections").isArray()){
            setError(error,"Malformed live-mate record or unknown version.");return false;
        }
        loadedMateFixed=archive.value("fixed").toString();
        const QJsonArray links=archive.value("connections").toArray();
        if(loadedMateFixed.isEmpty()||links.isEmpty()||links.size()>128){
            setError(error,"Saved live mates have no fixed component or invalid connection count.");return false;
        }
        const auto knownComponent=[&](const QString& id){
            return std::any_of(candidate.cbegin(),candidate.cend(),[&](const Part& p){
                return p.id==id&&!p.componentDefinitionId.isEmpty()&&p.assemblyJointType.isEmpty();
            });
        };
        if(!knownComponent(loadedMateFixed)){
            setError(error,"Saved live-mate fixed occurrence is missing or not independent.");return false;
        }
        const auto readVec=[](const QJsonValue& raw,mates::Vec3* value){
            if(!raw.isArray())return false;
            const auto components=raw.toArray();
            if(components.size()!=3)return false;
            double a[3]{};
            for(int i=0;i<3;++i){
                if(!components[i].isDouble()||
                   !std::isfinite(components[i].toDouble())||
                   std::abs(components[i].toDouble())>1e6)return false;
                a[i]=components[i].toDouble();
            }
            *value={a[0],a[1],a[2]};return true;
        };
        QStringList bodyIds{loadedMateFixed};
        std::vector<mates::NetworkConnection> graph;
        for(const auto& rawLink:links){
            if(!rawLink.isObject()){
                setError(error,"Saved mate connection must be an object.");return false;
            }
            const QJsonObject link=rawLink.toObject();
            if(link.size()!=3||!link.value("parent").isString()||
               !link.value("child").isString()||!link.value("constraints").isArray()){
                setError(error,"Malformed saved rigid mate connection.");return false;
            }
            AssemblyNetworkMate entry;
            entry.parentBodyId=link.value("parent").toString();
            entry.childBodyId=link.value("child").toString();
            if(entry.parentBodyId==entry.childBodyId||!knownComponent(entry.parentBodyId)||
               !knownComponent(entry.childBodyId)){
                setError(error,"Saved mate refers to an invalid or jointed component.");return false;
            }
            if(!bodyIds.contains(entry.parentBodyId))bodyIds.append(entry.parentBodyId);
            if(!bodyIds.contains(entry.childBodyId))bodyIds.append(entry.childBodyId);
            const QJsonArray observations=link.value("constraints").toArray();
            if(observations.isEmpty()||observations.size()>32){
                setError(error,"Saved mate has invalid constraint count.");return false;
            }
            for(const auto& raw:observations){
                if(!raw.isObject()){
                    setError(error,"Saved mate observation must be an object.");return false;
                }
                const QJsonObject record=raw.toObject();
                const QJsonValue kind=record.value("kind");
                if((record.size()!=5&&record.size()!=6)||!kind.isDouble()||
                   kind.toInt(-1)<0||kind.toInt(-1)>2||kind.toDouble()!=kind.toInt(-1)){
                    setError(error,"Invalid stored mate observation type.");return false;
                }
                mates::Constraint observation;
                observation.kind=static_cast<mates::Kind>(kind.toInt());
                if(record.contains("plane_offset_mm")){
                    const auto offset=record.value("plane_offset_mm");
                    if(record.size()!=6||observation.kind!=mates::Kind::CoincidentPlane||
                       !offset.isDouble()||!std::isfinite(offset.toDouble())||
                       std::abs(offset.toDouble())>1e6){
                        setError(error,"Invalid saved plane offset observation.");return false;
                    }
                    observation.offsetMm=offset.toDouble();
                }else if(record.size()!=5){
                    setError(error,"Unexpected saved mate observation field.");return false;
                }
                if(!readVec(record.value("parent_point"),&observation.parentPoint)||
                   !readVec(record.value("child_point"),&observation.childPoint)||
                   !readVec(record.value("parent_axis"),&observation.parentDirection)||
                   !readVec(record.value("child_axis"),&observation.childDirection)){
                    setError(error,"Invalid stored rigid mate vector.");return false;
                }
                entry.constraints.push_back(observation);
            }
            loadedMates.append(std::move(entry));
        }
        if(bodyIds.size()>32||bodyIds.size()<2){
            setError(error,"Saved live-mate network exceeds supported component count.");return false;
        }
        for(const auto& link:loadedMates){
            graph.push_back({static_cast<std::size_t>(bodyIds.indexOf(link.parentBodyId)),
                             static_cast<std::size_t>(bodyIds.indexOf(link.childBodyId)),
                             link.constraints});
        }
        mates::NetworkSolution validated;std::string detail;
        if(!mates::solveRigidNetwork(static_cast<std::size_t>(bodyIds.size()),0,
                                     graph,&validated,&detail)){
            setError(error,QStringLiteral("Saved live-mate graph is invalid: ")+QString::fromStdString(detail));return false;
        }
    }
    // All optional articulated data is validated before the document commit.
    // Old v24 scenes that omit this record retain their original meaning.
    QString loadedArticulatedFixed;
    QVector<ArticulatedNetworkJoint> loadedArticulatedJoints;
    QMap<QString,mates::Transform> loadedArticulatedReferences;
    if(root.contains("live_articulated_mates")){
        if(schema!=kSchema24||!root.value("live_articulated_mates").isObject()||
           !loadedMateFixed.isEmpty()){
            setError(error,"Invalid or conflicting saved articulated network format.");return false;
        }
        const auto network=root.value("live_articulated_mates").toObject();
        if(network.size()!=4||network.value("version").toInt(-1)!=1||
           !network.value("fixed").isString()||
           !network.value("joints").isArray()||
           !network.value("reference_poses").isArray()){
            setError(error,"Malformed articulated record or unsupported version.");return false;
        }
        loadedArticulatedFixed=network.value("fixed").toString();
        const auto hasFreeOccurrence=[&](const QString& id){
            return std::any_of(candidate.cbegin(),candidate.cend(),[&](const Part& part){
                return part.id==id&&!part.componentDefinitionId.isEmpty()&&
                       part.assemblyJointType.isEmpty();
            });
        };
        if(loadedArticulatedFixed.isEmpty()||!hasFreeOccurrence(loadedArticulatedFixed)){
            setError(error,"Saved articulated fixed occurrence is invalid.");return false;
        }
        const auto readVec=[](const QJsonValue& source,mates::Vec3* value){
            if(!source.isArray()||source.toArray().size()!=3||!value)return false;
            const QJsonArray xyz=source.toArray();double data[3]{};
            for(int i=0;i<3;++i){
                if(!xyz[i].isDouble()||!std::isfinite(xyz[i].toDouble())||
                   std::abs(xyz[i].toDouble())>=1e6)return false;
                data[i]=xyz[i].toDouble();
            }
            *value={data[0],data[1],data[2]};return true;
        };
        const auto readFrame=[&](const QJsonValue& source,mates::Transform* frame){
            if(!source.isObject()||!frame)return false;
            const auto item=source.toObject();
            if(item.size()!=2||!readVec(item.value("translation"),&frame->translation)||
               !item.value("rotation").isArray()||item.value("rotation").toArray().size()!=9)
                return false;
            const auto rotation=item.value("rotation").toArray();
            for(int i=0;i<9;++i){
                if(!rotation[i].isDouble()||!std::isfinite(rotation[i].toDouble())||
                   std::abs(rotation[i].toDouble())>2.0)return false;
                frame->rotation[static_cast<size_t>(i)]=rotation[i].toDouble();
            }
            const auto& r=frame->rotation;
            const double determinant=r[0]*(r[4]*r[8]-r[5]*r[7])-
                                     r[1]*(r[3]*r[8]-r[5]*r[6])+
                                     r[2]*(r[3]*r[7]-r[4]*r[6]);
            if(!std::isfinite(determinant)||std::abs(determinant-1.0)>1e-5)return false;
            const auto identity=mates::multiply(*frame,mates::inverseRigid(*frame));
            for(int i=0;i<9;++i)
                if(std::abs(identity.rotation[static_cast<size_t>(i)]-(i%4==0?1.0:0.0))>1e-5)return false;
            return true;
        };
        const QJsonArray edges=network.value("joints").toArray();
        if(edges.isEmpty()||edges.size()>64){
            setError(error,"Saved articulated joint count is invalid.");return false;
        }
        QStringList ids{loadedArticulatedFixed};
        for(const auto& raw:edges){
            if(!raw.isObject()){
                setError(error,"Malformed articulated connection.");return false;
            }
            const auto joint=raw.toObject();
            const auto kind=joint.value("kind");
            if(joint.size()!=9||!joint.value("parent").isString()||
               !joint.value("child").isString()||!kind.isDouble()||
               kind.toInt(-1)<0||kind.toInt(-1)>2||kind.toDouble()!=kind.toInt(-1)||
               !joint.value("driven").isBool()||
               !joint.value("initial").isDouble()||!joint.value("min").isDouble()||
               !joint.value("max").isDouble()){
                setError(error,"Invalid saved articulated joint fields.");return false;
            }
            ArticulatedNetworkJoint parsed;
            parsed.parentBodyId=joint.value("parent").toString();
            parsed.childBodyId=joint.value("child").toString();
            if(parsed.parentBodyId==parsed.childBodyId||
               !hasFreeOccurrence(parsed.parentBodyId)||!hasFreeOccurrence(parsed.childBodyId)||
               !readVec(joint.value("axis"),&parsed.axis)||
               !readFrame(joint.value("child_at_zero"),&parsed.childAtZero)){
                setError(error,"Invalid saved articulated joint frame or endpoints.");return false;
            }
            parsed.kind=static_cast<articulated::JointKind>(kind.toInt());
            parsed.initial=joint.value("initial").toDouble();
            parsed.minimum=joint.value("min").toDouble();
            parsed.maximum=joint.value("max").toDouble();
            parsed.driven=joint.value("driven").toBool();
            loadedArticulatedJoints.append(parsed);
            if(!ids.contains(parsed.parentBodyId))ids.append(parsed.parentBodyId);
            if(!ids.contains(parsed.childBodyId))ids.append(parsed.childBodyId);
        }
        if(ids.size()<2||ids.size()>24){
            setError(error,"Saved articulated component count is invalid.");return false;
        }
        const auto frames=network.value("reference_poses").toArray();
        if(frames.size()!=ids.size()){
            setError(error,"Saved articulated reference count does not match graph components.");return false;
        }
        for(const auto& raw:frames){
            if(!raw.isObject()){
                setError(error,"Invalid articulated reference pose record.");return false;
            }
            const auto item=raw.toObject();
            const QString id=item.value("id").toString();
            mates::Transform pose;
            if(item.size()!=2||!item.value("id").isString()||
               !ids.contains(id)||loadedArticulatedReferences.contains(id)||
               !readFrame(item.value("frame"),&pose)){
                setError(error,"Invalid or duplicate articulated reference frame.");return false;
            }
            loadedArticulatedReferences.insert(id,pose);
        }
        std::vector<articulated::Joint> graph;
        graph.reserve(static_cast<size_t>(loadedArticulatedJoints.size()));
        for(const auto& joint:loadedArticulatedJoints){
            graph.push_back({static_cast<size_t>(ids.indexOf(joint.parentBodyId)),
                             static_cast<size_t>(ids.indexOf(joint.childBodyId)),joint.kind,
                             joint.childAtZero,joint.axis,joint.initial,joint.minimum,
                             joint.maximum,joint.driven});
        }
        articulated::Solution solution;std::string detail;
        if(!articulated::solveNetwork(static_cast<size_t>(ids.size()),0,graph,&solution,&detail)){
            setError(error,QStringLiteral("Saved articulated mechanism is invalid: ")+
                           QString::fromStdString(detail));return false;
        }
    }
    // Transactional load: no part of the original document is touched until all validation succeeds.
    parts_ = std::move(candidate);
    drawingReadinessPolicy_=candidateReadinessPolicy;
    liveRigidMateFixed_=loadedMateFixed;liveRigidMates_=std::move(loadedMates);
    liveArticulatedFixed_=loadedArticulatedFixed;
    liveArticulatedJoints_=std::move(loadedArticulatedJoints);
    liveArticulatedReferencePoses_=std::move(loadedArticulatedReferences);
    undo_.clear(); redo_.clear(); path_ = path;
    revision_ = 0; savedRevision_ = 0; nextRevision_ = 1;
    return true;
}

bool SceneDocument::importSTL(const QString& path,double weldToleranceMm,QString* error,
                              MeshImportSummary* repairSummary,bool fillPlanarHoles,bool importAsSurfaceBody) {
    if(path.trimmed().isEmpty()){setError(error,"Provide an STL file path.");return false;}
    const QFileInfo info(path);
    if(!info.exists()||!info.isFile()||info.size()<=0){setError(error,"STL file does not exist or is empty.");return false;}
    if(info.size()>kMaxStlImportBytes){setError(error,"STL file exceeds the 512 MiB import limit.");return false;}
    if(!std::isfinite(weldToleranceMm)||weldToleranceMm<1e-6||weldToleranceMm>1.0){setError(error,"STL weld tolerance must be between 0.000001 and 1 mm.");return false;}
    if(fillPlanarHoles&&importAsSurfaceBody){setError(error,"Choose either planar-hole filling for a solid or open surface-body import.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        TopoDS_Shape raw;
        StlAPI_Reader reader;
        if(!reader.Read(raw,QFile::encodeName(path).constData())||raw.IsNull()){
            setError(error,"Could not read STL facets from the selected file.");return false;
        }
        BRepMesh_IncrementalMesh mesher(raw,std::max(1e-4,weldToleranceMm*0.1),false,0.5,true);
        if(!mesher.IsDone()){setError(error,"Could not tessellate STL facets for mesh repair.");return false;}
        std::vector<mesh::Point3> sourceVertices;
        std::vector<mesh::Triangle> sourceTriangles;
        constexpr std::size_t maxRepairVertices=5000000;
        for(TopExp_Explorer faceExplorer(raw,TopAbs_FACE);faceExplorer.More();faceExplorer.Next()){
            const TopoDS_Face face=TopoDS::Face(faceExplorer.Current());
            TopLoc_Location location;
            const Handle(Poly_Triangulation) triangulation=BRep_Tool::Triangulation(face,location);
            if(triangulation.IsNull())continue;
            const gp_Trsf placement=location.Transformation();
            for(int index=1;index<=triangulation->NbTriangles();++index){
                if(sourceVertices.size()>maxRepairVertices-3){
                    setError(error,"STL mesh exceeds the 5,000,000-vertex repair limit.");return false;
                }
                int a=0,b=0,c=0;triangulation->Triangle(index).Get(a,b,c);
                if(face.Orientation()==TopAbs_REVERSED)std::swap(b,c);
                const int base=static_cast<int>(sourceVertices.size());
                for(const int node:{a,b,c}){
                    gp_Pnt point=triangulation->Node(node);point.Transform(placement);
                    sourceVertices.push_back({point.X(),point.Y(),point.Z()});
                }
                sourceTriangles.push_back({static_cast<std::uint32_t>(base),
                                           static_cast<std::uint32_t>(base+1),
                                           static_cast<std::uint32_t>(base+2)});
            }
        }
        if(sourceTriangles.empty()){setError(error,"STL file contains no usable triangulated facets.");return false;}
        mesh::RepairResult repaired;std::string repairError;
        if(!mesh::repairTriangleMesh(sourceVertices,sourceTriangles,weldToleranceMm,&repaired,&repairError)){
            setError(error,QStringLiteral("STL mesh repair failed: %1").arg(QString::fromStdString(repairError)));return false;
        }
        if(fillPlanarHoles&&!mesh::fillPlanarBoundaryHoles(&repaired,weldToleranceMm,64,&repairError)){
            setError(error,QStringLiteral("STL planar-hole repair failed: %1").arg(QString::fromStdString(repairError)));return false;
        }
        if(repaired.stats.nonManifoldEdges!=0||(!importAsSurfaceBody&&repaired.stats.boundaryEdges!=0)){
            setError(error,QStringLiteral("STL mesh is not a closed two-manifold after cleanup (%1 boundary edges, %2 non-manifold edges).")
                         .arg(static_cast<qulonglong>(repaired.stats.boundaryEdges))
                         .arg(static_cast<qulonglong>(repaired.stats.nonManifoldEdges)));return false;
        }
        BRep_Builder builder;TopoDS_Compound repairedMesh;builder.MakeCompound(repairedMesh);
        for(const mesh::Triangle& triangle:repaired.triangles){
            const auto& a=repaired.vertices[triangle.a];const auto& b=repaired.vertices[triangle.b];const auto& c=repaired.vertices[triangle.c];
            const gp_Pnt pa(a.x,a.y,a.z),pb(b.x,b.y,b.z),pc(c.x,c.y,c.z);
            BRepBuilderAPI_MakePolygon polygon;polygon.Add(pa);polygon.Add(pb);polygon.Add(pc);polygon.Close();
            if(!polygon.IsDone()){setError(error,"Could not construct a repaired STL triangle.");return false;}
            BRepBuilderAPI_MakeFace face(polygon.Wire());
            if(!face.IsDone()){setError(error,"Could not construct a repaired STL triangle face.");return false;}
            builder.Add(repairedMesh,face.Face());
        }
        BRepBuilderAPI_Sewing sewing(weldToleranceMm,true,true,true,false);sewing.Add(repairedMesh);sewing.Perform();
        const TopoDS_Shape stitched=sewing.SewedShape();if(stitched.IsNull()){setError(error,"STL facets could not be sewn into a closed surface.");return false;}
        const MeshImportSummary summary=summarizeMeshRepair(repaired.stats);
        if(importAsSurfaceBody){
            QVector<TopoDS_Shell> shells;
            for(TopExp_Explorer explorer(stitched,TopAbs_SHELL);explorer.More();explorer.Next())shells.append(TopoDS::Shell(explorer.Current()));
            if(shells.isEmpty()&&stitched.ShapeType()==TopAbs_FACE){
                TopoDS_Shell shell;builder.MakeShell(shell);builder.Add(shell,stitched);shells.append(shell);
            }
            if(shells.isEmpty()){setError(error,"Repaired STL facets did not produce a surface shell.");return false;}
            if(parts_.size()+shells.size()>kMaxObjects){setError(error,"STL surface import would exceed the scene body limit.");return false;}
            QVector<Part> surfaceParts;surfaceParts.reserve(shells.size());
            const QString base=info.completeBaseName().trimmed().isEmpty()?QStringLiteral("Imported STL"):info.completeBaseName();
            for(qsizetype i=0;i<shells.size();++i){
                QString validationError;
                if(!validSurfaceBodyShape(shells[i],&validationError)){
                    setError(error,QStringLiteral("STL surface component %1 is invalid: %2").arg(i+1).arg(validationError));return false;
                }
                const QString name=shells.size()==1?base:QStringLiteral("%1 Surface %2").arg(base).arg(i+1);
                Part part{generatedId(),uniqueName(name),shells[i],true,"#6c9dc2"};part.isSurfaceBody=true;
                surfaceParts.append(std::move(part));
            }
            checkpoint();for(Part& part:surfaceParts)parts_.append(std::move(part));
            if(repairSummary){*repairSummary=summary;repairSummary->importedAsSurfaceBody=true;}
            return true;
        }
        TopoDS_Compound solids;builder.MakeCompound(solids);int solidCount=0;
        for(TopExp_Explorer explorer(stitched,TopAbs_SOLID);explorer.More();explorer.Next()){
            const TopoDS_Shape solid=explorer.Current();if(!validShape(solid,error))return false;builder.Add(solids,solid);++solidCount;
        }
        if(solidCount==0){
            for(TopExp_Explorer explorer(stitched,TopAbs_SHELL);explorer.More();explorer.Next()){
                const TopoDS_Shell shell=TopoDS::Shell(explorer.Current());
                if(!BRep_Tool::IsClosed(shell)){setError(error,"STL mesh is open; only closed manifold meshes can be imported as solid bodies.");return false;}
                BRepBuilderAPI_MakeSolid makeSolid(shell);
                if(!makeSolid.IsDone()){setError(error,"Closed STL shell could not be converted to a faceted solid.");return false;}
                TopoDS_Solid solid=makeSolid.Solid();
                if(!BRepLib::OrientClosedSolid(solid)){setError(error,"Closed STL shell could not be consistently oriented.");return false;}
                if(!validShape(solid,error))return false;
                builder.Add(solids,solid);++solidCount;
            }
        }
        if(solidCount==0||!validShape(solids,error)){if(error&&error->isEmpty())setError(error,"STL file contains no closed solid mesh.");return false;}
        Part part{generatedId(),uniqueName(info.completeBaseName().trimmed().isEmpty()?QStringLiteral("Imported STL"):info.completeBaseName()),solids,true,"#6c9dc2"};
        checkpoint();parts_.append(std::move(part));if(repairSummary)*repairSummary=summary;return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("STL import kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::importOBJ(const QString& path,double weldToleranceMm,QString* error,
                              MeshImportSummary* repairSummary,bool fillPlanarHoles,bool importAsSurfaceBody) {
    if(path.trimmed().isEmpty()){setError(error,"Provide an OBJ file path.");return false;}
    const QFileInfo info(path);
    if(!info.exists()||!info.isFile()||info.size()<=0){setError(error,"OBJ file does not exist or is empty.");return false;}
    if(info.size()>kMaxStlImportBytes){setError(error,"OBJ file exceeds the 512 MiB import limit.");return false;}
    if(!std::isfinite(weldToleranceMm)||weldToleranceMm<1e-6||weldToleranceMm>1.0){setError(error,"OBJ weld tolerance must be between 0.000001 and 1 mm.");return false;}
    if(fillPlanarHoles&&importAsSurfaceBody){setError(error,"Choose either planar-hole filling for a solid or open surface-body import.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    QFile file(path);if(!file.open(QIODevice::ReadOnly|QIODevice::Text)){setError(error,file.errorString());return false;}
    try{
        QVector<gp_Pnt> vertices;vertices.reserve(1024);
        std::vector<mesh::Triangle> sourceTriangles;
        BRep_Builder builder;
        QTextStream input(&file);input.setLocale(QLocale::c());
        const QRegularExpression whitespace(QStringLiteral("\\s+"));
        qsizetype triangleCount=0;int lineNumber=0;
        while(!input.atEnd()){
            QString line=input.readLine();++lineNumber;
            // OBJ comments may follow data on the same line (for example, a face
            // followed by an exporter annotation). Strip them before tokenizing.
            const qsizetype commentStart=line.indexOf('#');
            if(commentStart>=0)line.truncate(commentStart);
            line=line.trimmed();
            if(line.isEmpty()||line.startsWith('#'))continue;
            const QStringList fields=line.split(whitespace,Qt::SkipEmptyParts);
            if(fields.first()==QStringLiteral("v")){
                if(fields.size()<4||vertices.size()>=5000000){setError(error,QStringLiteral("Invalid or excessive OBJ vertex data at line %1.").arg(lineNumber));return false;}
                double xyz[3];for(int axis=0;axis<3;++axis){bool ok=false;xyz[axis]=fields[axis+1].toDouble(&ok);if(!ok||!std::isfinite(xyz[axis])||std::abs(xyz[axis])>1e6){setError(error,QStringLiteral("Invalid OBJ vertex coordinate at line %1.").arg(lineNumber));return false;}}
                vertices.append(gp_Pnt(xyz[0],xyz[1],xyz[2]));
            }else if(fields.first()==QStringLiteral("f")){
                if(fields.size()<4){setError(error,QStringLiteral("OBJ face at line %1 has fewer than three vertices.").arg(lineNumber));return false;}
                QVector<int> indices;indices.reserve(fields.size()-1);
                for(int fieldIndex=1;fieldIndex<fields.size();++fieldIndex){
                    bool ok=false;const int rawIndex=fields[fieldIndex].section('/',0,0).toInt(&ok);
                    const qint64 resolved=rawIndex>0?qint64(rawIndex)-1:qint64(vertices.size())+rawIndex;
                    if(!ok||rawIndex==0||resolved<0||resolved>=vertices.size()){
                        setError(error,QStringLiteral("OBJ face at line %1 contains an invalid vertex index.").arg(lineNumber));return false;
                    }
                    indices.append(static_cast<int>(resolved));
                }
                for(int corner=1;corner+1<indices.size();++corner){
                    if(++triangleCount>5000000){setError(error,"OBJ mesh exceeds the 5,000,000 triangle limit.");return false;}
                    sourceTriangles.push_back({static_cast<std::uint32_t>(indices[0]),
                                               static_cast<std::uint32_t>(indices[corner]),
                                               static_cast<std::uint32_t>(indices[corner+1])});
                }
            }
        }
        if(vertices.size()<3||triangleCount<(importAsSurfaceBody?1:4)){
            setError(error,importAsSurfaceBody?"OBJ file contains too few vertices or facets to form a surface body."
                                              :"OBJ file contains too few vertices or faces to form a closed solid.");return false;
        }
        std::vector<mesh::Point3> sourcePoints;sourcePoints.reserve(static_cast<std::size_t>(vertices.size()));
        for(const gp_Pnt& point:vertices)sourcePoints.push_back({point.X(),point.Y(),point.Z()});
        mesh::RepairResult repaired;std::string repairError;
        if(!mesh::repairTriangleMesh(sourcePoints,sourceTriangles,weldToleranceMm,&repaired,&repairError)){
            setError(error,QStringLiteral("OBJ mesh repair failed: %1").arg(QString::fromStdString(repairError)));return false;
        }
        if(fillPlanarHoles&&!mesh::fillPlanarBoundaryHoles(&repaired,weldToleranceMm,64,&repairError)){
            setError(error,QStringLiteral("OBJ planar-hole repair failed: %1").arg(QString::fromStdString(repairError)));return false;
        }
        if(repaired.triangles.empty()||(!importAsSurfaceBody&&repaired.triangles.size()<4)||
           repaired.stats.nonManifoldEdges!=0||(!importAsSurfaceBody&&repaired.stats.boundaryEdges!=0)){
            setError(error,QStringLiteral("OBJ mesh is not a closed two-manifold after cleanup (%1 boundary edges, %2 non-manifold edges).")
                         .arg(static_cast<qulonglong>(repaired.stats.boundaryEdges))
                         .arg(static_cast<qulonglong>(repaired.stats.nonManifoldEdges)));return false;
        }
        TopoDS_Compound triangles;builder.MakeCompound(triangles);
        for(const mesh::Triangle& triangle:repaired.triangles){
            const auto& a=repaired.vertices[triangle.a];const auto& b=repaired.vertices[triangle.b];const auto& c=repaired.vertices[triangle.c];
            const gp_Pnt pa(a.x,a.y,a.z),pb(b.x,b.y,b.z),pc(c.x,c.y,c.z);
            BRepBuilderAPI_MakePolygon polygon;polygon.Add(pa);polygon.Add(pb);polygon.Add(pc);polygon.Close();
            if(!polygon.IsDone()){setError(error,"Could not construct a repaired OBJ triangle.");return false;}
            BRepBuilderAPI_MakeFace face(polygon.Wire());
            if(!face.IsDone()){setError(error,"Could not construct a repaired OBJ triangle face.");return false;}
            builder.Add(triangles,face.Face());
        }
        BRepBuilderAPI_Sewing sewing(weldToleranceMm,true,true,true,false);sewing.Add(triangles);sewing.Perform();
        const TopoDS_Shape stitched=sewing.SewedShape();if(stitched.IsNull()){setError(error,"OBJ facets could not be sewn into a closed surface.");return false;}
        const MeshImportSummary summary=summarizeMeshRepair(repaired.stats);
        if(importAsSurfaceBody){
            QVector<TopoDS_Shell> shells;
            for(TopExp_Explorer explorer(stitched,TopAbs_SHELL);explorer.More();explorer.Next())shells.append(TopoDS::Shell(explorer.Current()));
            if(shells.isEmpty()&&stitched.ShapeType()==TopAbs_FACE){
                TopoDS_Shell shell;builder.MakeShell(shell);builder.Add(shell,stitched);shells.append(shell);
            }
            if(shells.isEmpty()){setError(error,"Repaired OBJ facets did not produce a surface shell.");return false;}
            if(parts_.size()+shells.size()>kMaxObjects){setError(error,"OBJ surface import would exceed the scene body limit.");return false;}
            QVector<Part> surfaceParts;surfaceParts.reserve(shells.size());
            const QString base=info.completeBaseName().trimmed().isEmpty()?QStringLiteral("Imported OBJ"):info.completeBaseName();
            for(qsizetype i=0;i<shells.size();++i){
                QString validationError;
                if(!validSurfaceBodyShape(shells[i],&validationError)){
                    setError(error,QStringLiteral("OBJ surface component %1 is invalid: %2").arg(i+1).arg(validationError));return false;
                }
                const QString name=shells.size()==1?base:QStringLiteral("%1 Surface %2").arg(base).arg(i+1);
                Part part{generatedId(),uniqueName(name),shells[i],true,"#6c9dc2"};part.isSurfaceBody=true;
                surfaceParts.append(std::move(part));
            }
            checkpoint();for(Part& part:surfaceParts)parts_.append(std::move(part));
            if(repairSummary){*repairSummary=summary;repairSummary->importedAsSurfaceBody=true;}
            return true;
        }
        TopoDS_Compound solids;builder.MakeCompound(solids);int solidCount=0;
        for(TopExp_Explorer explorer(stitched,TopAbs_SOLID);explorer.More();explorer.Next()){
            const TopoDS_Shape solid=explorer.Current();if(!validShape(solid,error))return false;builder.Add(solids,solid);++solidCount;
        }
        if(solidCount==0){
            for(TopExp_Explorer explorer(stitched,TopAbs_SHELL);explorer.More();explorer.Next()){
                const TopoDS_Shell shell=TopoDS::Shell(explorer.Current());
                if(!BRep_Tool::IsClosed(shell)){setError(error,"OBJ mesh is open; only closed manifold meshes can be imported as solid bodies.");return false;}
                BRepBuilderAPI_MakeSolid makeSolid(shell);if(!makeSolid.IsDone()){setError(error,"Closed OBJ shell could not be converted to a faceted solid.");return false;}
                TopoDS_Solid solid=makeSolid.Solid();if(!BRepLib::OrientClosedSolid(solid)||!validShape(solid,error))return false;
                builder.Add(solids,solid);++solidCount;
            }
        }
        if(solidCount==0||!validShape(solids,error)){if(error&&error->isEmpty())setError(error,"OBJ file contains no closed solid mesh.");return false;}
        Part part{generatedId(),uniqueName(info.completeBaseName().trimmed().isEmpty()?QStringLiteral("Imported OBJ"):info.completeBaseName()),solids,true,"#6c9dc2"};
        checkpoint();parts_.append(std::move(part));if(repairSummary)*repairSummary=summary;return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("OBJ mesh kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::importSTEP(const QString& path, QString* error) {
    if (path.trimmed().isEmpty()) { setError(error,"Provide a STEP file path."); return false; }
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile() || fileInfo.size() <= 0) {
        setError(error,"STEP file does not exist or is empty."); return false;
    }
    if (fileInfo.size() > kMaxStepImportBytes) {
        setError(error,"STEP file exceeds the 512 MiB import limit."); return false;
    }
    if (parts_.size() >= kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    try {
        STEPCAFControl_Reader reader;
        reader.SetNameMode(true);
        reader.SetColorMode(true);
        if (reader.ReadFile(QFile::encodeName(path).constData()) != IFSelect_RetDone) {
            setError(error,"Could not read the STEP file."); return false;
        }
        auto application=XCAFApp_Application::GetApplication();
        occ::handle<TDocStd_Document> xdeDocument;
        application->NewDocument(TCollection_ExtendedString("MDTV-XCAF"),xdeDocument);
        if(xdeDocument.IsNull()||!reader.Transfer(xdeDocument)){
            setError(error,"The STEP file contains no transferable assembly or solid geometry.");return false;
        }
        const auto shapeTool=XCAFDoc_DocumentTool::ShapeTool(xdeDocument->Main());
        if(shapeTool.IsNull()){setError(error,"Could not read the STEP assembly structure.");return false;}
        const QString fileBase = fileInfo.completeBaseName().trimmed();
        const QString baseName = fileBase.isEmpty() ? QStringLiteral("Imported STEP") : fileBase;
        QVector<Part> imported;
        imported.reserve(16);
        QSet<QString> reservedNames;
        QHash<QString,QString> definitionIds;
        NCollection_Sequence<TDF_Label> roots;
        shapeTool->GetFreeShapes(roots);
        if(roots.IsEmpty()){setError(error,"The STEP file contains no top-level assembly or shape.");return false;}
        auto labelName=[](const TDF_Label& label){
            occ::handle<TDataStd_Name> attribute;
            if(!label.FindAttribute(TDataStd_Name::GetID(),attribute)||attribute.IsNull())return QString();
            const TCollection_AsciiString ascii(attribute->Get(),'?');
            return QString::fromUtf8(ascii.ToCString()).trimmed();
        };
        std::function<bool(const TDF_Label&,const QStringList&,const TopLoc_Location&)> collect;
        collect=[&](const TDF_Label& instance,const QStringList& parents,const TopLoc_Location& parentLocation){
            TDF_Label definition=instance;
            TopLoc_Location location=parentLocation;
            if(XCAFDoc_ShapeTool::IsReference(instance)){
                if(!XCAFDoc_ShapeTool::GetReferredShape(instance,definition))return false;
                location=location*XCAFDoc_ShapeTool::GetLocation(instance);
            }
            const QString instanceName=labelName(instance);
            const QString definitionName=labelName(definition);
            const QString sourceName=instanceName.isEmpty()?definitionName:instanceName;
            if(XCAFDoc_ShapeTool::IsAssembly(definition)){
                NCollection_Sequence<TDF_Label> components;
                if(!XCAFDoc_ShapeTool::GetComponents(definition,components,false))return false;
                QStringList path=parents;
                const QString segment=sourceName.isEmpty()?baseName:sourceName;
                if(path.isEmpty()||path.last()!=segment)path.append(segment);
                for(int i=1;i<=components.Length();++i)if(!collect(components.Value(i),path,location))return false;
                return true;
            }
            TopoDS_Shape sourceShape=XCAFDoc_ShapeTool::GetShape(definition);
            if(sourceShape.IsNull())return true; // Non-geometric reference leaves are not CAD bodies.
            TopoDS_Shape placedShape=sourceShape;
            if(!location.IsIdentity())placedShape=BRepBuilderAPI_Transform(sourceShape,location.Transformation(),true).Shape();
            NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> solids;
            TopExp::MapShapes(placedShape,TopAbs_SOLID,solids);
            for(int solidIndex=1;solidIndex<=solids.Extent();++solidIndex){
                if(parts_.size()+imported.size()>=kMaxObjects){setError(error,"Import would exceed the scene object limit.");return false;}
                const TopoDS_Shape solid=solids(solidIndex);
                QString validationError;
                if(!validShape(solid,&validationError)){setError(error,QStringLiteral("Invalid STEP body: %1").arg(validationError));return false;}
                const QString defKey=[&]{TCollection_AsciiString entry;TDF_Tool::Entry(definition,entry);return QString::fromLatin1(entry.ToCString())+QStringLiteral("/")+QString::number(solidIndex);}();
                QString definitionId=definitionIds.value(defKey);
                if(definitionId.isEmpty()){definitionId=QUuid::createUuid().toString(QUuid::WithoutBraces);definitionIds.insert(defKey,definitionId);}
                QString componentName=definitionName.isEmpty()?sourceName:definitionName;
                static const QRegularExpression genericStepName(QStringLiteral("^(SOLID|COMPOUND|SHAPE|ASSEMBLY|PART)([ _-]*[0-9]+)?$"),QRegularExpression::CaseInsensitiveOption);
                if(componentName.isEmpty()||genericStepName.match(componentName).hasMatch()||
                   componentName.startsWith(QStringLiteral("Open CASCADE STEP translator"),Qt::CaseInsensitive))
                    componentName=baseName+(solids.Extent()>1?QStringLiteral(" %1").arg(solidIndex):QString());
                QString candidate=sourceName;
                if(candidate.isEmpty()||genericStepName.match(candidate).hasMatch()||
                   candidate.startsWith(QStringLiteral("Open CASCADE STEP translator"),Qt::CaseInsensitive))candidate=componentName;
                if(solids.Extent()>1)candidate+=QStringLiteral(" %1").arg(solidIndex);
                const QString requested=candidate;
                int suffix=2;
                while(reservedNames.contains(candidate.toCaseFolded())||uniqueName(candidate)!=candidate)candidate=requested+QStringLiteral(" %1").arg(suffix++);
                reservedNames.insert(candidate.toCaseFolded());
                Part part{generatedId(),candidate,solid,true,"#6c9dc2"};
                part.assemblyPath=parents.isEmpty()?baseName:parents.join('/');
                part.componentDefinitionId=definitionId;
                part.componentName=componentName;
                part.occurrenceId=QUuid::createUuid().toString(QUuid::WithoutBraces);
                const gp_Vec translation=location.Transformation().TranslationPart();
                part.placementX=translation.X();part.placementY=translation.Y();part.placementZ=translation.Z();
                imported.append(std::move(part));
            }
            return true;
        };
        for(int i=1;i<=roots.Length();++i){
            if(!collect(roots.Value(i),{},TopLoc_Location())){
                if(error&&error->isEmpty())setError(error,"Could not traverse the STEP assembly structure.");
                return false;
            }
        }
        if(imported.isEmpty()){setError(error,"The STEP file contains no solid bodies.");return false;}

        checkpoint();
        for (Part& part : imported) parts_.append(std::move(part));
        return true;
    } catch (const Standard_Failure& e) {
        setError(error,QStringLiteral("STEP import kernel error: %1").arg(e.what()));
    } catch (const std::exception& e) {
        setError(error,QString::fromUtf8(e.what()));
    }
    return false;
}

bool SceneDocument::importIGES(const QString& path,QString* error) {
    if(path.trimmed().isEmpty()){setError(error,"Provide an IGES file path.");return false;}
    const QFileInfo info(path);
    if(!info.exists()||!info.isFile()||info.size()<=0){setError(error,"IGES file does not exist or is empty.");return false;}
    if(info.size()>kMaxIgesImportBytes){setError(error,"IGES file exceeds the 512 MiB import limit.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        IGESControl_Reader reader;
        if(reader.ReadFile(QFile::encodeName(path).constData())!=IFSelect_RetDone){setError(error,"Could not read the IGES file.");return false;}
        if(reader.NbRootsForTransfer()<=0||reader.TransferRoots()<=0){setError(error,"The IGES file contains no transferable geometry.");return false;}
        const TopoDS_Shape translated=reader.OneShape();
        if(translated.IsNull()){setError(error,"The IGES translator produced no geometry.");return false;}
        NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> solids;TopExp::MapShapes(translated,TopAbs_SOLID,solids);
        if(solids.IsEmpty()){setError(error,"IGES import found no solid bodies; open surface entities are not converted to solids.");return false;}
        if(parts_.size()+solids.Extent()>kMaxObjects){setError(error,"IGES import would exceed the scene object limit.");return false;}
        const QString base=info.completeBaseName().trimmed().isEmpty()?QStringLiteral("Imported IGES"):info.completeBaseName().trimmed();
        QVector<Part> imported;imported.reserve(solids.Extent());QSet<QString> names;
        for(int i=1;i<=solids.Extent();++i){
            QString validationError;const TopoDS_Shape solid=solids(i);
            if(!validShape(solid,&validationError)){setError(error,QStringLiteral("Invalid IGES solid: %1").arg(validationError));return false;}
            QString requested=solids.Extent()==1?base:base+QStringLiteral(" %1").arg(i),name=requested;int suffix=2;
            while(names.contains(name.toCaseFolded())||uniqueName(name)!=name)name=requested+QStringLiteral(" %1").arg(suffix++);
            names.insert(name.toCaseFolded());Part part{generatedId(),name,solid,true,"#6c9dc2"};
            part.assemblyPath=base;part.componentName=name;part.componentDefinitionId=QUuid::createUuid().toString(QUuid::WithoutBraces);
            part.occurrenceId=QUuid::createUuid().toString(QUuid::WithoutBraces);imported.append(std::move(part));
        }
        checkpoint();for(Part& part:imported)parts_.append(std::move(part));return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("IGES import kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::exportSTEP(const QString& path, QString* error) const {
    if (parts_.isEmpty()) { setError(error,"No geometry to export."); return false; }
    try {
        auto application=XCAFApp_Application::GetApplication();
        occ::handle<TDocStd_Document> xdeDocument;
        application->NewDocument(TCollection_ExtendedString("MDTV-XCAF"),xdeDocument);
        if(xdeDocument.IsNull()){setError(error,"Could not create an XDE document for STEP export.");return false;}
        const auto shapeTool=XCAFDoc_DocumentTool::ShapeTool(xdeDocument->Main());
        if(shapeTool.IsNull()){setError(error,"Could not create the STEP assembly structure.");return false;}
        TDF_Label root=shapeTool->NewShape();
        const QString rootName=QFileInfo(path).completeBaseName().trimmed().isEmpty()?QStringLiteral("CADWorkbench Assembly"):QFileInfo(path).completeBaseName().trimmed();
        TDataStd_Name::Set(root,TCollection_ExtendedString(reinterpret_cast<const char16_t*>(rootName.utf16())));
        QHash<QString,TDF_Label> groups;groups.insert(QString(),root);
        QHash<QString,TDF_Label> definitions;
        QHash<QString,QVector<TopoDS_Shape>> definitionVariants;
        auto sameSolidGeometry=[](const TopoDS_Shape& a,const TopoDS_Shape& b){
            GProp_GProps propsA,propsB;BRepGProp::VolumeProperties(a,propsA);BRepGProp::VolumeProperties(b,propsB);
            const double volumeA=propsA.Mass(),volumeB=propsB.Mass();
            const double tolerance=1e-7*std::max({1.0,std::abs(volumeA),std::abs(volumeB)});
            if(std::abs(volumeA-volumeB)>tolerance)return false;
            BRepAlgoAPI_Common common(a,b);common.Build();if(!common.IsDone()||common.HasErrors())return false;
            GProp_GProps commonProps;BRepGProp::VolumeProperties(common.Shape(),commonProps);
            return std::abs(commonProps.Mass()-volumeA)<=tolerance;
        };
        for(const Part& part:parts_){
            TDF_Label destination=root;QStringList segments;
            for(const QString& segment:part.assemblyPath.split('/',Qt::SkipEmptyParts)){
                segments.append(segment);const QString key=segments.join('/');
                if(!groups.contains(key)){
                    TDF_Label group=shapeTool->NewShape();
                    const TCollection_ExtendedString groupName(reinterpret_cast<const char16_t*>(segment.utf16()));
                    TDataStd_Name::Set(group,groupName);
                    const TDF_Label groupOccurrence=shapeTool->AddComponent(destination,group,TopLoc_Location());
                    if(!groupOccurrence.IsNull())TDataStd_Name::Set(groupOccurrence,groupName);
                    groups.insert(key,group);
                }
                destination=groups.value(key);
            }
            TopoDS_Shape localShape=part.shape;
            const bool hasComponent=!part.componentDefinitionId.isEmpty();
            if(hasComponent&&(part.placementX!=0.0||part.placementY!=0.0||part.placementZ!=0.0)){
                gp_Trsf inversePlacement;inversePlacement.SetTranslation(gp_Vec(-part.placementX,-part.placementY,-part.placementZ));
                localShape=BRepBuilderAPI_Transform(part.shape,inversePlacement,true).Shape();
            }
            const QString family=hasComponent?part.componentDefinitionId:QStringLiteral("body/")+part.id;
            QString definitionKey;
            if(hasComponent){
                QVector<TopoDS_Shape>& variants=definitionVariants[family];
                for(int i=0;i<variants.size();++i)if(sameSolidGeometry(variants[i],localShape)){
                    definitionKey=family+QStringLiteral("/")+QString::number(i);break;
                }
                if(definitionKey.isEmpty())definitionKey=family+QStringLiteral("/")+QString::number(variants.size());
            }else definitionKey=family;
            TDF_Label definition=definitions.value(definitionKey);
            if(definition.IsNull()){
                definition=shapeTool->AddShape(localShape,false,false);
                const QString definitionName=part.componentName.isEmpty()?part.name:part.componentName;
                TDataStd_Name::Set(definition,TCollection_ExtendedString(reinterpret_cast<const char16_t*>(definitionName.utf16())));
                definitions.insert(definitionKey,definition);
                if(hasComponent)definitionVariants[family].append(localShape);
            }
            gp_Trsf placement;placement.SetTranslation(gp_Vec(part.placementX,part.placementY,part.placementZ));
            const TDF_Label occurrence=shapeTool->AddComponent(destination,definition,TopLoc_Location(placement));
            TDataStd_Name::Set(occurrence,TCollection_ExtendedString(reinterpret_cast<const char16_t*>(part.name.utf16())));
        }
        shapeTool->UpdateAssemblies();
        STEPCAFControl_Writer writer;writer.SetNameMode(true);writer.SetColorMode(true);
        if(!writer.Transfer(xdeDocument,STEPControl_AsIs)){setError(error,"STEP assembly transfer failed.");return false;}
        if (writer.Write(QFile::encodeName(path).constData()) != IFSelect_RetDone) { setError(error,"STEP assembly write failed."); return false; }
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("STEP kernel error: %1").arg(e.what())); return false; }
}
bool SceneDocument::exportIGES(const QString& path,QString* error) const {
    if(path.trimmed().isEmpty()){setError(error,"Provide an IGES output path.");return false;}
    if(parts_.isEmpty()){setError(error,"No geometry to export.");return false;}
    try{
        TopoDS_Compound compound;BRep_Builder builder;builder.MakeCompound(compound);
        for(const Part& part:parts_){QString validationError;if(!validShape(part.shape,&validationError)){
                setError(error,QStringLiteral("Could not export body '%1': %2").arg(part.name,validationError));return false;}
            builder.Add(compound,part.shape);
        }
        IGESControl_Writer writer("MM",1);
        if(!writer.AddShape(compound)){setError(error,"IGES writer rejected the scene geometry.");return false;}
        writer.ComputeModel();
        if(!writer.Write(QFile::encodeName(path).constData())){setError(error,"IGES file could not be written.");return false;}
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("IGES export kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}
bool SceneDocument::exportSTL(const QString& path, QString* error) const {
    if (parts_.isEmpty()) { setError(error,"No geometry to export."); return false; }
    try {
        TopoDS_Compound compound; BRep_Builder builder; builder.MakeCompound(compound);
        for (const auto& part : parts_) builder.Add(compound,part.shape);
        BRepMesh_IncrementalMesh mesher(compound,0.20,false,0.35,true);
        mesher.Perform();
        if (!mesher.IsDone()) { setError(error,"STL tessellation failed."); return false; }
        StlAPI_Writer writer;
        writer.ASCIIMode() = false;
        if (!writer.Write(compound,QFile::encodeName(path).constData())) { setError(error,"STL write failed."); return false; }
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("STL kernel error: %1").arg(e.what())); return false; }
}

bool SceneDocument::exportBodyScheduleCsv(const QString& path,QString* error) const {
    if(path.trimmed().isEmpty()){setError(error,"Provide a CSV output path.");return false;}
    if(parts_.isEmpty()){setError(error,"No bodies are available for the schedule.");return false;}
    QByteArray csv;
    QTextStream stream(&csv,QIODevice::WriteOnly);
    stream.setLocale(QLocale::c());
    const auto field=[](const QString& value){
        QString escaped=value;
        // External body names must be inert when the schedule is opened in Excel.
        const QString trimmed=escaped.trimmed();
        if(!trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front()))
            escaped.prepend(QChar(0x27));
        escaped.replace('"',"\"\"");
        return QStringLiteral("\"")+escaped+QStringLiteral("\"");
    };
    const auto number=[](double value){return QString::number(value,'g',15);};
    stream << "Body ID,Name,Visible,Volume (mm^3),Surface area (mm^2),Size X (mm),Size Y (mm),Size Z (mm),Center X (mm),Center Y (mm),Center Z (mm)\r\n";
    for(const Part& part:parts_){
        BodyDiagnostics geometry;
        QString geometryError;
        if(!diagnoseBody(part.id,&geometry,&geometryError)||!geometry.valid){
            setError(error,QStringLiteral("Could not export body '%1': %2").arg(part.name,geometryError));
            return false;
        }
        stream << field(part.id) << ',' << field(part.name) << ',' << (part.visible?"true":"false") << ','
               << number(geometry.volume) << ',' << number(geometry.surfaceArea) << ','
               << number(geometry.maxX-geometry.minX) << ',' << number(geometry.maxY-geometry.minY) << ','
               << number(geometry.maxZ-geometry.minZ) << ',' << number(geometry.centerX) << ','
               << number(geometry.centerY) << ',' << number(geometry.centerZ) << "\r\n";
    }
    stream.flush();
    if(stream.status()!=QTextStream::Ok){setError(error,"Could not format the body schedule CSV.");return false;}
    QSaveFile output(path);
    if(!output.open(QIODevice::WriteOnly)){setError(error,QStringLiteral("Could not create CSV file: %1").arg(output.errorString()));return false;}
    if(output.write(csv)!=csv.size()){setError(error,QStringLiteral("Could not write CSV file: %1").arg(output.errorString()));output.cancelWriting();return false;}
    if(!output.commit()){setError(error,QStringLiteral("Could not finalize CSV file: %1").arg(output.errorString()));return false;}
    return true;
}
} // namespace cad


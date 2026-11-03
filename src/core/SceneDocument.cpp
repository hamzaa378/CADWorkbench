#include "SceneDocument.h"
#include "MeshRepair.h"

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
constexpr qsizetype kMaxStlImportBytes = 512 * 1024 * 1024;

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
    std::sort(sources.begin(),sources.end(),[](const Part* a,const Part* b){return a->id<b->id;});
    QJsonArray records;
    for(const Part* source:sources){
        QJsonObject record;
        record.insert("id",source->id);record.insert("part_number",source->partNumber);
        record.insert("description",source->description);record.insert("revision",source->revision);
        record.insert("assembly_path",source->assemblyPath);record.insert("procurement_type",source->procurementType);
        record.insert("supplier",source->supplier);record.insert("supplier_part_number",source->supplierPartNumber);
        record.insert("material",source->materialName);record.insert("density_kg_per_m3",source->materialDensityKgPerM3);
        record.insert("material_provenance",source->materialEngineering.provenance);
        record.insert("youngs_modulus_gpa",source->materialEngineering.youngsModulusGPa);
        records.append(record);
    }
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(records).toJson(QJsonDocument::Compact),
                                                        QCryptographicHash::Sha256).toHex());
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
           !std::isfinite(dimension.offsetMm)||dimension.offsetMm<5.0||dimension.offsetMm>60.0){
            if(error)*error="Selected-edge dimensions need a unique ID, drawing source body, edge index, orthographic view, and 5-60 mm offset.";return false;
        }
        annotationIds.insert(dimension.id);
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
    return BRepPrimAPI_MakePrism(face.Face(),gp_Vec(0.0,0.0,width)).Shape();
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
    if(hasHole){
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
    QVector<Part> snapshotParts = parts_;
    for (auto& part : snapshotParts) part.shape = BRepBuilderAPI_Copy(part.shape).Shape();
    undo_.append({std::move(snapshotParts), drawingReadinessPolicy_, revision_});
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

bool SceneDocument::editFoldedSheetMetal(const QString& partId,const QVector<double>& flanges,
        double width,double thickness,const QVector<FoldedSheetBend>& bends,QString* error) {
    const auto found=std::find_if(parts_.begin(),parts_.end(),[&](const Part& part){return part.id==partId;});
    if(found==parts_.end()||!found->hasFoldedSheetMetalDefinition){setError(error,"Selected body has no editable folded sheet-metal definition.");return false;}
    if(found->geometryToken!=found->foldedSheetGeometryToken){setError(error,"Folded sheet-metal definition is stale because the body was changed by another modeling operation.");return false;}
    try{
        const TopoDS_Shape rebuilt=makeFoldedSheetSolid(flanges,width,thickness,bends,error);
        if(!validShape(rebuilt,error))return false;
        if(flanges==found->foldedSheetFlangesMm&&width==found->foldedSheetWidthMm&&
           thickness==found->foldedSheetThicknessMm&&bends.size()==found->foldedSheetBends.size()){
            bool same=true;for(int i=0;i<bends.size();++i)same=same&&bends[i].angleDegrees==found->foldedSheetBends[i].angleDegrees&&
                bends[i].insideRadiusMm==found->foldedSheetBends[i].insideRadiusMm;
            if(same)return true;
        }
        checkpoint();
        Part& part=*std::find_if(parts_.begin(),parts_.end(),[&](const Part& candidate){return candidate.id==partId;});
        part.shape=rebuilt;part.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        part.foldedSheetGeometryToken=part.geometryToken;part.foldedSheetFlangesMm=flanges;
        part.foldedSheetWidthMm=width;part.foldedSheetThicknessMm=thickness;part.foldedSheetBends=bends;
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
        checkpoint();
        for(const TranslationUpdate& update:updates){
            Part& target=parts_[update.index];target.shape=update.shape;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
            if(!target.componentDefinitionId.isEmpty()){target.placementX+=x;target.placementY+=y;target.placementZ+=z;}
            if(target.assemblyJointType==QStringLiteral("Revolute")||target.assemblyJointType==QStringLiteral("Prismatic")){
                target.jointOriginX+=x;target.jointOriginY+=y;target.jointOriginZ+=z;
            }
        }
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::rotate(const QString& id, Axis axis, double degrees, QString* error) {
    if (!std::isfinite(degrees) || std::abs(degrees) > 36000) { setError(error, "Angle must be finite and within Â±36,000 degrees."); return false; }
    int rootIndex=-1;for(int i=0;i<parts_.size();++i)if(parts_[i].id==id){rootIndex=i;break;}
    if (rootIndex<0) { setError(error, "Object not found."); return false; }
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
        checkpoint();
        for(const RotationUpdate& update:updates){
            Part& target=parts_[update.index];target.shape=update.shape;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
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
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::scale(const QString& id, double factor, QString* error) {
    if (!std::isfinite(factor) || factor < 1e-4 || factor > 1e4) { setError(error, "Scale must be 0.0001â€“10,000."); return false; }
    const Part* part = find(id);
    if (!part) { setError(error, "Object not found."); return false; }
    if (factor == 1.0) return true;
    try {
        gp_Trsf transform;
        transform.SetScale(gp_Pnt(0,0,0), factor);
        const TopoDS_Shape result = BRepBuilderAPI_Transform(part->shape, transform, true).Shape();
        if(part->isSurfaceBody?!validSurfaceBodyShape(result,error):!validShape(result,error))return false;
        checkpoint();
        for (auto& target : parts_) if (target.id == id) { target.shape = result; target.geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces); break; }
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
    if(factorX==1.0&&factorY==1.0&&factorZ==1.0)return true;
    try{
        const gp_GTrsf transform(gp_Mat(factorX,0,0,0,factorY,0,0,0,factorZ),gp_XYZ(0,0,0));
        const TopoDS_Shape result=BRepBuilderAPI_GTransform(part->shape,transform,true).Shape();
        if(part->isSurfaceBody?!validSurfaceBodyShape(result,error):!validShape(result,error))return false;
        checkpoint();
        for(auto& target:parts_)if(target.id==id){target.shape=result;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
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
    QVector<MotionInterferenceSample> candidate;candidate.reserve(sampleCount);
    for(int index=0;index<sampleCount;++index){
        const double time=startSeconds+(endSeconds-startSeconds)*index/(sampleCount-1);
        SceneDocument frame;frame.parts_=parts_;
        QString frameError;
        if(!frame.evaluateAssemblyMotionStudy(time,&frameError)){
            setError(error,QStringLiteral("Motion scan failed at %1 s: %2").arg(time,0,'f',3).arg(frameError));return false;
        }
        InterferenceResult interference;
        if(!frame.measureInterference(firstId,secondId,&interference,&frameError)){
            setError(error,QStringLiteral("Interference scan failed at %1 s: %2").arg(time,0,'f',3).arg(frameError));return false;
        }
        candidate.append({time,interference.volume,interference.volume>1e-7});
    }
    *samples=std::move(candidate);return true;
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
    report.state=((matchesAggregate||matchesSingleBody)&&metadataMatches)?DrawingSourceState::Current:DrawingSourceState::Stale;
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
                                                    double depth,QString* error) {
    if(!finitePositive(depth)||boundaryPointIds.size()<3||boundaryPointIds.size()>64||
       sketch.points.size()>64||sketch.segments.size()>128||sketch.circles.size()>64||sketch.constraints.size()>128){
        setError(error,"A constrained polygon needs 3–64 boundary points, finite positive depth, and at most 128 segments or constraints.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    const SketchSolveResult solved=SketchSolver::solve(sketch);
    if(!solved.success){setError(error,solved.message);return false;}
    QMap<QString,QPointF> positions;
    for(int i=0;i<sketch.points.size();++i)positions.insert(sketch.points[i].id,solved.solvedPoints[i]);
    QSet<QString> unique;
    QVector<QPointF> boundary;boundary.reserve(boundaryPointIds.size());
    for(const QString& id:boundaryPointIds){
        if(unique.contains(id)||!positions.contains(id)){setError(error,"Polygon boundary point IDs must be unique and present in the sketch.");return false;}
        unique.insert(id);const QPointF point=positions.value(id);
        if(!std::isfinite(point.x())||!std::isfinite(point.y())||std::abs(point.x())>1e6||std::abs(point.y())>1e6){
            setError(error,"Solved sketch point exceeds the supported coordinate range.");return false;
        }
        boundary.append(point);
    }
    const int count=boundary.size();
    double twiceArea=0.0;
    for(int i=0;i<count;++i){
        const QPointF a=boundary[i],b=boundary[(i+1)%count];
        if(std::hypot(a.x()-b.x(),a.y()-b.y())<1e-5){setError(error,"Polygon has a zero-length boundary edge.");return false;}
        twiceArea+=a.x()*b.y()-a.y()*b.x();
    }
    if(std::abs(twiceArea)<1e-8){setError(error,"Polygon boundary has zero area.");return false;}
    auto cross=[](const QPointF& a,const QPointF& b,const QPointF& c){
        return (b.x()-a.x())*(c.y()-a.y())-(b.y()-a.y())*(c.x()-a.x());
    };
    for(int i=0;i<count;++i)for(int j=i+1;j<count;++j){
        if(j==i+1||(i==0&&j==count-1))continue;
        const QPointF a=boundary[i],b=boundary[(i+1)%count],c=boundary[j],d=boundary[(j+1)%count];
        const double abC=cross(a,b,c),abD=cross(a,b,d),cdA=cross(c,d,a),cdB=cross(c,d,b);
        if((abC*abD<=0.0)&&(cdA*cdB<=0.0)){
            setError(error,"Polygon boundary crosses or touches itself.");return false;
        }
    }
    struct Cutout { QPointF center; double radius; };
    QVector<Cutout> cutouts;
    for(int circleIndex=0;circleIndex<sketch.circles.size();++circleIndex){
        const SketchCircle& circle=sketch.circles[circleIndex];
        if(!circle.cutout)continue;
        const QPointF center=positions.value(circle.centerPointId);
        const double radius=solved.solvedCircleRadii[circleIndex];
        if(!std::isfinite(center.x())||!std::isfinite(center.y())||
           std::abs(center.x())>1e6||std::abs(center.y())>1e6||!std::isfinite(radius)||radius<1e-5||radius>1e6){
            setError(error,"A circular sketch cutout exceeds the supported coordinate or radius range.");return false;
        }
        bool inside=false;
        double nearestEdge=std::numeric_limits<double>::infinity();
        for(int i=0,j=count-1;i<count;j=i++){
            const QPointF a=boundary[i],b=boundary[j];
            if((a.y()>center.y())!=(b.y()>center.y())&&
               center.x()<(b.x()-a.x())*(center.y()-a.y())/(b.y()-a.y())+a.x())inside=!inside;
            const double dx=b.x()-a.x(),dy=b.y()-a.y();
            const double t=std::clamp(((center.x()-a.x())*dx+(center.y()-a.y())*dy)/(dx*dx+dy*dy),0.0,1.0);
            nearestEdge=std::min(nearestEdge,std::hypot(center.x()-a.x()-t*dx,center.y()-a.y()-t*dy));
        }
        if(!inside||nearestEdge<=radius+1e-5){setError(error,"Circular sketch cutouts must lie entirely inside the polygon with clearance from its boundary.");return false;}
        for(const Cutout& other:cutouts)if(std::hypot(center.x()-other.center.x(),center.y()-other.center.y())<=radius+other.radius+1e-5){
            setError(error,"Circular sketch cutouts must not overlap or touch.");return false;
        }
        cutouts.append({center,radius});
    }
    try{
        BRepBuilderAPI_MakePolygon polygon;
        for(const QPointF& p:boundary)polygon.Add(gp_Pnt(p.x(),p.y(),0));
        polygon.Close();
        if(!polygon.IsDone()){setError(error,"Could not close the constrained polygon.");return false;}
        BRepBuilderAPI_MakeFace face(polygon.Wire());
        for(const Cutout& cutout:cutouts){
            const gp_Circ circle(gp_Ax2(gp_Pnt(cutout.center.x(),cutout.center.y(),0),gp_Dir(0,0,1)),cutout.radius);
            const TopoDS_Edge edge=BRepBuilderAPI_MakeEdge(circle).Edge();
            TopoDS_Wire wire=BRepBuilderAPI_MakeWire(edge).Wire();
            if(twiceArea>0)wire.Reverse();
            face.Add(wire);
        }
        if(!face.IsDone()){setError(error,"Could not build a face from the constrained polygon.");return false;}
        BRepPrimAPI_MakePrism prism(face.Face(),gp_Vec(0,0,depth));
        if(!prism.IsDone()||!validShape(prism.Shape(),error))return false;
        Part part{generatedId(),uniqueName(QStringLiteral("Constrained Polygon Extrusion")),prism.Shape()};
        part.hasConstrainedPolygonDefinition=true;part.constrainedPolygonSketch=sketch;
        part.constrainedPolygonBoundary=boundaryPointIds;part.constrainedPolygonDepthMm=depth;
        part.constrainedPolygonGeometryToken=part.geometryToken;
        checkpoint();parts_.append(std::move(part));
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Constrained sketch kernel error: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::updateConstrainedPolygonExtrusion(const QString& partId,const SketchModel& sketch,
                                                       const QStringList& boundaryPointIds,double depth,QString* error) {
    Part* part=nullptr;for(Part& candidate:parts_)if(candidate.id==partId){part=&candidate;break;}
    if(!part||!part->hasConstrainedPolygonDefinition){setError(error,"Select a constrained polygon extrusion.");return false;}
    if(part->geometryToken!=part->constrainedPolygonGeometryToken){
        setError(error,"The constrained sketch history is stale after a direct geometry edit.");return false;
    }
    if(depth==part->constrainedPolygonDepthMm&&boundaryPointIds==part->constrainedPolygonBoundary&&
       sketchModelToJson(sketch)==sketchModelToJson(part->constrainedPolygonSketch))return true;
    SceneDocument rebuilt;
    if(!rebuilt.addConstrainedPolygonExtrusion(sketch,boundaryPointIds,depth,error))return false;
    const TopoDS_Shape shape=rebuilt.parts().first().shape;
    checkpoint();part->shape=shape;part->geometryToken=generatedId();
    part->constrainedPolygonGeometryToken=part->geometryToken;
    part->constrainedPolygonSketch=sketch;part->constrainedPolygonBoundary=boundaryPointIds;
    part->constrainedPolygonDepthMm=depth;
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


bool SceneDocument::setNamedParameter(const QString& partId,const QString& parameterName,double value,QString* error) {
    return setNamedParameters(partId,QMap<QString,double>{{parameterName,value}},error);
}

bool SceneDocument::setNamedParameters(const QString& partId,const QMap<QString,double>& values,QString* error) {
    return setNamedParametersWithLinks(partId,values,parameterLinks(partId),false,error);
}

bool SceneDocument::setNamedParametersWithLinks(const QString& partId,const QMap<QString,double>& values,
        const QMap<QString,ParameterLink>& links,bool allowEmpty,QString* error) {
    int index=-1;
    for(int i=0;i<parts_.size();++i)if(parts_[i].id==partId){index=i;break;}
    if(index<0){setError(error,"Object not found.");return false;}
    const Part& current=parts_[index];
    if(!current.hasParametricDefinition){setError(error,"The selected body has no editable parametric feature history.");return false;}
    if(current.geometryToken!=current.parametricGeometryToken){setError(error,"This body has geometry edits after its parametric feature history. The old parameters are read-only; rebuild a new parametric feature before editing dimensions.");return false;}
    if(values.isEmpty()&&!allowEmpty){setError(error,"Provide at least one named parameter value to edit.");return false;}
    QMap<QString,double> updated=current.namedParameters;
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
    // Valid no-op edits must not invoke OpenCASCADE or disturb undo/redo state.
    if(current.namedParameters==updated)return true;
    try{
        TopoDS_Shape rebuilt;
        if(!rebuildParametricSolid(current,updated,current.suppressedParametricFeatures,&rebuilt,error))return false;
        checkpoint();
        parts_[index].shape=rebuilt;parts_[index].geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        parts_[index].parametricGeometryToken=parts_[index].geometryToken;
        parts_[index].namedParameters=std::move(updated);return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Parametric feature rebuild failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

QMap<QString,double> SceneDocument::namedParameters(const QString& partId) const {
    const Part* part=find(partId);return part&&part->hasParametricDefinition?part->namedParameters:QMap<QString,double>{};
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
        checkpoint();Part& updated=parts_[index];updated.shape=rebuilt;
        updated.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        updated.parametricGeometryToken=updated.geometryToken;
        updated.suppressedParametricFeatures=std::move(proposed);
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Feature suppression rebuild failed: %1").arg(e.what()));}
    catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
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
    const Part* a = find(firstId);
    const Part* b = find(secondId);
    if (!a || !b) { setError(error, "Both solids must exist."); return false; }
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
    if(rejectSurfaceOnlyInput(*part,QStringLiteral("Fillet"),error))return false;
    try{
        BRepFilletAPI_MakeFillet operation(part->shape);
        int edgeCount=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next()){
            operation.Add(radius,TopoDS::Edge(explorer.Current()));++edgeCount;
        }
        if(edgeCount==0){setError(error,"The body has no edges to fillet.");return false;}
        operation.Build();
        if(!operation.IsDone()){setError(error,"The selected radius cannot be applied to every body edge.");return false;}
        const TopoDS_Shape result=operation.Shape();if(!validShape(result,error))return false;
        checkpoint();for(Part& target:parts_)if(target.id==id){target.shape=result;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
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
    if (part->geometryToken != command.edge.geometryToken) {
        setError(error, "The fillet preview is stale because the body geometry changed. Select the edge again.");
        return false;
    }
    if (!validShape(preview, error)) return false;
    checkpoint();
    for (auto& target : parts_) if (target.id == command.edge.partId) {
        target.shape = preview;
        target.geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
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
    if(rejectSurfaceOnlyInput(*part,QStringLiteral("Chamfer"),error))return false;
    try{
        BRepFilletAPI_MakeChamfer operation(part->shape);
        int edgeCount=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next()){
            operation.Add(distance,TopoDS::Edge(explorer.Current()));++edgeCount;
        }
        if(edgeCount==0){setError(error,"The body has no edges to chamfer.");return false;}
        operation.Build();
        if(!operation.IsDone()){setError(error,"The selected distance cannot be applied to every body edge.");return false;}
        const TopoDS_Shape result=operation.Shape();if(!validShape(result,error))return false;
        checkpoint();for(Part& target:parts_)if(target.id==id){target.shape=result;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
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
    if (part->geometryToken != command.edge.geometryToken) {
        setError(error, "The chamfer preview is stale because the body geometry changed. Select the edge again.");
        return false;
    }
    if (!validShape(preview, error)) return false;
    checkpoint();
    for (auto& target : parts_) if (target.id == command.edge.partId) {
        target.shape = preview;
        target.geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
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
    command->sketchPlane = SketchPlane::XY;
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
        if(command.sketchPlane==SketchPlane::Custom){
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
            (command.sketchPlane!=SketchPlane::XY&&command.sketchPlane!=SketchPlane::XZ&&
             command.sketchPlane!=SketchPlane::YZ&&command.sketchPlane!=SketchPlane::Custom)) {
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
            if(command.sketchPlane==SketchPlane::Custom){
                low=std::numeric_limits<double>::infinity();high=-low;start=command.planeOffset;worldDirection=1;
                const gp_Pnt origin(command.customOriginX,command.customOriginY,command.customOriginZ);
                for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z){
                    const gp_Pnt corner(x?xmax:xmin,y?ymax:ymin,z?zmax:zmin);
                    const double projection=gp_Vec(origin,corner).Dot(gp_Vec(customNormal));
                    low=std::min(low,projection);high=std::max(high,projection);
                }
            }else if(command.sketchPlane==SketchPlane::XY){low=zmin;high=zmax;start=command.planeOffset;worldDirection=1;}
            else if(command.sketchPlane==SketchPlane::XZ){low=ymin;high=ymax;start=-command.planeOffset;worldDirection=-1;}
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
        if(command.sketchPlane==SketchPlane::Custom){
            const gp_Pnt frameOrigin(command.customOriginX,command.customOriginY,command.customOriginZ);
            gp_Trsf frameTransform;
            frameTransform.SetValues(customU.X(),customV.X(),customNormal.X(),frameOrigin.X(),
                                     customU.Y(),customV.Y(),customNormal.Y(),frameOrigin.Y(),
                                     customU.Z(),customV.Z(),customNormal.Z(),frameOrigin.Z());
            tool=BRepBuilderAPI_Transform(tool,frameTransform,true).Shape();
        }else if(command.sketchPlane!=SketchPlane::XY){
            gp_Trsf planeTransform;
            if(command.sketchPlane==SketchPlane::XZ)
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
    command->diameter=diameter;
    command->depth=depth;
    command->throughAll=throughAll;
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
        double cutDepth=command.depth;
        double margin=0.01;
        if (command.throughAll) {
            Bnd_Box bounds;
            BRepBndLib::Add(command.sourceShape,bounds);
            if (bounds.IsVoid()) { preview.error="The selected body has no measurable bounds."; return preview; }
            double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
            bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
            margin=std::max(0.01,(zmax-zmin)*1e-4);
            cutDepth=command.entryZ-zmin+2.0*margin;
            if (!finitePositive(cutDepth)) { preview.error="The hole entry point is below the selected body's lower bound."; return preview; }
        }
        const double startZ=command.entryZ+margin;
        const double toolLength=cutDepth+margin;
        if (!finiteOffset(startZ) || !finitePositive(toolLength)) { preview.error="Hole cutter dimensions are out of range."; return preview; }
        const gp_Ax2 axis(gp_Pnt(command.x,command.y,startZ),gp_Dir(0,0,-1));
        const TopoDS_Shape cutter=BRepPrimAPI_MakeCylinder(axis,command.diameter/2.0,toolLength).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);
        operation.Build();
        if (!operation.IsDone()) { preview.error="The kernel could not cut the requested hole."; return preview; }
        preview.shape=operation.Shape();
        if(command.style==HoleStyle::Counterbore){
            const TopoDS_Shape headCutter=BRepPrimAPI_MakeCylinder(axis,command.headDiameter/2.0,command.headDepth+margin).Shape();
            BRepAlgoAPI_Cut headCut(preview.shape,headCutter);headCut.Build();
            if(!headCut.IsDone()){preview.shape=TopoDS_Shape();preview.error="The counterbore recess could not be cut.";return preview;}
            preview.shape=headCut.Shape();
        } else if(command.style==HoleStyle::Countersink){
            const double sinkDepth=(command.headDiameter-command.diameter)/(2.0*std::tan(command.sinkAngleDegrees*std::numbers::pi/360.0));
            if(!finitePositive(sinkDepth)){preview.shape=TopoDS_Shape();preview.error="The countersink depth is invalid.";return preview;}
            const double drillRadius=command.diameter/2.0;
            const double radialSlope=(command.headDiameter/2.0-drillRadius)/sinkDepth;
            const TopoDS_Shape sinkCutter=BRepPrimAPI_MakeCone(axis,command.headDiameter/2.0+radialSlope*margin,
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
    if (!validShape(preview,error)) return false;
    checkpoint();
    for (auto& target:parts_) if (target.id==command.partId) {
        target.shape=preview;
        target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
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

bool SceneDocument::commitDovetailGroove(const DovetailGrooveCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The dovetail-groove preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;
        target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;
        target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;
    }
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;
    }
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;
    }
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
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
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
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

bool SceneDocument::setVisibility(const QString& id, bool visible, QString* error) {
    for (const auto& part : parts_) if (part.id == id) {
        if (part.visible == visible) return true;
        checkpoint();
        for (auto& item : parts_) if (item.id == id) { item.visible = visible; break; }
        return true;
    }
    setError(error, "Object not found."); return false;
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
    for(const Part& part:parts_)if(!requested.contains(part.id)&&requested.contains(part.assemblyJointParentBodyId)){
        setError(error,"A selected occurrence is the parent of a joint. Include its jointed descendants in the delete selection.");return false;
    }
    checkpoint();
    for (int i = parts_.size()-1; i >= 0; --i) if (requested.contains(parts_[i].id)) parts_.removeAt(i);
    return true;
}

void SceneDocument::clear() { if (!parts_.isEmpty()) { checkpoint(); parts_.clear(); } }
void SceneDocument::newDocument() {
    parts_.clear(); undo_.clear(); redo_.clear(); path_.clear();
    drawingReadinessPolicy_={};
    revision_ = 0; savedRevision_ = 0; nextRevision_ = 1;
}
bool SceneDocument::undo() {
    if (undo_.isEmpty()) return false;
    QVector<Part> snapshotParts = parts_;
    for (auto& part : snapshotParts) part.shape = BRepBuilderAPI_Copy(part.shape).Shape();
    redo_.append({std::move(snapshotParts),drawingReadinessPolicy_,revision_});
    const Snapshot previous = undo_.takeLast();
    parts_ = previous.parts; drawingReadinessPolicy_=previous.drawingReadinessPolicy; revision_ = previous.revision;
    return true;
}
bool SceneDocument::redo() {
    if (redo_.isEmpty()) return false;
    QVector<Part> snapshotParts = parts_;
    for (auto& part : snapshotParts) part.shape = BRepBuilderAPI_Copy(part.shape).Shape();
    undo_.append({std::move(snapshotParts),drawingReadinessPolicy_,revision_});
    if (undo_.size() > kHistoryDepth) undo_.removeFirst();
    const Snapshot next = redo_.takeLast();
    parts_ = next.parts; drawingReadinessPolicy_=next.drawingReadinessPolicy; revision_ = next.revision;
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
                    annotation.insert("view",dimension.view);annotation.insert("offset_mm",dimension.offsetMm);edgeDimensions.append(annotation);
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
        root.insert("schema",kSchema22);
        root.insert("units","mm");
        QJsonObject readinessPolicy;
        QJsonArray requiredFields;for(const QString& field:drawingReadinessPolicy_.requiredTitleBlockFields)requiredFields.append(field);
        QJsonArray requiredConventions;for(const QString& convention:drawingReadinessPolicy_.requiredAnnotationConventions)requiredConventions.append(convention);
        readinessPolicy.insert("required_title_block_fields",requiredFields);
        readinessPolicy.insert("required_annotation_conventions",requiredConventions);
        root.insert("drawing_readiness_policy",readinessPolicy);
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
    if ((schema != kSchema1 && schema != kSchema2 && schema != kSchema3 && schema != kSchema4 && schema != kSchema5 && schema != kSchema6 && schema != kSchema7 && schema != kSchema8 && schema != kSchema9 && schema != kSchema10 && schema != kSchema11 && schema != kSchema12 && schema != kSchema13 && schema != kSchema14 && schema != kSchema15 && schema != kSchema16 && schema != kSchema17 && schema != kSchema18 && schema != kSchema19 && schema != kSchema20 && (schema != kSchema21 && schema != kSchema22)) || root.value("units").toString() != "mm" || !root.value("parts").isArray()) {
        setError(error,"Unsupported project schema or units."); return false;
    }
    DrawingReadinessPolicy candidateReadinessPolicy;
    if(schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))){
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
            if (schema == kSchema2 || schema == kSchema3 || schema == kSchema4 || schema == kSchema5 || schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))) {
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
                if (schema == kSchema3 || schema == kSchema4 || schema == kSchema5 || schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))) {
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
                    if (schema == kSchema4 || schema == kSchema5 || schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))) {
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
                    if (schema == kSchema5 || schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))) {
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
                    if (schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))) {
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
            candidate.last().materialName=materialName;
            if((schema==kSchema8||schema==kSchema9||schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22)))&&item.contains("geometry_token")){
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
            if (schema == kSchema6 || schema == kSchema7 || schema == kSchema8 || schema == kSchema9 || schema == kSchema10 || schema == kSchema11 || schema == kSchema12 || schema == kSchema13 || schema == kSchema14 || schema == kSchema15 || (schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))) {
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
                    if((schema!=kSchema19&&(schema!=kSchema20&&(schema!=kSchema21&&schema!=kSchema22)))||!yawValue.isDouble()){
                        setError(error,"Invalid saved occurrence yaw record.");return false;
                    }
                    loaded.placementYawDegrees=yawValue.toDouble();
                }
                if(!pitchValue.isUndefined()||!rollValue.isUndefined()){
                    if(schema!=kSchema22||(!pitchValue.isUndefined()&&!pitchValue.isDouble())||
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
            if(schema==kSchema7||schema==kSchema8||schema==kSchema9||schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))){
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
                    if(schema==kSchema9||schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))){
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
                        if(schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))){
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
                        if(schema==kSchema22){
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
                                settings.edgeDimensionAnnotations.append({dimension.value("id").toString(),dimension.value("source_body_id").toString(),
                                    dimension.value("source_geometry_token").toString(),static_cast<int>(edgeIndex),dimension.value("view").toString(),
                                    dimension.value("offset_mm").toDouble()});
                            }
                        }
                        if(schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))){
                            const QJsonValue rawPartsList=drawing.value("linked_parts_list");
                            if(!rawPartsList.isObject()){setError(error,"Invalid linked drawing parts-list settings.");return false;}
                            const QJsonObject table=rawPartsList.toObject();
                            if(!table.value("id").isString()||!table.value("visible").isBool()||!table.value("x_mm").isDouble()||!table.value("y_mm").isDouble()){
                                setError(error,"Invalid linked drawing parts-list fields.");return false;
                            }
                            settings.partsListAnnotation={table.value("id").toString(),table.value("visible").toBool(),table.value("x_mm").toDouble(),table.value("y_mm").toDouble()};
                        }
                        if(schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))){
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
                        if(schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))){
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
                        if(schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))){
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
            if(schema==kSchema8||schema==kSchema9||schema==kSchema10||schema==kSchema11||schema==kSchema12||schema==kSchema13||schema==kSchema14||schema==kSchema15||(schema == kSchema16 || schema == kSchema17 || schema == kSchema18 || schema == kSchema19 || schema == kSchema20 || (schema == kSchema21 || schema == kSchema22))){
                const QJsonValue raw=item.value("parametric_definition");
                if(raw.isObject()){
                    const QJsonObject definition=raw.toObject();
                    if(definition.value("type").toString()==QStringLiteral("constrained-polygon-extrusion-v1")){
                        const QJsonArray boundary=definition.value("boundary").toArray();
                        if((schema!=kSchema21&&schema!=kSchema22)||definition.size()!=5||!definition.value("sketch").isObject()||
                           !definition.value("boundary").isArray()||boundary.size()<3||boundary.size()>64||
                           !definition.value("depth_mm").isDouble()||!finitePositive(definition.value("depth_mm").toDouble())||
                           !definition.value("geometry_token").isString()||
                           definition.value("geometry_token").toString().isEmpty()||
                           definition.value("geometry_token").toString().size()>64){
                            setError(error,"Invalid saved constrained polygon definition.");return false;
                        }
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
                        if(!rebuilt.addConstrainedPolygonExtrusion(parsedSketch,boundaryIds,depth,error))return false;
                        const QString token=definition.value("geometry_token").toString();
                        if(candidate.last().geometryToken==token){
                            GProp_GProps storedMass,rebuiltMass;
                            BRepGProp::VolumeProperties(candidate.last().shape,storedMass);
                            BRepGProp::VolumeProperties(rebuilt.parts().first().shape,rebuiltMass);
                            Bnd_Box storedBox,rebuiltBox;
                            BRepBndLib::AddOptimal(candidate.last().shape,storedBox,false,false);
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
                    if(hasHolePattern&&(schema==kSchema19||schema==kSchema20||(schema==kSchema21||schema==kSchema22))){
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
                        ((schema==kSchema21||schema==kSchema22)&&(type==QStringLiteral("rectangle-extrusion-blind-hole-v1")||
                         type==QStringLiteral("rectangle-extrusion-offset-blind-hole-v1")))||
                        (schema==kSchema22&&blindPattern);
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
                        if(schema!=kSchema19&&(schema!=kSchema20&&(schema!=kSchema21&&schema!=kSchema22))){setError(error,"Unexpected parameter dependency graph in older scene schema.");return false;}
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
                               (rule.contains("offset")&&(schema!=kSchema20&&(schema!=kSchema21&&schema!=kSchema22)))||
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
                        if((schema!=kSchema19&&(schema!=kSchema20&&(schema!=kSchema21&&schema!=kSchema22)))||!definition.value("suppressed_features").isArray()){
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
                        if(x-r<=originX||y-r<=originY||lastX+r>=originX+parsedParameters.value("width")||lastY+r>=originY+parsedParameters.value("height")||
                           (hasHolePattern&&spacing<parsedParameters.value("hole_diameter"))){
                            setError(error,"Saved parametric through-hole is outside its rectangular sketch profile.");return false;
                        }
                    }
                    candidate.last().hasParametricDefinition=true;candidate.last().namedParameters=std::move(parsedParameters);
                    candidate.last().parameterLinks=std::move(parsedLinks);
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
            const QJsonValue rawFolded=item.value("folded_sheet_definition");
            if(rawFolded.isObject()){
                if(schema!=kSchema19&&(schema!=kSchema20&&(schema!=kSchema21&&schema!=kSchema22))){setError(error,"Unexpected folded sheet-metal definition in an older scene schema.");return false;}
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
            ids.insert(id); names.insert(name.toLower());
        }
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("Invalid B-rep: %1").arg(e.what())); return false; }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); return false; }
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
    // Transactional load: no part of the original document is touched until all validation succeeds.
    parts_ = std::move(candidate);
    drawingReadinessPolicy_=candidateReadinessPolicy;
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


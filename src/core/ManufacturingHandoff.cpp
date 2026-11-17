#include "ManufacturingHandoff.h"
#include "SceneDocument.h"
#include "DocumentAudit.h"
#include "HoleTable.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QUuid>
#include <QVector>

namespace cad {
namespace {
struct FileEntry {
    QString name;
    QString digest;
    qint64 bytes=0;
};

QString safeCsv(QString field){
    // Spreadsheet CSV injection is a real handoff security risk. This value
    // is never interpreted as an expression by conforming spreadsheet tools.
    const QString trimmed=field.trimmed();
    if(!trimmed.isEmpty()&&QStringLiteral("=+-@\t\r\n").contains(trimmed[0]))
        field.prepend(QChar(0x27));
    field.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+field+QStringLiteral("\"");
}

bool writeBytes(const QString& path,const QByteArray& contents,QString* issue){
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)){
        if(issue)*issue=QStringLiteral("Cannot open %1: %2").arg(path,file.errorString());
        return false;
    }
    if(file.write(contents)!=contents.size()||!file.commit()){
        if(issue)*issue=QStringLiteral("Failed to commit %1: %2").arg(path,file.errorString());
        file.cancelWriting();return false;
    }
    return true;
}

bool digestFile(const QString& path,FileEntry* entry,QString* issue){
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.isSequential()){
        if(issue)*issue=QStringLiteral("Cannot hash handoff file %1").arg(path);
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total=0;
    while(!file.atEnd()){
        const QByteArray chunk=file.read(128*1024);
        if(chunk.isEmpty()){
            if(issue)*issue=QStringLiteral("Cannot finish reading handoff file %1").arg(path);
            return false;
        }
        total+=chunk.size();hash.addData(chunk);
    }
    if(total<=0){if(issue)*issue=QStringLiteral("Empty handoff file: %1").arg(path);return false;}
    if(entry){entry->name=QFileInfo(path).fileName();entry->digest=QString::fromLatin1(hash.result().toHex());entry->bytes=total;}
    return true;
}

bool stageFile(QDir staging,const QString& name,QVector<FileEntry>* inventory,QString* issue){
    FileEntry entry;
    if(!digestFile(staging.filePath(name),&entry,issue))return false;
    inventory->append(entry);
    return true;
}
}

bool createManufacturingHandoff(const SceneDocument& document,
        const QString& destinationParent,ManufacturingHandoffResult* result,
        QString* error){
    const auto fail=[&](const QString& message){if(error)*error=message;return false;};
    if(!result)return fail("A handoff result pointer is required.");
    if(document.path().isEmpty()||document.dirty())
        return fail("Save the current CAD document before exporting a traceable handoff.");
    if(document.parts().isEmpty())return fail("The scene has no parts to export.");
    const QFileInfo source(document.path());
    if(!source.isFile()||source.size()<=0)return fail("Saved project file is missing or empty.");
    if(!QDir(destinationParent).exists())return fail("Select an existing output parent folder.");
    const DocumentAuditReport audit=runDocumentAudit(document);
    if(audit.errorCount>0)return fail(QStringLiteral(
        "The document audit reports %1 ERROR finding(s). Fix those before creating a handoff.")
        .arg(audit.errorCount));
    QString issue;
    const QVector<BillOfMaterialsRow> bom=document.billOfMaterials(&issue);
    if(bom.isEmpty())return fail(issue.isEmpty()?QStringLiteral("The bill of materials is empty."):issue);
    qint64 bomQuantity=0;
    for(const BillOfMaterialsRow& row:bom){
        if(row.quantity<=0)return fail("BOM contains a non-positive component quantity.");
        bomQuantity+=row.quantity;
    }
    if(bomQuantity!=document.parts().size())
        return fail("BOM quantity does not match the saved occurrence count; correct the model data.");
    const QString packageName=QStringLiteral("CADWorkbench_Engineering_Handoff_%1_%2")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzzZ"),
             QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    const QString transient=QStringLiteral(".%1_STAGING").arg(packageName);
    QDir parent(destinationParent);
    if(!parent.mkdir(transient))return fail("Cannot create a private staging folder.");
    QDir staged(parent.filePath(transient));
    bool committed=false;
    struct StageCleanup {QDir directory; bool* committed; ~StageCleanup(){if(!*committed)directory.removeRecursively();}} cleanup{staged,&committed};
    QVector<FileEntry> entries;
    // Copy the exact saved input bytes, not an uncommitted in-memory snapshot.
    const QString sourceSuffix=source.suffix().toLower();
    if(!QRegularExpression(QStringLiteral("^[a-z0-9]{1,16}$")).match(sourceSuffix).hasMatch())
        return fail("Saved project file must have a short ASCII extension for portable handoff.");
    const QString sourceName=QStringLiteral("source_design.")+sourceSuffix;
    if(!QFile::copy(source.absoluteFilePath(),staged.filePath(sourceName))||
       !stageFile(staged,sourceName,&entries,&issue))
        return fail(issue.isEmpty()?QStringLiteral("Could not copy original project."):issue);
    if(!document.exportSTEP(staged.filePath("assembly.step"),&issue)||
       !stageFile(staged,"assembly.step",&entries,&issue))
        return fail(issue.isEmpty()?QStringLiteral("STEP export failed."):issue);
    if(!document.exportBillOfMaterialsCsv(staged.filePath("bill_of_materials.csv"),&issue)||
       !stageFile(staged,"bill_of_materials.csv",&entries,&issue))
        return fail(issue.isEmpty()?QStringLiteral("BOM export failed."):issue);
    if(!document.exportPartsListCsv(staged.filePath("occurrence_parts.csv"),&issue)||
       !stageFile(staged,"occurrence_parts.csv",&entries,&issue))
        return fail(issue.isEmpty()?QStringLiteral("Occurrence export failed."):issue);
    if(!document.exportBodyScheduleCsv(staged.filePath("body_schedule.csv"),&issue)||
       !stageFile(staged,"body_schedule.csv",&entries,&issue))
        return fail(issue.isEmpty()?QStringLiteral("Body schedule export failed."):issue);
    if(!writeBytes(staged.filePath("document_audit.csv"),audit.toCsv(),&issue)||
       !stageFile(staged,"document_audit.csv",&entries,&issue))return fail(issue);

    QStringList procurementRows{"item_number,part_number,revision,procurement_type,quantity,exception_code,next_action"};
    int exceptionCount=0;
    auto addException=[&](const BillOfMaterialsRow& row,const QString& code,const QString& action){
        procurementRows<<QStringList{QString::number(row.itemNumber),safeCsv(row.partNumber),
            safeCsv(row.revision),safeCsv(row.procurementType),QString::number(row.quantity),
            safeCsv(code),safeCsv(action)}.join(',');
        ++exceptionCount;
    };
    for(const BillOfMaterialsRow& row:bom){
        if(row.partNumber.trimmed().isEmpty())addException(row,"PART_NUMBER_MISSING","Assign a stable part number");
        if(row.revision.trimmed().isEmpty())addException(row,"REVISION_MISSING","Assign a controlled revision");
        if(row.procurementType.compare("Purchased",Qt::CaseInsensitive)==0){
            if(row.supplier.trimmed().isEmpty())addException(row,"SUPPLIER_MISSING","Identify supplier before ordering");
            if(row.supplierPartNumber.trimmed().isEmpty())addException(row,"CATALOG_NUMBER_MISSING","Identify supplier stock code");
        }else if(row.materialName.trimmed().isEmpty()){
            addException(row,"MATERIAL_MISSING","Confirm engineering material and stock");
        }
        if(row.unitMassKg<=0.0)addException(row,"MASS_UNKNOWN","Review mass and material density");
    }
    if(!writeBytes(staged.filePath("procurement_exceptions.csv"),
        (procurementRows.join("\r\n")+"\r\n").toUtf8(),&issue)||
       !stageFile(staged,"procurement_exceptions.csv",&entries,&issue))return fail(issue);

    QStringList inspectionRows{"body_id,body_name,feature_id,category,review_status,requirement"};
    int inspectionCount=0;
    for(const Part& part:document.parts()){
        for(const FeatureInspectionNote& note:part.featureInspectionNotes){
            const QString status=note.reviewedGeometryToken==part.geometryToken?
                QStringLiteral("SOURCE_GEOMETRY_CURRENT"):QStringLiteral("REVIEW_REQUIRED");
            inspectionRows<<QStringList{safeCsv(part.id),safeCsv(part.name),safeCsv(note.featureId),
                safeCsv(note.category),safeCsv(status),safeCsv(note.text)}.join(',');
            ++inspectionCount;
        }
    }
    if(!writeBytes(staged.filePath("feature_inspection_requirements.csv"),
        (inspectionRows.join("\r\n")+"\r\n").toUtf8(),&issue)||
       !stageFile(staged,"feature_inspection_requirements.csv",&entries,&issue))return fail(issue);

    // Revision-linked machining review is a separate conservative geometry
    // inspection, NOT CAM output. No excluded cylinder or failed inspection
    // may silently be counted as a verified hole or a machining approval.
    QStringList coverageRows{
        "body_id,geometry_token,status,hole_count,excluded_cylinders,reason"};
    QStringList holeRows{
        "body_id,geometry_token,hole_number,x_mm,y_mm,diameter_mm,z_min_mm,z_max_mm,through_envelope"};
    // The template is derived from the same native hole record as the signed
    // nominal table. Do not re-parse CSV: quoted/escaped body IDs may include
    // commas, quotes or newlines.
    QStringList measurementTemplate{
        "body_id,geometry_token,hole_number,nominal_x_mm,nominal_y_mm,nominal_diameter_mm,measured_datum_x_mm,measured_datum_y_mm,measured_projected_x_mm,measured_projected_y_mm,projected_height_mm,actual_diameter_mm,minimum_diameter_mm,maximum_diameter_mm,diametral_position_tolerance_mm,material_condition,maximum_pin_diameter_mm,inspection_status"};
    int inspectedHoleCount=0;
    int machiningReviewCount=0;
    for(const Part& part:document.parts()){
        QString status;
        QString reason;
        int count=0;
        int excluded=0;
        if(part.isSurfaceBody){
            status="SURFACE_NOT_APPLICABLE";
            reason="Surface-only body has no verified solid drilling envelope";
        }else if(part.geometryToken.trimmed().isEmpty()){
            status="NOT_EVALUATED";
            reason="Missing stable source geometry revision token";
            ++machiningReviewCount;
        }else{
            AxialHoleAudit holes;
            QString inspectionIssue;
            if(!inspectAxialHoles(part,&holes,&inspectionIssue)){
                status="NOT_EVALUATED";
                reason=inspectionIssue;
                ++machiningReviewCount;
            }else{
                count=holes.holes.size();
                excluded=holes.excludedCylinders;
                status=excluded>0?"PARTIAL_CYLINDERS_EXCLUDED":
                    count>0?"INSPECTED_AXIAL_ONLY":"INSPECTED_NO_AXIAL_HOLES";
                if(excluded>0){
                    reason="Excluded cylindrical faces require separate review";
                    ++machiningReviewCount;
                }else{
                    reason="Only complete Z-axis internal cylindrical bores were inspected";
                }
                for(const AxialHoleRecord& hole:holes.holes){
                    const auto number=[](double value){return QString::number(value,'g',17);};
                    holeRows<<QStringList{safeCsv(part.id),safeCsv(part.geometryToken),
                        QString::number(hole.number),number(hole.xMm),number(hole.yMm),
                        number(hole.diameterMm),number(hole.zMinMm),number(hole.zMaxMm),
                        hole.throughEnvelope?"YES":"NO"}.join(',');
                    measurementTemplate<<QStringList{safeCsv(part.id),safeCsv(part.geometryToken),
                        QString::number(hole.number),number(hole.xMm),number(hole.yMm),
                        number(hole.diameterMm),QString(),QString(),QString(),QString(),
                        QString(),QString(),QString(),QString(),QString(),QString(),
                        QString(),"UNMEASURED"}.join(',');
                    ++inspectedHoleCount;
                }
            }
        }
        coverageRows<<QStringList{safeCsv(part.id),safeCsv(part.geometryToken),
            status,QString::number(count),QString::number(excluded),safeCsv(reason)}.join(',');
    }
    // The signed handoff contains only blank independent-metrology fields.
    // Copy it outside the signed snapshot before entering inspection data.
    if(!writeBytes(staged.filePath("hole_position_inspection_TEMPLATE.csv"),
        (measurementTemplate.join("\r\n")+"\r\n").toUtf8(),&issue)||
       !stageFile(staged,"hole_position_inspection_TEMPLATE.csv",&entries,&issue))
        return fail(issue);

    if(!writeBytes(staged.filePath("machining_coverage.csv"),
        (coverageRows.join("\r\n")+"\r\n").toUtf8(),&issue)||
       !stageFile(staged,"machining_coverage.csv",&entries,&issue))return fail(issue);
    if(!writeBytes(staged.filePath("axial_hole_locations.csv"),
        (holeRows.join("\r\n")+"\r\n").toUtf8(),&issue)||
       !stageFile(staged,"axial_hole_locations.csv",&entries,&issue))return fail(issue);

    const QByteArray readme=QStringLiteral(
        "CADWorkbench ENGINEERING HANDOFF — UNISSUED, REVIEW REQUIRED\n"
        "This is NOT an approved manufacturing release, drawing revision, or\n"
        "ISO/ASME certification. Do not manufacture from it without review.\n\n"
        "The original saved model, STEP assembly, BOM, occurrence schedule,\n"
        "audit, procurement exceptions and inspection requirements were\n"
        "exported from one unchanged CAD document snapshot.\n"
        "Warnings: %1; unresolved procurement exceptions: %2.\n"
        "Machining coverage and axial-hole locations are CONSERVATIVE\n"
        "design-review evidence, not drillability proof, setup, or G-code.\n"
        "Excluded faces and uninspected solids require separate review.\n"
        "hole_position_inspection_TEMPLATE.csv lists CAD design locations only.\n"
        "It contains NO measurements or size/position tolerances. Copy it OUT\n"
        "of this signed handoff before entering independent metrology data;\n"
        "tampering with a signed handoff invalidates its checksums.\n"
        "Use verify_manufacturing_handoff.py on this folder to check integrity.\n"
        "SHA-256 hashes verify accidental alteration, NOT author identity.\n"
        "No engineering sign-off is stored in this package.\n")
        .arg(audit.warningCount).arg(exceptionCount).toUtf8();
    if(!writeBytes(staged.filePath("READ_ME_BEFORE_FABRICATION.txt"),readme,&issue)||
       !stageFile(staged,"READ_ME_BEFORE_FABRICATION.txt",&entries,&issue))return fail(issue);

    QJsonArray files;
    for(const FileEntry& entry:entries){
        QJsonObject item;item.insert("path",entry.name);item.insert("bytes",entry.bytes);
        item.insert("sha256",entry.digest);files.append(item);
    }
    QJsonObject manifest;
    manifest.insert("schema_version",2);
    manifest.insert("application","CADWorkbench");
    manifest.insert("status","UNISSUED_REVIEW_REQUIRED");
    manifest.insert("generated_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    manifest.insert("body_count",audit.bodyCount);
    manifest.insert("bom_lines",bom.size());
    manifest.insert("audit_errors",audit.errorCount);
    manifest.insert("audit_warnings",audit.warningCount);
    manifest.insert("procurement_exceptions",exceptionCount);
    manifest.insert("inspection_requirements",inspectionCount);
    manifest.insert("machining_holes",inspectedHoleCount);
    manifest.insert("machining_review_required",machiningReviewCount);
    manifest.insert("files",files);
    if(!writeBytes(staged.filePath("handoff_manifest.json"),
        QJsonDocument(manifest).toJson(QJsonDocument::Indented),&issue))return fail(issue);
    // Fail closed if another process modified the saved design during export.
    // Otherwise the packaged native model could differ from its STEP/BOM.
    FileEntry finalSavedSource;
    if(!digestFile(source.absoluteFilePath(),&finalSavedSource,&issue))
        return fail(issue.isEmpty()?QStringLiteral("Saved CAD source cannot be rechecked."):issue);
    if(finalSavedSource.digest!=entries.first().digest||
       finalSavedSource.bytes!=entries.first().bytes)
        return fail("Saved CAD file changed during export. Save and export the new revision again.");
    // Verify the staged content against the *same* records before committing.
    for(const FileEntry& expected:entries){
        FileEntry observed;
        if(!digestFile(staged.filePath(expected.name),&observed,&issue)||
           observed.digest!=expected.digest||observed.bytes!=expected.bytes)
            return fail(issue.isEmpty()?QStringLiteral("Handoff file changed during staging."):issue);
    }
    if(!parent.rename(transient,packageName))return fail("Failed to atomically publish the complete package directory.");
    committed=true;
    ManufacturingHandoffResult ready;
    ready.folder=parent.filePath(packageName);ready.bodyCount=audit.bodyCount;
    ready.bomLines=bom.size();ready.warningCount=audit.warningCount;
    ready.procurementExceptions=exceptionCount;ready.inspectionRequirements=inspectionCount;
    ready.axialHolesReviewed=inspectedHoleCount;
    ready.machiningBodiesRequiringReview=machiningReviewCount;
    *result=ready;
    if(error)error->clear();
    return true;
}
} // namespace cad

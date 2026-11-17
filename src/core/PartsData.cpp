#include "SceneDocument.h"

#include <QFile>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <limits>

namespace cad {
namespace {
QString csvField(QString value) {
    // CSV quotation alone does not prevent spreadsheet formula execution.
    const QString trimmed=value.trimmed();
    if(!trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front()))
        value.prepend(QChar(0x27));
    value.replace('"', "\"\"");
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}
QString csvNumber(double value) { return QString::number(value, 'g', 15); }
// BOM identity must distinguish every representable density, even if CSV rounds it.
QString identityNumber(double value) {
    return QString::number(value, 'g', std::numeric_limits<double>::max_digits10);
}
// Length-prefix each field so embedded separators cannot alias different BOM records.
QString bomGroupKey(const QStringList& fields) {
    QString key;
    for (const QString& field : fields) {
        key += QString::number(field.size()) + QLatin1Char(':') + field;
    }
    return key;
}
QString geometryKey(const BodyDiagnostics& g) {
    // Rigidly rotated occurrences can differ by a few floating-point ulps in
    // OCCT mass properties. Compare geometry at a tight relative precision so
    // those placements still group, while meaningful size/topology changes do not.
    const QString volume=QString::number(g.volume,'g',10);
    const QString area=QString::number(g.surfaceArea,'g',10);
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(volume, area)
        .arg(g.solids).arg(g.shells).arg(g.faces).arg(g.edges).arg(g.vertices);
}
// BOM metadata is user-provided and may contain '|', ':' or digits. Encode
// field lengths instead of joining values with an ambiguous delimiter.
QString bomGroupKey(const QString& kind,const QStringList& fields) {
    QString key=kind+QLatin1Char(':');
    for(const QString& field:fields) {
        key+=QString::number(field.size());
        key+=QLatin1Char(':');
        key+=field;
    }
    return key;
}
bool writeCsvFile(const QString& path, const QByteArray& bytes, QString* error) {
    if (path.trimmed().isEmpty()) { if (error) *error = "Provide a CSV output path."; return false; }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = QStringLiteral("Could not create CSV file: %1").arg(file.errorString()); return false; }
    if (file.write(bytes) != bytes.size()) { if (error) *error = QStringLiteral("Could not write CSV file: %1").arg(file.errorString()); file.cancelWriting(); return false; }
    if (!file.commit()) { if (error) *error = QStringLiteral("Could not finalize CSV file: %1").arg(file.errorString()); return false; }
    return true;
}
QString profileName(StandardsProfile profile) {
    switch (profile) {
        case StandardsProfile::BomData: return QStringLiteral("CADWorkbench BOM data profile v1");
        case StandardsProfile::IsoDrawingReadiness: return QStringLiteral("ISO drawing readiness (ISO 7200 / ISO 129-1 / ISO 22081)");
        case StandardsProfile::AsmeDrawingReadiness: return QStringLiteral("ASME drawing readiness (Y14.100 / Y14.5)");
    }
    return QStringLiteral("CAD data profile");
}
}

bool SceneDocument::setPartMetadata(const QString& id, const QString& partNumber,
                                    const QString& description, const QString& revision, QString* error) {
    const Part* found = find(id);
    if (!found) { if (error) *error = "Object not found."; return false; }
    return setPartMetadata(id, partNumber, description, revision, found->assemblyPath, error);
}

bool SceneDocument::setPartMetadata(const QString& id, const QString& partNumber,
                                    const QString& description, const QString& revision,
                                    const QString& assemblyPath, QString* error) {
    const Part* found = find(id);
    if (!found) { if (error) *error = "Object not found."; return false; }
    return setPartMetadata(id, partNumber, description, revision, assemblyPath,
                           found->procurementType, found->supplier, found->supplierPartNumber, error);
}

bool SceneDocument::setPartMetadata(const QString& id, const QString& partNumber,
                                    const QString& description, const QString& revision,
                                    const QString& assemblyPath, const QString& procurementType,
                                    const QString& supplier, const QString& supplierPartNumber, QString* error) {
    const QString pn = partNumber.trimmed().toUpper();
    const QString desc = description.trimmed();
    const QString rev = revision.trimmed().toUpper();
    const QString typeKey = procurementType.trimmed().toCaseFolded();
    const QString type = typeKey == "purchased" ? QStringLiteral("Purchased") :
                        typeKey == "manufactured" ? QStringLiteral("Manufactured") : QString();
    const QString cleanSupplier = supplier.trimmed();
    const QString cleanSupplierPartNumber = supplierPartNumber.trimmed();
    if (type.isEmpty()) { if (error) *error = "Choose Manufactured or Purchased for the procurement type."; return false; }
    if (cleanSupplier.size() > 80 || cleanSupplierPartNumber.size() > 64 ||
        cleanSupplier.contains('\n') || cleanSupplierPartNumber.contains('\n') ||
        cleanSupplier.contains('\r') || cleanSupplierPartNumber.contains('\r')) {
        if (error) *error = "Supplier names are limited to 80 characters and supplier part numbers to 64; line breaks are not allowed.";
        return false;
    }
    QString cleanPath = assemblyPath.trimmed();
    cleanPath.replace('\\', '/');
    QStringList pathParts;
    for (const QString& segment : cleanPath.split('/', Qt::SkipEmptyParts)) {
        const QString cleanSegment = segment.trimmed();
        if (cleanSegment.isEmpty() || cleanSegment == "." || cleanSegment == ".." || cleanSegment.size() > 80) {
            if (error) *error = "Assembly path segments must be 1-80 characters and cannot be '.' or '..'.";
            return false;
        }
        pathParts.append(cleanSegment);
    }
    cleanPath = pathParts.join('/');
    if (pn.size() > 32 || desc.size() > 200 || rev.size() > 16 ||
        pn.contains('\n') || desc.contains('\n') || rev.contains('\n') ||
        pn.contains('\r') || desc.contains('\r') || rev.contains('\r') || cleanPath.size() > 200 ||
        cleanPath.contains('\n') || cleanPath.contains('\r')) {
        if (error) *error = "Part information or assembly path exceeds its field limit or contains a line break.";
        return false;
    }
    const Part* found = find(id);
    if (!found) { if (error) *error = "Object not found."; return false; }
    if (found->partNumber == pn && found->description == desc && found->revision == rev && found->assemblyPath == cleanPath &&
        found->procurementType == type && found->supplier == cleanSupplier && found->supplierPartNumber == cleanSupplierPartNumber) return true;
    checkpoint();
    for (Part& part : parts_) if (part.id == id) {
        part.partNumber = pn;
        part.description = desc;
        part.revision = rev;
        part.assemblyPath = cleanPath;
        part.procurementType = type;
        part.supplier = cleanSupplier;
        part.supplierPartNumber = cleanSupplierPartNumber;
        break;
    }
    return true;
}

namespace {
// The metadata editor allows slash-delimited groups. This stricter parser is
// used by *batch* hierarchy mutation, preventing ambiguous parent/child sets.
bool canonicalGroupPath(QString input,QString* path,QString* error) {
    input=input.trimmed();
    input.replace(QLatin1Char('\\'),QLatin1Char('/'));
    if(input.isEmpty() || input.size()>200 || input.startsWith('/') ||
       input.endsWith('/') || input.contains("//")) {
        if(error)*error="Assembly group path must be nonempty, at most 200 characters, without empty components.";
        return false;
    }
    const QStringList parts=input.split('/');
    if(parts.size()>32) {
        if(error)*error="Assembly nesting exceeds the 32-level limit.";
        return false;
    }
    for(const QString& segment:parts) {
        if(segment!=segment.trimmed() || segment=="." || segment==".." || segment.size()>80) {
            if(error)*error="Each assembly group name must be trimmed, 1-80 characters, and cannot be '.' or '..'.";
            return false;
        }
        for(const QChar c:segment) if(c.isNull() || c.category()==QChar::Other_Control) {
            if(error)*error="Assembly group names cannot contain control characters.";
            return false;
        }
    }
    if(path)*path=parts.join('/');
    return true;
}
}

QStringList SceneDocument::assemblyGroupPaths() const {
    QSet<QString> all;
    for(const Part& part:parts_) {
        QString prefix;
        for(const QString& item:part.assemblyPath.split('/',Qt::SkipEmptyParts)) {
            prefix=prefix.isEmpty()?item:prefix+'/'+item;
            all.insert(prefix);
        }
    }
    QStringList result=all.values();
    std::sort(result.begin(),result.end(),[](const QString& a,const QString& b){
        const int da=a.count('/'),db=b.count('/');
        return da==db?QString::compare(a,b,Qt::CaseSensitive)<0:da<db;
    });
    return result;
}

bool SceneDocument::moveAssemblySubtree(const QString& oldPath,const QString& newPath,QString* error) {
    QString from,to;
    if(!canonicalGroupPath(oldPath,&from,error) ||
       !canonicalGroupPath(newPath,&to,error))return false;
    const QString oldPrefix=from+QLatin1Char('/');
    const QString newPrefix=to+QLatin1Char('/');
    if(from==to) {
        for(const Part& part:parts_) if(part.assemblyPath==from || part.assemblyPath.startsWith(oldPrefix))
            return true;
        if(error)*error="No body belongs to the specified assembly group.";
        return false;
    }
    if(to.startsWith(oldPrefix,Qt::CaseInsensitive) ||
       from.compare(to,Qt::CaseInsensitive)==0) {
        if(error)*error="A group cannot be moved into itself or a case-ambiguous duplicate.";
        return false;
    }
    QVector<int> movedIndices;
    QStringList replacementPaths;
    for(int i=0;i<parts_.size();++i) {
        const QString& current=parts_[i].assemblyPath;
        if(current==from || current.startsWith(oldPrefix)) {
            const QString replacement=to+current.mid(from.size());
            QString valid;
            if(!canonicalGroupPath(replacement,&valid,error))return false;
            movedIndices.append(i);
            replacementPaths.append(valid);
        }
    }
    if(movedIndices.isEmpty()) {
        if(error)*error="No body belongs to the specified assembly group.";
        return false;
    }
    // A subtree move must not silently absorb an independent existing group.
    for(int i=0;i<parts_.size();++i) {
        if(movedIndices.contains(i))continue;
        const QString& current=parts_[i].assemblyPath;
        if(current.compare(to,Qt::CaseInsensitive)==0 ||
           current.startsWith(newPrefix,Qt::CaseInsensitive)) {
            if(error)*error="Destination already has an independent assembly group; choose a unique group path.";
            return false;
        }
    }
    checkpoint();
    for(int i=0;i<movedIndices.size();++i)
        parts_[movedIndices[i]].assemblyPath=replacementPaths[i];
    return true;
}

QVector<PartsListRow> SceneDocument::partsList(QString* error) const {
    QVector<const Part*> ordered;
    ordered.reserve(parts_.size());
    for (const Part& part : parts_) ordered.append(&part);
    std::stable_sort(ordered.begin(), ordered.end(), [](const Part* a, const Part* b) {
        const int pn = QString::compare(a->partNumber, b->partNumber, Qt::CaseInsensitive);
        if (pn != 0) return pn < 0;
        const int name = QString::compare(a->name, b->name, Qt::CaseInsensitive);
        return name == 0 ? a->id < b->id : name < 0;
    });
    QVector<PartsListRow> rows;
    rows.reserve(ordered.size());
    for (const Part* part : ordered) {
        BodyDiagnostics g;
        QString detail;
        if (!diagnoseBody(part->id, &g, &detail) || !g.valid) {
            if (error) *error = QStringLiteral("Could not calculate parts-list row for '%1': %2").arg(part->name, detail);
            return {};
        }
        double mass = 0.0;
        if (part->materialDensityKgPerM3 > 0.0) {
            DensityMassProperties p;
            if (!measureMassFromDensity(part->id, part->materialDensityKgPerM3, &p, &detail)) {
                if (error) *error = QStringLiteral("Could not calculate mass for '%1': %2").arg(part->name, detail);
                return {};
            }
            mass = p.massKg;
        }
        PartsListRow row{static_cast<int>(rows.size()) + 1, part->id, part->name, part->partNumber, part->description,
                         part->revision, part->materialName, 1, g.volume, mass, part->assemblyPath,
                         part->procurementType, part->supplier, part->supplierPartNumber};
        row.componentDefinitionId=part->componentDefinitionId;row.componentName=part->componentName;row.occurrenceId=part->occurrenceId;
        row.placementX=part->placementX;row.placementY=part->placementY;row.placementZ=part->placementZ;
        row.materialEngineering = part->materialEngineering;
        rows.append(std::move(row));
    }
    return rows;
}

QVector<BillOfMaterialsRow> SceneDocument::billOfMaterials(QString* error) const {
    if(parts_.isEmpty())return {};
    QStringList allBodyIds;allBodyIds.reserve(parts_.size());
    for(const Part& part:parts_)allBodyIds.append(part.id);
    // A whole-document BOM is not constrained by the drawing source selection cap.
    return buildBillOfMaterials(allBodyIds,error);
}

QVector<BillOfMaterialsRow> SceneDocument::billOfMaterials(const QStringList& bodyIds,QString* error) const {
    if(bodyIds.isEmpty()||bodyIds.size()>50){if(error)*error="Choose between one and fifty bodies for a source-scoped BOM.";return {};}
    return buildBillOfMaterials(bodyIds,error);
}

QVector<BillOfMaterialsRow> SceneDocument::buildBillOfMaterials(const QStringList& bodyIds,QString* error) const {
    QSet<QString> requestedIds;
    for(const QString& id:bodyIds){
        if(id.trimmed().isEmpty()||requestedIds.contains(id)||!find(id)){
            if(error)*error="A source-scoped BOM requires distinct body IDs that exist in the current document.";
            return {};
        }
        requestedIds.insert(id);
    }
    QVector<const Part*> ordered;
    ordered.reserve(requestedIds.size());
    for (const Part& part : parts_) if(requestedIds.contains(part.id))ordered.append(&part);
    std::stable_sort(ordered.begin(), ordered.end(), [](const Part* a, const Part* b) {
        const int pn = QString::compare(a->partNumber, b->partNumber, Qt::CaseInsensitive);
        if (pn != 0) return pn < 0;
        const int name = QString::compare(a->name, b->name, Qt::CaseInsensitive);
        return name == 0 ? a->id < b->id : name < 0;
    });
    QVector<BillOfMaterialsRow> rows;
    QHash<QString, int> rowForKey;
    for (const Part* source : ordered) {
        BodyDiagnostics g;
        QString detail;
        if (!diagnoseBody(source->id, &g, &detail) || !g.valid) {
            if (error) *error = QStringLiteral("Could not calculate BOM row for '%1': %2").arg(source->name, detail);
            return {};
        }
        double massKg = 0.0;
        if (source->materialDensityKgPerM3 > 0.0) {
            DensityMassProperties mass;
            if (!measureMassFromDensity(source->id, source->materialDensityKgPerM3, &mass, &detail)) {
                if (error) *error = QStringLiteral("Could not calculate mass for '%1': %2").arg(source->name, detail);
                return {};
            }
            massKg = mass.massKg;
        }
        // Preserve the spelling of fields stored verbatim. The BOM checker treats
        // case-different descriptions, materials, suppliers, and SKUs as distinct.
        const QStringList identityFields = {
            source->assemblyPath.toCaseFolded(), source->partNumber.toCaseFolded(),
            source->description, source->revision.toCaseFolded(),
            source->materialName, identityNumber(source->materialDensityKgPerM3),
            source->materialEngineering.provenance,
            identityNumber(source->materialEngineering.youngsModulusGPa),
            geometryKey(g), source->procurementType.toCaseFolded(),
            source->supplier, source->supplierPartNumber
        };
        QString group;
        if (!source->componentDefinitionId.isEmpty()) {
            QStringList fields = identityFields;
            fields.prepend(source->componentDefinitionId);
            fields.prepend(QStringLiteral("COMP"));
            group = bomGroupKey(fields);
        } else if (source->partNumber.isEmpty()) {
            group = bomGroupKey({QStringLiteral("BODY"), source->id});
        } else {
            QStringList fields = identityFields;
            fields.prepend(QStringLiteral("PN"));
            group = bomGroupKey(fields);
        }
        auto it = rowForKey.constFind(group);
        if (it == rowForKey.cend()) {
            BillOfMaterialsRow row;
            row.itemNumber = rows.size() + 1;
            row.partNumber = source->partNumber;
            row.description = source->description;
            row.revision = source->revision;
            row.materialName = source->materialName;
            row.materialEngineering = source->materialEngineering;
            row.quantity = 1;
            row.unitVolumeMm3 = g.volume;
            row.totalVolumeMm3 = g.volume;
            row.unitMassKg = massKg;
            row.totalMassKg = massKg;
            row.assemblyPath = source->assemblyPath;
            row.procurementType = source->procurementType;
            row.supplier = source->supplier;
            row.supplierPartNumber = source->supplierPartNumber;
            row.componentDefinitionId = source->componentDefinitionId;
            row.componentName = source->componentName;
            row.bodyIds.append(source->id);
            rowForKey.insert(group, rows.size());
            rows.append(row);
        } else {
            BillOfMaterialsRow& row = rows[it.value()];
            ++row.quantity;
            row.totalVolumeMm3 += g.volume;
            row.totalMassKg += massKg;
            row.bodyIds.append(source->id);
        }
    }
    return rows;
}

bool SceneDocument::exportMotionSamplesCsv(const QString& firstId,const QString& secondId,
                                            const QVector<MotionInterferenceSample>& samples,
                                            const QString& path,QString* error) const {
    const Part* first=find(firstId),*second=find(secondId);
    if(firstId==secondId||!first||!second){
        if(error)*error="Select two distinct existing bodies for a motion-sample report.";
        return false;
    }
    if(samples.size()<2||samples.size()>256){
        if(error)*error="A motion-sample report needs 2 to 256 sampled positions.";
        return false;
    }
    double previousTime=-1.0;
    for(const MotionInterferenceSample& sample:samples){
        if(!std::isfinite(sample.timeSeconds)||sample.timeSeconds<0||
           sample.timeSeconds<=previousTime||sample.timeSeconds>3600||
           !std::isfinite(sample.overlapVolumeMm3)||sample.overlapVolumeMm3<0||
           !std::isfinite(sample.clearanceMm)||sample.clearanceMm<0||
           !std::isfinite(sample.firstX)||!std::isfinite(sample.firstY)||!std::isfinite(sample.firstZ)||
           !std::isfinite(sample.secondX)||!std::isfinite(sample.secondY)||!std::isfinite(sample.secondZ)||
           sample.interfering!=(sample.overlapVolumeMm3>1e-7)){
            if(error)*error="Motion-sample report contains an invalid or out-of-order sample.";
            return false;
        }
        previousTime=sample.timeSeconds;
    }
    QByteArray bytes;
    QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    out << "Time (s),First body ID,First body name,Second body ID,Second body name,Shared volume (mm^3),Clearance (mm),Interfering,First X (mm),First Y (mm),First Z (mm),Second X (mm),Second Y (mm),Second Z (mm)\r\n";
    for(const MotionInterferenceSample& sample:samples){
        out << csvNumber(sample.timeSeconds) << ',' << csvField(firstId) << ',' << csvField(first->name)
            << ',' << csvField(secondId) << ',' << csvField(second->name) << ','
            << csvNumber(sample.overlapVolumeMm3) << ',' << csvNumber(sample.clearanceMm) << ','
            << (sample.interfering?1:0) << ','
            << csvNumber(sample.firstX) << ',' << csvNumber(sample.firstY) << ',' << csvNumber(sample.firstZ) << ','
            << csvNumber(sample.secondX) << ',' << csvNumber(sample.secondY) << ',' << csvNumber(sample.secondZ)
            << "\r\n";
    }
    out.flush();
    if(out.status()!=QTextStream::Ok){if(error)*error="Could not format the motion-sample CSV.";return false;}
    return writeCsvFile(path,bytes,error);
}

bool SceneDocument::exportMotionClearanceIntervalsCsv(const QString& firstId,const QString& secondId,
                                                     const QVector<MotionClearanceInterval>& intervals,
                                                     const QString& path,QString* error) const {
    const Part* first=find(firstId),*second=find(secondId);
    if(firstId==secondId||!first||!second){
        if(error)*error="Select two distinct existing bodies for a clearance-interval report.";
        return false;
    }
    if(intervals.isEmpty()||intervals.size()>255){
        if(error)*error="An interval audit needs 1 to 255 consecutive time intervals.";
        return false;
    }
    double previousEnd=-1.0;
    for(const auto& item:intervals){
        if(!std::isfinite(item.startSeconds)||!std::isfinite(item.endSeconds)||
           item.startSeconds<0||item.endSeconds<=item.startSeconds||item.endSeconds>3600||
           (previousEnd>=0&&item.startSeconds!=previousEnd)||
           !std::isfinite(item.minimumEndpointClearanceMm)||item.minimumEndpointClearanceMm<0||
           !std::isfinite(item.clearanceLowerBoundMm)||
           (!item.speedBoundAvailable&&item.provenAboveMargin)||
           (item.sampledOverlap&&item.provenAboveMargin)||
           (item.provenAboveMargin&&item.clearanceLowerBoundMm<=0)){
            if(error)*error="Clearance interval report contains invalid or nonconsecutive observations.";
            return false;
        }
        previousEnd=item.endSeconds;
    }
    QByteArray bytes;
    QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    out << "Start (s),End (s),First body ID,First body name,Second body ID,Second body name,"
           "Minimum endpoint clearance (mm),Conditional lower bound (mm),Speed bound supplied,"
           "Sampled overlap,Conditionally above margin,Needs refinement\r\n";
    for(const auto& item:intervals){
        out << csvNumber(item.startSeconds) << ',' << csvNumber(item.endSeconds) << ','
            << csvField(firstId) << ',' << csvField(first->name) << ','
            << csvField(secondId) << ',' << csvField(second->name) << ','
            << csvNumber(item.minimumEndpointClearanceMm) << ',';
        if(item.speedBoundAvailable)out << csvNumber(item.clearanceLowerBoundMm);
        out << ',' << (item.speedBoundAvailable?1:0)
            << ',' << (item.sampledOverlap?1:0)
            << ',' << (item.provenAboveMargin?1:0)
            << ',' << (item.refinementNeeded?1:0) << "\r\n";
    }
    out.flush();
    if(out.status()!=QTextStream::Ok){if(error)*error="Could not format the clearance-interval CSV.";return false;}
    return writeCsvFile(path,bytes,error);
}

bool SceneDocument::exportMotionContactsCsv(const QVector<MotionInterferenceContact>& contacts,
                                             const QString& path,QString* error) const {
    if(contacts.size()>50000){if(error)*error="Motion-contact report exceeds its row limit.";return false;}
    for(const MotionInterferenceContact& contact:contacts){
        if(!std::isfinite(contact.timeSeconds)||contact.timeSeconds<0||contact.timeSeconds>3600||
           !std::isfinite(contact.overlapVolumeMm3)||contact.overlapVolumeMm3<=1e-7||
           contact.firstBodyId==contact.secondBodyId||!find(contact.firstBodyId)||!find(contact.secondBodyId)){
            if(error)*error="Motion-contact report contains an invalid sample or missing body.";
            return false;
        }
    }
    QByteArray bytes;
    QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    out << "Time (s),First body ID,First body name,Second body ID,Second body name,Shared volume (mm^3)\r\n";
    for(const MotionInterferenceContact& contact:contacts){
        const Part* first=find(contact.firstBodyId),*second=find(contact.secondBodyId);
        out << csvNumber(contact.timeSeconds) << ',' << csvField(contact.firstBodyId) << ','
            << csvField(first->name) << ',' << csvField(contact.secondBodyId) << ','
            << csvField(second->name) << ',' << csvNumber(contact.overlapVolumeMm3) << "\r\n";
    }
    out.flush();
    if(out.status()!=QTextStream::Ok){if(error)*error="Could not format the motion-contact CSV.";return false;}
    return writeCsvFile(path,bytes,error);
}

bool SceneDocument::exportBillOfMaterialsCsv(const QString& path, QString* error) const {
    if (parts_.isEmpty()) { if (error) *error = "No bodies are available for the BOM."; return false; }
    QString detail;
    const auto rows = billOfMaterials(&detail);
    if (rows.isEmpty()) { if (error) *error = detail.isEmpty() ? QStringLiteral("The BOM could not be generated.") : detail; return false; }
    QByteArray bytes;
    QTextStream out(&bytes, QIODevice::WriteOnly);
    out.setLocale(QLocale::c());
    out << "Item,Part number,Description,Revision,Material,Quantity,Unit volume (mm^3),Total volume (mm^3),Unit mass (kg),Total mass (kg),Assembly path,Procurement type,Supplier,Supplier part number,Component definition ID,Component name,Material provenance,Young's modulus (GPa)\r\n";
    for (const auto& row : rows) {
        out << row.itemNumber << ',' << csvField(row.partNumber) << ',' << csvField(row.description) << ','
            << csvField(row.revision) << ',' << csvField(row.materialName) << ',' << row.quantity << ','
            << csvNumber(row.unitVolumeMm3) << ',' << csvNumber(row.totalVolumeMm3) << ','
            << (row.unitMassKg > 0.0 ? csvNumber(row.unitMassKg) : QString()) << ','
            << (row.totalMassKg > 0.0 ? csvNumber(row.totalMassKg) : QString()) << ',' << csvField(row.assemblyPath) << ','
            << csvField(row.procurementType) << ',' << csvField(row.supplier) << ',' << csvField(row.supplierPartNumber) << ','
            << csvField(row.componentDefinitionId) << ',' << csvField(row.componentName) << ','
            << csvField(row.materialEngineering.provenance) << ','
            << (row.materialEngineering.youngsModulusGPa>0.0 ? csvNumber(row.materialEngineering.youngsModulusGPa) : QString()) << "\r\n";
    }
    out.flush();
    if (out.status() != QTextStream::Ok) { if (error) *error = "Could not format the BOM CSV."; return false; }
    return writeCsvFile(path, bytes, error);
}

bool SceneDocument::exportPartsListCsv(const QString& path, QString* error) const {
    if (parts_.isEmpty()) { if (error) *error = "No bodies are available for the parts list."; return false; }
    const auto rows = partsList(error);
    if (rows.size() != parts_.size()) return false;
    QByteArray bytes;
    QTextStream out(&bytes, QIODevice::WriteOnly);
    out.setLocale(QLocale::c());
    out << "Item,Body ID,Body name,Part number,Description,Revision,Material,Quantity,Volume (mm^3),Mass (kg),Assembly path,Procurement type,Supplier,Supplier part number,Component definition ID,Component name,Occurrence ID,Translation offset X (mm),Translation offset Y (mm),Translation offset Z (mm),Material provenance,Young's modulus (GPa)\r\n";
    for (const auto& row : rows) {
        out << row.itemNumber << ',' << csvField(row.bodyId) << ',' << csvField(row.bodyName) << ','
            << csvField(row.partNumber) << ',' << csvField(row.description) << ',' << csvField(row.revision) << ','
            << csvField(row.materialName) << ",1," << csvNumber(row.volumeMm3) << ','
            << (row.massKg > 0.0 ? csvNumber(row.massKg) : QString()) << ',' << csvField(row.assemblyPath) << ','
            << csvField(row.procurementType) << ',' << csvField(row.supplier) << ',' << csvField(row.supplierPartNumber) << ','
            << csvField(row.componentDefinitionId) << ',' << csvField(row.componentName) << ',' << csvField(row.occurrenceId) << ','
            << csvNumber(row.placementX) << ',' << csvNumber(row.placementY) << ',' << csvNumber(row.placementZ) << ','
            << csvField(row.materialEngineering.provenance) << ','
            << (row.materialEngineering.youngsModulusGPa>0.0 ? csvNumber(row.materialEngineering.youngsModulusGPa) : QString()) << "\r\n";
    }
    out.flush();
    if (out.status() != QTextStream::Ok) { if (error) *error = "Could not format the parts-list CSV."; return false; }
    return writeCsvFile(path, bytes, error);
}

StandardsCheckReport SceneDocument::checkStandards(StandardsProfile profile) const {
    StandardsCheckReport report;
    report.profile = profile;
    auto add = [&report](FindingSeverity severity, const QString& code, const QString& bodyId, const QString& message) {
        report.findings.append({severity, code, bodyId, message});
        if (severity == FindingSeverity::Error) ++report.errors;
        else if (severity == FindingSeverity::Warning) ++report.warnings;
    };
    if (parts_.isEmpty()) add(FindingSeverity::Error, "DOC_EMPTY", {}, "The document contains no model bodies.");
    QHash<QString, QString> partNumbers;
    for (const Part& part : parts_) {
        BodyDiagnostics geometry;
        QString detail;
        if (!diagnoseBody(part.id, &geometry, &detail) || !geometry.valid || geometry.solids != 1 || geometry.volume <= 0.0) {
            add(FindingSeverity::Error, "GEOMETRY_INVALID", part.id,
                QStringLiteral("Body '%1' must contain one valid, positive-volume solid. %2").arg(part.name, detail));
        }
        if (part.partNumber.isEmpty()) {
            add(FindingSeverity::Warning, "PART_NUMBER_MISSING", part.id, QStringLiteral("Body '%1' has no part number.").arg(part.name));
        } else {
            static const QRegularExpression validPartNumber(QStringLiteral("^[A-Z0-9][A-Z0-9._-]{2,31}$"));
            if (!validPartNumber.match(part.partNumber).hasMatch())
                add(FindingSeverity::Error, "PART_NUMBER_FORMAT", part.id, QStringLiteral("Part number '%1' must be 3-32 uppercase letters, digits, dots, underscores, or hyphens.").arg(part.partNumber));
            const QString key = part.partNumber.toCaseFolded();
            if (partNumbers.contains(key)) {
                const Part* first = find(partNumbers.value(key));
                const bool sameMetadata = first && first->description == part.description && first->revision == part.revision &&
                    first->materialName == part.materialName && first->materialDensityKgPerM3 == part.materialDensityKgPerM3 &&
                    first->materialEngineering.provenance == part.materialEngineering.provenance &&
                    first->materialEngineering.youngsModulusGPa == part.materialEngineering.youngsModulusGPa &&
                    first->procurementType == part.procurementType && first->supplier == part.supplier &&
                    first->supplierPartNumber == part.supplierPartNumber;
                BodyDiagnostics firstGeometry;
                QString firstError;
                const bool sameGeometry = first && diagnoseBody(first->id, &firstGeometry, &firstError) &&
                    geometryKey(firstGeometry) == geometryKey(geometry);
                if (!sameMetadata || !sameGeometry)
                    add(FindingSeverity::Error, "PART_NUMBER_CONFLICT", part.id,
                        QStringLiteral("Part number '%1' is reused for different metadata or geometry.").arg(part.partNumber));
            } else partNumbers.insert(key, part.id);
        }
        if (part.description.isEmpty()) add(FindingSeverity::Warning, "DESCRIPTION_MISSING", part.id, QStringLiteral("Body '%1' has no description.").arg(part.name));
        if (part.revision.isEmpty()) add(FindingSeverity::Warning, "REVISION_MISSING", part.id, QStringLiteral("Body '%1' has no revision.").arg(part.name));
        if (part.materialName.isEmpty()) add(FindingSeverity::Warning, "MATERIAL_MISSING", part.id, QStringLiteral("Body '%1' has no assigned material.").arg(part.name));
        if (part.procurementType == "Purchased") {
            if (part.supplier.isEmpty()) add(FindingSeverity::Warning, "SUPPLIER_MISSING", part.id, QStringLiteral("Purchased item '%1' has no supplier.").arg(part.name));
            if (part.supplierPartNumber.isEmpty()) add(FindingSeverity::Warning, "SUPPLIER_PART_NUMBER_MISSING", part.id,
                                                       QStringLiteral("Purchased item '%1' has no supplier part number.").arg(part.name));
        }
        if(profile!=StandardsProfile::BomData&&
           (!drawingReadinessPolicy_.requiredTitleBlockFields.isEmpty()||!drawingReadinessPolicy_.requiredAnnotationConventions.isEmpty())){
            if(!part.hasDrawingSettings){
                add(FindingSeverity::Warning,"DRAWING_SHEET_MISSING",part.id,
                    QStringLiteral("Body '%1' has no saved drawing sheet to check against the project's configured drawing checklist.").arg(part.name));
            }else{
                const DrawingSheetSettings& sheet=part.drawingSettings;
                const auto requireField=[&](const QString& key,const QString& value){
                    if(drawingReadinessPolicy_.requiredTitleBlockFields.contains(key)&&value.trimmed().isEmpty())
                        add(FindingSeverity::Warning,"DRAWING_FIELD_MISSING",part.id,
                            QStringLiteral("Required project drawing field '%1' is empty on sheet '%2'.").arg(key,sheet.sheetId));
                };
                requireField("drawing_number",sheet.drawingNumber.isEmpty()?part.partNumber:sheet.drawingNumber);
                requireField("description",part.description);
                requireField("part_number",part.partNumber);
                requireField("revision",sheet.revision.isEmpty()?part.revision:sheet.revision);
                requireField("drawn_by",sheet.drawnBy);requireField("checked_by",sheet.checkedBy);
                requireField("issue_date",sheet.issueDate);requireField("material",part.materialName);
                QSet<QString> dimensionAxes;for(const DrawingDimensionAnnotation& dimension:sheet.dimensionAnnotations)dimensionAxes.insert(dimension.axis);
                const auto requireConvention=[&](const QString& key,bool present){
                    if(drawingReadinessPolicy_.requiredAnnotationConventions.contains(key)&&!present)
                        add(FindingSeverity::Warning,"DRAWING_CONVENTION_MISSING",part.id,
                            QStringLiteral("Required project drawing convention '%1' is missing on sheet '%2'.").arg(key,sheet.sheetId));
                };
                requireConvention("envelope_dimensions",dimensionAxes.contains("X")&&dimensionAxes.contains("Y")&&dimensionAxes.contains("Z"));
                requireConvention("projection_symbol",sheet.projection=="First-angle"||sheet.projection=="Third-angle");
                requireConvention("auxiliary_view",sheet.auxiliaryView=="Isometric"||sheet.auxiliaryView=="Section A-A");
                requireConvention("general_notes",!sheet.noteAnnotations.isEmpty()||!sheet.generalNotes.isEmpty());
                requireConvention("section_view",!sheet.sectionAnnotations.isEmpty());
                requireConvention("detail_view",!sheet.detailAnnotations.isEmpty());
                requireConvention("linked_parts_list",sheet.partsListAnnotation.visible&&!sheet.sourceBodyIds.isEmpty());
                requireConvention("bom_balloons",!sheet.balloonAnnotations.isEmpty());
                const DrawingSheetSourceStatus sourceStatus=drawingSheetSourceStatus(part.id);
                if(sourceStatus.state==DrawingSourceState::Stale)
                    add(FindingSeverity::Warning,"DRAWING_SOURCE_STALE",part.id,"The saved drawing source geometry or BOM/material revision is stale; refresh the link and export a new sheet.");
                else if(sourceStatus.state==DrawingSourceState::MissingBodies)
                    add(FindingSeverity::Warning,"DRAWING_SOURCE_MISSING",part.id,"One or more saved drawing source bodies are missing.");
            }
        }
    }
    if (profile == StandardsProfile::BomData) {
        add(FindingSeverity::Info, "CHECK_SCOPE", {}, "Checks BOM fields, part-number consistency, material metadata, and solid validity; it is not a third-party standards certification.");
    } else if (profile == StandardsProfile::IsoDrawingReadiness) {
        add(FindingSeverity::Info, "CHECK_SCOPE", {}, "ISO project readiness checks BOM fields, solid validity, and configured fields/conventions on saved sheets. Exported SVG content and formal ISO clauses/tolerances are not certified or fully inspected.");
    } else {
        add(FindingSeverity::Info, "CHECK_SCOPE", {}, "ASME project readiness checks BOM fields, solid validity, and configured fields/conventions on saved sheets. Exported SVG content and formal Y14 dimensioning, tolerancing, and GD&T rules are not certified or fully inspected.");
    }
    return report;
}
} // namespace cad

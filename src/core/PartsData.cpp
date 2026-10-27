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

namespace cad {
namespace {
QString csvField(QString value) {
    value.replace('"', "\"\"");
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}
QString csvNumber(double value) { return QString::number(value, 'g', 15); }
QString geometryKey(const BodyDiagnostics& g) {
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(csvNumber(g.volume), csvNumber(g.surfaceArea))
        .arg(g.solids).arg(g.shells).arg(g.faces).arg(g.edges).arg(g.vertices);
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
        rows.append({static_cast<int>(rows.size()) + 1, part->id, part->name, part->partNumber, part->description,
                     part->revision, part->materialName, 1, g.volume, mass, part->assemblyPath,
                     part->procurementType, part->supplier, part->supplierPartNumber});
    }
    return rows;
}

QVector<BillOfMaterialsRow> SceneDocument::billOfMaterials(QString* error) const {
    QVector<const Part*> ordered;
    ordered.reserve(parts_.size());
    for (const Part& part : parts_) ordered.append(&part);
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
        const QString group = source->partNumber.isEmpty()
            ? QStringLiteral("BODY:%1").arg(source->id)
            : QStringLiteral("PN:%1|%2|%3|%4|%5|%6|%7|%8|%9|%10")
                .arg(source->assemblyPath.toCaseFolded(), source->partNumber.toCaseFolded(), source->description.toCaseFolded(),
                     source->revision.toCaseFolded(), source->materialName.toCaseFolded(),
                     csvNumber(source->materialDensityKgPerM3), geometryKey(g), source->procurementType.toCaseFolded(),
                     source->supplier.toCaseFolded(), source->supplierPartNumber.toCaseFolded());
        auto it = rowForKey.constFind(group);
        if (it == rowForKey.cend()) {
            BillOfMaterialsRow row;
            row.itemNumber = rows.size() + 1;
            row.partNumber = source->partNumber;
            row.description = source->description;
            row.revision = source->revision;
            row.materialName = source->materialName;
            row.quantity = 1;
            row.unitVolumeMm3 = g.volume;
            row.totalVolumeMm3 = g.volume;
            row.unitMassKg = massKg;
            row.totalMassKg = massKg;
            row.assemblyPath = source->assemblyPath;
            row.procurementType = source->procurementType;
            row.supplier = source->supplier;
            row.supplierPartNumber = source->supplierPartNumber;
            rowForKey.insert(group, rows.size());
            rows.append(row);
        } else {
            BillOfMaterialsRow& row = rows[it.value()];
            ++row.quantity;
            row.totalVolumeMm3 += g.volume;
            row.totalMassKg += massKg;
        }
    }
    return rows;
}

bool SceneDocument::exportBillOfMaterialsCsv(const QString& path, QString* error) const {
    if (parts_.isEmpty()) { if (error) *error = "No bodies are available for the BOM."; return false; }
    QString detail;
    const auto rows = billOfMaterials(&detail);
    if (rows.isEmpty()) { if (error) *error = detail.isEmpty() ? QStringLiteral("The BOM could not be generated.") : detail; return false; }
    QByteArray bytes;
    QTextStream out(&bytes, QIODevice::WriteOnly);
    out.setLocale(QLocale::c());
    out << "Item,Part number,Description,Revision,Material,Quantity,Unit volume (mm^3),Total volume (mm^3),Unit mass (kg),Total mass (kg),Assembly path,Procurement type,Supplier,Supplier part number\r\n";
    for (const auto& row : rows) {
        out << row.itemNumber << ',' << csvField(row.partNumber) << ',' << csvField(row.description) << ','
            << csvField(row.revision) << ',' << csvField(row.materialName) << ',' << row.quantity << ','
            << csvNumber(row.unitVolumeMm3) << ',' << csvNumber(row.totalVolumeMm3) << ','
            << (row.unitMassKg > 0.0 ? csvNumber(row.unitMassKg) : QString()) << ','
            << (row.totalMassKg > 0.0 ? csvNumber(row.totalMassKg) : QString()) << ',' << csvField(row.assemblyPath) << ','
            << csvField(row.procurementType) << ',' << csvField(row.supplier) << ',' << csvField(row.supplierPartNumber) << "\r\n";
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
    out << "Item,Body ID,Body name,Part number,Description,Revision,Material,Quantity,Volume (mm^3),Mass (kg),Assembly path,Procurement type,Supplier,Supplier part number\r\n";
    for (const auto& row : rows) {
        out << row.itemNumber << ',' << csvField(row.bodyId) << ',' << csvField(row.bodyName) << ','
            << csvField(row.partNumber) << ',' << csvField(row.description) << ',' << csvField(row.revision) << ','
            << csvField(row.materialName) << ",1," << csvNumber(row.volumeMm3) << ','
            << (row.massKg > 0.0 ? csvNumber(row.massKg) : QString()) << ',' << csvField(row.assemblyPath) << ','
            << csvField(row.procurementType) << ',' << csvField(row.supplier) << ',' << csvField(row.supplierPartNumber) << "\r\n";
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
    }
    if (profile == StandardsProfile::BomData) {
        add(FindingSeverity::Info, "CHECK_SCOPE", {}, "Checks BOM fields, part-number consistency, material metadata, and solid validity; it is not a third-party standards certification.");
    } else if (profile == StandardsProfile::IsoDrawingReadiness) {
        add(FindingSeverity::Info, "CHECK_SCOPE", {}, "ISO drawing readiness only: model units are millimetres and BOM fields/solids are checked. Exported drawing sheets are not inspected; title-block completeness, projection symbols, dimension presentation, and ISO tolerance rules are not verified.");
    } else {
        add(FindingSeverity::Info, "CHECK_SCOPE", {}, "ASME drawing readiness only: BOM fields and solid validity are checked. Exported drawing sheets are not inspected; Y14 title-block, dimensioning, tolerancing, and GD&T rules are not verified.");
    }
    return report;
}
} // namespace cad

#pragma once

#include "SceneDocument.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace cad {
// Shared fingerprint used by both saved drawing-sheet revisions and exported
// SVG snapshots. The JSON field set/order and QJson compact encoding MUST
// remain consistent; its digest is part of the persisted drawing contract.
inline QString drawingSourceMetadataFingerprint(QVector<const Part*> sources) {
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
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(records).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
}
} // namespace cad

#pragma once

#include "SceneDocument.h"
#include <QByteArray>
#include <QVector>

namespace cad {
// Conservative inspection of complete, Z-axis cylindrical bore walls. A
// partially open slot, countersink, helical bore, or non-Z-axis hole is not
// silently called a drillable hole. This is a drawing aid, NOT a CAM plan.
struct AxialHoleRecord {
    int number=0;
    double xMm=0,yMm=0,diameterMm=0,zMinMm=0,zMaxMm=0;
    bool throughEnvelope=false;
};
struct AxialHoleAudit {
    QVector<AxialHoleRecord> holes;
    int excludedCylinders=0;
    double stockTopZ=0,stockBottomZ=0;
    QString sourceBodyId;
    QString sourceGeometryToken;
};
// The audit does not change the model. On any failure, *result is unchanged.
bool inspectAxialHoles(const Part& part,AxialHoleAudit* result,QString* error=nullptr);
// Data includes the model revision, explicit bounds and a disclaimer. A CSV
// is a snapshot, regenerated on demand, not a machining drawing or toolpath.
bool buildAxialHoleTableCsv(const AxialHoleAudit& audit,QByteArray* csv,QString* error=nullptr);
bool exportAxialHoleTableCsv(const Part& part,const QString& path,QString* error=nullptr);
}

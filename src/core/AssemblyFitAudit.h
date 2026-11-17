#pragma once
#include "AssemblyFitPolicy.h"
#include <QString>
#include <QStringList>
#include <QVector>

namespace cad {
class SceneDocument;

struct AssemblyFitPair {
    QString firstBodyId, secondBodyId;
    QString firstName, secondName;
    QString firstGeometryToken, secondGeometryToken;
    double distanceMm = 0.0;
    double overlapVolumeMm3 = 0.0;
    double closestFirstX = 0.0, closestFirstY = 0.0, closestFirstZ = 0.0;
    double closestSecondX = 0.0, closestSecondY = 0.0, closestSecondZ = 0.0;
    fit::Status status = fit::Status::Clear;
    double firstDeviationMm=0.0, secondDeviationMm=0.0;
    double lowerBoundGapMm=0.0, signedClearanceMarginMm=0.0;
    fit::ToleranceStatus toleranceStatus=fit::ToleranceStatus::NoGuaranteedSeparation;
};

// Read-only exact OCCT pairwise assembly fit analysis. Bounded at 32 bodies.
// Output is committed only after *all* pair evaluations succeed.
bool inspectAssemblyFit(const SceneDocument& document,const QStringList& bodyIds,
                        double requiredClearanceMm,QVector<AssemblyFitPair>* result,
                        QString* error=nullptr);
// Each deviation is a nonnegative bound on the actual solid's outward
// deviation from the corresponding nominal solid, in millimetres.
// Does not alter the document; reports commit only after all pairs succeed.
bool inspectAssemblyFitWithTolerances(const SceneDocument& document,const QStringList& bodyIds,
                        double requiredClearanceMm,const QVector<double>& maxDeviationByBodyMm,
                        QVector<AssemblyFitPair>* result,QString* error=nullptr);
// Audits all solid occurrences beneath a persisted assembly group, including
// nested groups. Uses the same exact OCCT pairwise measurement and 32-body cap
// as the selection audit; no geometry, pose or stored mate is modified.
bool inspectAssemblyGroupFit(const SceneDocument& document,const QString& groupPath,
                             double requiredClearanceMm,QVector<AssemblyFitPair>* result,
                             QString* error=nullptr);
// Determine whether any B-rep has changed since a report was captured.
bool assemblyFitReportCurrent(const SceneDocument& document,
                              const QVector<AssemblyFitPair>& report);
QString assemblyFitAuditCsv(const QVector<AssemblyFitPair>& report,
                            double requiredClearanceMm);
QString assemblyFitAuditText(const QVector<AssemblyFitPair>& report,
                             double requiredClearanceMm);
}

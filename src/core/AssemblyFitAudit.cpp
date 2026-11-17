#include "AssemblyFitAudit.h"
#include "SceneDocument.h"
#include <QSet>
#include <cmath>
#include <utility>

namespace cad {
namespace {
void setError(QString* out,const QString& message) { if(out) *out=message; }
QString csv(const QString& value) {
    QString escaped=value;
    // Quoting a CSV field alone does NOT prevent Excel/LibreOffice formula
    // evaluation. Neutralize formula-looking user-controlled names/IDs.
    const QString leading=escaped.trimmed();
    if(!leading.isEmpty() && QStringLiteral("=+-@").contains(leading.front()))
        escaped.prepend(QChar(0x27));
    escaped.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+escaped+QStringLiteral("\"");
}
QString number(double value) { return QString::number(value,'g',15); }
}

bool inspectAssemblyFit(const SceneDocument& document,const QStringList& bodyIds,
                        double requiredClearanceMm,QVector<AssemblyFitPair>* result,
                        QString* error) {
    return inspectAssemblyFitWithTolerances(document,bodyIds,requiredClearanceMm,
                                           QVector<double>(bodyIds.size(),0.0),result,error);
}

bool inspectAssemblyFitWithTolerances(const SceneDocument& document,const QStringList& bodyIds,
                        double requiredClearanceMm,const QVector<double>& maxDeviationByBodyMm,
                        QVector<AssemblyFitPair>* result,QString* error) {
    if(!result){setError(error,QStringLiteral("Missing assembly audit output."));return false;}
    if(!fit::validThreshold(requiredClearanceMm)){
        setError(error,QStringLiteral("Required clearance must be finite, nonnegative and no more than 1,000,000 mm."));
        return false;
    }
    if(bodyIds.size()<2 || bodyIds.size()>32){
        setError(error,QStringLiteral("Select between 2 and 32 solid bodies for an assembly fit audit."));return false;
    }
    if(maxDeviationByBodyMm.size()!=bodyIds.size()){
        setError(error,QStringLiteral("Provide one maximum geometric deviation per body."));return false;
    }
    for(double tolerance:maxDeviationByBodyMm){
        if(!fit::validThreshold(tolerance)){
            setError(error,QStringLiteral("Maximum per-body deviation must be finite and between 0 and 1,000,000 mm."));return false;
        }
    }
    QSet<QString> seen;
    for(const QString& id:bodyIds){
        const Part* part=document.find(id);
        if(id.isEmpty() || seen.contains(id) || !part || part->shape.IsNull() || part->isSurfaceBody){
            setError(error,QStringLiteral("Assembly fit audit requires distinct, existing solid bodies."));return false;
        }
        seen.insert(id);
    }
    QVector<AssemblyFitPair> staged;
    staged.reserve(bodyIds.size()*(bodyIds.size()-1)/2);
    for(qsizetype a=0;a<bodyIds.size();++a){
        for(qsizetype b=a+1;b<bodyIds.size();++b){
            MinimumDistanceResult distance;
            InterferenceResult overlap;
            QString cause;
            if(!document.measureMinimumDistance(bodyIds[a],bodyIds[b],&distance,&cause) ||
               !document.measureInterference(bodyIds[a],bodyIds[b],&overlap,&cause)){
                setError(error,QStringLiteral("Could not inspect pair %1 / %2: %3")
                    .arg(bodyIds[a],bodyIds[b],cause));
                return false;
            }
            fit::Status state;
            if(!fit::classify(distance.distance,overlap.volume,requiredClearanceMm,&state)){
                setError(error,QStringLiteral("CAD kernel returned invalid clearance or overlap measurements."));return false;
            }
            const Part* first=document.find(bodyIds[a]);
            const Part* second=document.find(bodyIds[b]);
            AssemblyFitPair pair;
            pair.firstBodyId=first->id;pair.secondBodyId=second->id;
            pair.firstName=first->name;pair.secondName=second->name;
            pair.firstGeometryToken=first->geometryToken;
            pair.secondGeometryToken=second->geometryToken;
            pair.distanceMm=distance.distance;pair.overlapVolumeMm3=overlap.volume;
            pair.closestFirstX=distance.firstX;pair.closestFirstY=distance.firstY;pair.closestFirstZ=distance.firstZ;
            pair.closestSecondX=distance.secondX;pair.closestSecondY=distance.secondY;pair.closestSecondZ=distance.secondZ;
            pair.status=state;
            pair.firstDeviationMm=maxDeviationByBodyMm[a];
            pair.secondDeviationMm=maxDeviationByBodyMm[b];
            fit::ToleranceResult tolerance;
            if(!fit::assessTolerance(pair.distanceMm,pair.overlapVolumeMm3,requiredClearanceMm,
                                     pair.firstDeviationMm,pair.secondDeviationMm,&tolerance)){
                setError(error,QStringLiteral("Invalid tolerance analysis for pair %1 / %2.")
                    .arg(bodyIds[a],bodyIds[b]));return false;
            }
            pair.lowerBoundGapMm=tolerance.lowerBoundGapMm;
            pair.signedClearanceMarginMm=tolerance.signedClearanceMarginMm;
            pair.toleranceStatus=tolerance.status;
            staged.append(std::move(pair));
        }
    }
    *result=std::move(staged);
    return true;
}

bool inspectAssemblyGroupFit(const SceneDocument& document,const QString& groupPath,
                             double requiredClearanceMm,QVector<AssemblyFitPair>* result,
                             QString* error) {
    const QString prefix=groupPath+QLatin1Char('/');
    if(groupPath.isEmpty() || !document.assemblyGroupPaths().contains(groupPath)) {
        setError(error,"Choose an existing assembly group to audit.");return false;
    }
    QStringList ids;
    for(const Part& part:document.parts()) {
        if((part.assemblyPath==groupPath || part.assemblyPath.startsWith(prefix)) &&
           !part.isSurfaceBody)ids.append(part.id);
    }
    if(ids.size()<2 || ids.size()>32) {
        setError(error,"An assembly-group clearance audit requires 2–32 solid bodies under the selected group.");
        return false;
    }
    return inspectAssemblyFit(document,ids,requiredClearanceMm,result,error);
}

bool assemblyFitReportCurrent(const SceneDocument& document,const QVector<AssemblyFitPair>& report){
    if(report.isEmpty())return false;
    for(const auto& pair:report){
        const Part* first=document.find(pair.firstBodyId);
        const Part* second=document.find(pair.secondBodyId);
        if(!first||!second||first->geometryToken!=pair.firstGeometryToken||
           second->geometryToken!=pair.secondGeometryToken)return false;
    }
    return true;
}

QString assemblyFitAuditCsv(const QVector<AssemblyFitPair>& report,double requiredClearanceMm){
    QString out=QStringLiteral("First ID,First body,Second ID,Second body,Minimum distance (mm),Overlap (mm^3),Required clearance (mm),Status,First maximum deviation (mm),Second maximum deviation (mm),Conservative minimum gap (mm),Signed clearance margin (mm),Tolerance status,First geometry token,Second geometry token\n");
    for(const auto& pair:report){
        const bool overlap=pair.toleranceStatus==fit::ToleranceStatus::NominalInterference;
        out+=csv(pair.firstBodyId)+','+csv(pair.firstName)+','+csv(pair.secondBodyId)+','+csv(pair.secondName)+','+
            number(pair.distanceMm)+','+number(pair.overlapVolumeMm3)+','+number(requiredClearanceMm)+','+
            csv(QString::fromLatin1(fit::statusName(pair.status)))+','+
            number(pair.firstDeviationMm)+','+number(pair.secondDeviationMm)+','+
            (overlap?QString():number(pair.lowerBoundGapMm))+','+
            (overlap?QString():number(pair.signedClearanceMarginMm))+','+
            csv(QString::fromLatin1(fit::toleranceStatusName(pair.toleranceStatus)))+','+
            csv(pair.firstGeometryToken)+','+csv(pair.secondGeometryToken)+'\n';
    }
    return out;
}
QString assemblyFitAuditText(const QVector<AssemblyFitPair>& report,double requiredClearanceMm){
    int overlaps=0,near=0,contacts=0,clear=0;
    int guaranteed=0,atRisk=0,unseparated=0;
    for(const auto& pair:report){
        switch(pair.status){
        case fit::Status::Interference:++overlaps;break;
        case fit::Status::InsufficientClearance:++near;break;
        case fit::Status::Contact:++contacts;break;
        case fit::Status::Clear:++clear;break;
        }
        switch(pair.toleranceStatus){
        case fit::ToleranceStatus::NominalInterference:break;
        case fit::ToleranceStatus::NoGuaranteedSeparation:++unseparated;break;
        case fit::ToleranceStatus::BelowRequiredClearance:++atRisk;break;
        case fit::ToleranceStatus::GuaranteedClearance:++guaranteed;break;
        }
    }
    QString out=QStringLiteral("ASSEMBLY FIT AUDIT (read-only, current geometry snapshot)\n")+
        QStringLiteral("Required clearance: %1 mm\nPairs: %2 | Interference: %3 | Below clearance: %4 | Contact: %5 | Clear: %6\n")
            .arg(number(requiredClearanceMm)).arg(report.size()).arg(overlaps).arg(near).arg(contacts).arg(clear)+
        QStringLiteral("Tolerance-aware: %1 guaranteed | %2 gap at risk | %3 separation not guaranteed | %4 nominal interference\n\n")
            .arg(guaranteed).arg(atRisk).arg(unseparated).arg(overlaps);
    for(const auto& pair:report){
        out+=QStringLiteral("%1 [%2] / %3 [%4]\n  Nominal: %5 | distance: %6 mm | shared volume: %7 mm^3\n")
            .arg(pair.firstName,pair.firstBodyId,pair.secondName,pair.secondBodyId,
                 QString::fromLatin1(fit::statusName(pair.status)),number(pair.distanceMm),
                 number(pair.overlapVolumeMm3));
        out+=QStringLiteral("  Body deviation bounds: %1 + %2 mm | %3")
            .arg(number(pair.firstDeviationMm),number(pair.secondDeviationMm),
                 QString::fromLatin1(fit::toleranceStatusName(pair.toleranceStatus)));
        if(pair.toleranceStatus!=fit::ToleranceStatus::NominalInterference)
            out+=QStringLiteral(" | minimum gap >= %1 mm | margin: %2 mm")
                .arg(number(pair.lowerBoundGapMm),number(pair.signedClearanceMarginMm));
        out+='\n';
    }
    out+=QStringLiteral("\nAssumption: each real body is entirely contained within its nominal solid expanded by the specified maximum outward deviation in every direction.\n")
        +QStringLiteral("The lower gap bound is max(0, measured separation - both deviations); a negative margin means the required clearance is NOT assured.\n")
        +QStringLiteral("A nominal overlap is reported separately; this is not a penetration-depth calculation.\n")
        +QStringLiteral("Zero deviation bounds check nominal geometry only. This is not a statistical tolerance study, GD&T certification, dynamic swept-clearance check, or manufacturing approval.\n");
    return out;
}
}

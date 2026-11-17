#include "DrillingWorksheet.h"
#include <QRegularExpression>
#include <QSaveFile>
#include <cmath>
#include <limits>
#include <vector>
namespace cad {
namespace {
bool fail(QString* error,const QString& message){if(error)*error=message;return false;}
}
bool buildDrillingWorksheetCsv(const Part& part,const DrillingReviewSettings& settings,
                              QByteArray* output,QString* error){
    if(!output)return fail(error,"Provide a drilling worksheet output buffer.");
    if(!std::isfinite(settings.clearanceAboveStockMm)||settings.clearanceAboveStockMm<=0.1||
       settings.clearanceAboveStockMm>10000||!std::isfinite(settings.diameterToleranceMm)||
       settings.diameterToleranceMm<0||settings.diameterToleranceMm>2)
        return fail(error,"Clearance and drill diameter tolerance must be finite, positive engineering values.");
    const QStringList entries=settings.toolCatalog.split(',',Qt::KeepEmptyParts);
    if(entries.isEmpty()||entries.size()>128)
        return fail(error,"Specify 1-128 drill entries in diameter:length format.");
    std::vector<drilling::Tool> tools;
    tools.reserve(static_cast<size_t>(entries.size()));
    for(int i=0;i<entries.size();++i){
        const QStringList pair=entries[i].trimmed().split(':',Qt::KeepEmptyParts);
        if(pair.size()!=2)return fail(error,"Each drill must have diameter:length in mm.");
        bool diameterOkay=false,lengthOkay=false;
        const double diameter=pair[0].trimmed().toDouble(&diameterOkay);
        const double length=pair[1].trimmed().toDouble(&lengthOkay);
        if(!diameterOkay||!lengthOkay||!std::isfinite(diameter)||!std::isfinite(length)||
           diameter<=0||length<=0)
            return fail(error,"Drill diameter and usable length must be finite and positive.");
        tools.push_back({QStringLiteral("T%1").arg(i+1).toStdString(),diameter,length});
    }
    AxialHoleAudit audit;
    if(!inspectAxialHoles(part,&audit,error))return false;
    if(audit.holes.isEmpty())return fail(error,"No complete Z-axis internal holes were found.");
    drilling::Setup setup;
    setup.bodyId=audit.sourceBodyId.toUtf8().toStdString();
    setup.geometryRevision=audit.sourceGeometryToken.toUtf8().toStdString();
    setup.stockTopZ=audit.stockTopZ;
    setup.stockBottomZ=audit.stockBottomZ;
    setup.clearanceZ=setup.stockTopZ+settings.clearanceAboveStockMm;
    setup.diameterTolerance=settings.diameterToleranceMm;
    setup.fixtureExitReviewed=settings.fixtureExitReviewed;
    std::vector<drilling::Hole> holes;
    holes.reserve(static_cast<size_t>(audit.holes.size()));
    for(const auto& hole:audit.holes){
        holes.push_back({QStringLiteral("H%1").arg(hole.number).toStdString(),
                         hole.xMm,hole.yMm,hole.diameterMm,hole.zMaxMm,hole.zMinMm,
                         hole.throughEnvelope});
    }
    drilling::Plan plan;std::string failure,csv;
    if(!drilling::plan(setup,holes,tools,&plan,&failure)||
       !drilling::exportReviewCsv(plan,&csv,&failure))
        return fail(error,QString::fromStdString(failure));
    // Do not omit exclusions from the exported worksheet: topological hole
    // detection is deliberately conservative and is not complete CAM feature recognition.
    csv+="Unsupported cylindrical faces excluded,"+std::to_string(audit.excludedCylinders)+"\r\n";
    csv+="Tool catalog (diameter:usable length mm),";
    for(std::size_t i=0;i<tools.size();++i){
        if(i)csv+=';';
        csv+=std::to_string(tools[i].diameter)+":"+std::to_string(tools[i].usableLength);
    }
    csv+="\r\n";
    *output=QByteArray::fromStdString(csv);
    if(error)error->clear();
    return true;
}
bool exportDrillingWorksheetCsv(const Part& part,const DrillingReviewSettings& settings,
                               const QString& path,QString* error){
    if(path.trimmed().isEmpty())return fail(error,"Provide a CSV filename.");
    QByteArray bytes;
    if(!buildDrillingWorksheetCsv(part,settings,&bytes,error))return false;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))return fail(error,file.errorString());
    if(file.write(bytes)!=bytes.size()){
        const QString message=file.errorString();file.cancelWriting();return fail(error,message);
    }
    if(!file.commit())return fail(error,file.errorString());
    if(error)error->clear();return true;
}
}

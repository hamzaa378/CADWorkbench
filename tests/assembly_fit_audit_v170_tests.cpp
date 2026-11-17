#include "core/AssemblyFitAudit.h"
#include "core/SceneDocument.h"
#include <QCoreApplication>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void eq(cad::fit::Status actual,cad::fit::Status expected){require(actual==expected,"Unexpected assembly fit classification");}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        cad::SceneDocument scene;QString error;
        require(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Cannot create first box");
        const QString a=scene.parts().last().id;
        require(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Cannot create second box");
        const QString b=scene.parts().last().id;
        require(scene.translate(b,28,0,0,&error),"Cannot position second box");
        QVector<cad::AssemblyFitPair> report;
        require(cad::inspectAssemblyFit(scene,{a,b},5,&report,&error),"Pair measurement failed");
        require(report.size()==1,"Pair count wrong");
        eq(report[0].status,cad::fit::Status::Clear);
        require(std::abs(report[0].distanceMm-8)<1e-6,"Eight mm gap not measured");
        require(cad::inspectAssemblyFitWithTolerances(scene,{a,b},5,{1.0,1.0},&report,&error),
                "Tolerance-aware fit audit failed");
        require(report[0].firstDeviationMm==1.0&&report[0].secondDeviationMm==1.0,
                "Body deviations not captured");
        require(report[0].toleranceStatus==cad::fit::ToleranceStatus::GuaranteedClearance &&
                std::abs(report[0].lowerBoundGapMm-6.0)<1e-6,
                "Nominal 8 mm with 2 mm budget must guarantee 6 mm");
        require(cad::inspectAssemblyFitWithTolerances(scene,{a,b},5,{2.0,2.0},&report,&error),
                "Risk audit failed");
        require(report[0].status==cad::fit::Status::Clear &&
                report[0].toleranceStatus==cad::fit::ToleranceStatus::BelowRequiredClearance,
                "Nominal clearance must remain distinct from tolerance risk");
        require(std::abs(report[0].signedClearanceMarginMm+1)<1e-6,
                "Negative worst-case clearance margin wrong");
        const QString csvRisk=cad::assemblyFitAuditCsv(report,5);
        require(csvRisk.contains("Conservative minimum gap (mm)")&&
                csvRisk.contains("Clearance at risk"),"Tolerance CSV incomplete");
        cad::AssemblyFitPair unsafeName=report[0];
        unsafeName.firstName=QStringLiteral("  =HYPERLINK(\"https://example.invalid\",\"click\")");
        unsafeName.secondName=QStringLiteral("@SUM(1,2)");
        const QString escapedCsv=cad::assemblyFitAuditCsv({unsafeName},5);
        require(escapedCsv.contains(QStringLiteral("\"'  =HYPERLINK")) &&
                escapedCsv.contains(QStringLiteral("\"'@SUM")),
                "Formula-like user labels must not be executable CSV cells");
        const QString textRisk=cad::assemblyFitAuditText(report,5);
        require(textRisk.contains("gap at risk")&&textRisk.contains("margin: -1"),
                "Human-readable tolerance review incomplete");
        const auto committed=report;
        require(!cad::inspectAssemblyFitWithTolerances(scene,{a,b},5,{1.0},&report,&error),
                "Mismatched deviation vector accepted");
        require(!cad::inspectAssemblyFitWithTolerances(scene,{a,b},5,{1.0,-1.0},&report,&error),
                "Negative deviation accepted");
        require(report[0].signedClearanceMarginMm==committed[0].signedClearanceMarginMm,
                "Failed tolerance scan altered last committed report");
        require(cad::inspectAssemblyFitWithTolerances(scene,{a,b},5,{4.0,4.0},&report,&error),
                "Zero-gap risk scan failed");
        require(report[0].toleranceStatus==cad::fit::ToleranceStatus::NoGuaranteedSeparation,
                "No guaranteed body separation should be reported");
        // Legacy entrypoint stays available for callers that only want nominal geometry.
        require(cad::inspectAssemblyFit(scene,{a,b},5,&report,&error),
                "Original nominal audit entrypoint broke");
        require(cad::assemblyFitReportCurrent(scene,report),"Fresh report erroneously stale");
        const QStringList duplicate{a,a};
        const auto before=report;
        require(!cad::inspectAssemblyFit(scene,duplicate,5,&report,&error),"Duplicate body accepted");
        require(report.size()==before.size()&&report[0].distanceMm==before[0].distanceMm,
                "Failed scan changed committed output");
        require(!cad::inspectAssemblyFit(scene,{a,b},-1,&report,&error),"Negative clearance accepted");
        const bool snapshot=argc>1 && QString::fromUtf8(argv[1])==QStringLiteral("snapshot");
        require(scene.translate(b,-5,0,0,&error),"Move to three mm gap failed");
        require(!cad::assemblyFitReportCurrent(scene,report),"Old report not invalidated by geometry edit");
        require(cad::inspectAssemblyFit(scene,{a,b},5,&report,&error),"Low-clearance scan failed");
        eq(report[0].status,cad::fit::Status::InsufficientClearance);
        require(std::abs(report[0].distanceMm-3)<1e-6,"Three mm gap wrong");
        require(scene.translate(b,-3,0,0,&error),"Contact placement failed");
        require(cad::inspectAssemblyFit(scene,{a,b},5,&report,&error),"Contact scan failed");
        eq(report[0].status,cad::fit::Status::Contact);
        require(scene.translate(b,-4,0,0,&error),"Overlap placement failed");
        require(cad::inspectAssemblyFit(scene,{a,b},5,&report,&error),"Overlap scan failed");
        eq(report[0].status,cad::fit::Status::Interference);
        require(std::abs(report[0].overlapVolumeMm3-800)<1e-4,"Overlap volume wrong");
        if(snapshot){
            const QString csv=cad::assemblyFitAuditCsv(report,5);
            require(csv.startsWith("First ID,") && csv.contains("Interference"),"CSV report incorrect");
            const QString txt=cad::assemblyFitAuditText(report,5);
            require(txt.contains("Interference: 1")&&txt.contains("800"),"Text summary incorrect");
            require(scene.undo(),"Undo failed");
            require(!cad::assemblyFitReportCurrent(scene,report),"Undo did not invalidate snapshot");
        }
        require(scene.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Third body failed");
        const QString c=scene.parts().last().id;
        require(scene.translate(c,75,0,0,&error),"Third body translation failed");
        require(cad::inspectAssemblyFit(scene,{a,b,c},5,&report,&error),"Three-body scan failed");
        require(report.size()==3,"Expected all unique assembly body pairs");
        require(scene.setPartMetadata(a,"BOX-A","A","A","Mechanism/Moving",&error)&&
                scene.setPartMetadata(b,"BOX-B","B","A","Mechanism/Moving",&error)&&
                scene.setPartMetadata(c,"BOX-C","C","A","Mechanism/Frame",&error),
                "Assign assembly-group hierarchy for clearance scan");
        require(cad::inspectAssemblyGroupFit(scene,"Mechanism",5,&report,&error)&&
                report.size()==3,"Root group must include nested subassembly pairs");
        require(cad::inspectAssemblyGroupFit(scene,"Mechanism/Moving",5,&report,&error)&&
                report.size()==1,"Nested group must exclude other siblings");
        const auto groupReport=report;
        require(!cad::inspectAssemblyGroupFit(scene,"Mechanism/Frame",5,&report,&error),
                "Singleton group incorrectly permitted clearance scan");
        require(!cad::inspectAssemblyGroupFit(scene,"DoesNotExist",5,&report,&error),
                "Nonexistent group accepted");
        require(report.size()==groupReport.size()&&
                report[0].firstBodyId==groupReport[0].firstBodyId,
                "Rejected group scan must preserve previous report");
        require(cad::assemblyFitReportCurrent(scene,report),"Fresh three-body report stale");
        std::cout<<"Assembly fit native integration passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

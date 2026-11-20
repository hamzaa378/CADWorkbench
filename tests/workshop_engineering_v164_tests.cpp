#include "core/WorkshopEngineering.h"
#include "core/AssemblyToleranceBudget.h"
#include "core/HolePositionInspection.h"
#include "core/HoleInspectionWorkflow.h"
#include "core/HoleRevisionImpact.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <algorithm>
#include <numbers>
#include <string>
#include <vector>
using namespace cad::workshop;
static void verifyManufacturingFitBudget(){
    const std::vector<ToleranceTerm> chain={{"plate",50,-0.15,0.10,1},{"bush",20,-0.05,0.08,-1},{"shim",29.5,-0.02,0.03,-1}};
    AssemblyToleranceBudget result;std::string error;
    if(!assessAssemblyToleranceBudget(chain,0.20,0.80,&result,&error)||result.decision!=FitDecision::FeasibleWorstCase)
        throw std::runtime_error("Valid manufacturing fit budget rejected");
    if(!assessAssemblyToleranceBudget(chain,0.30,0.60,&result,&error)||result.decision!=FitDecision::BothLimitsViolated)
        throw std::runtime_error("Unsafe manufacturing fit budget accepted");
}

namespace {
int checks=0,failed=0;
void expect(bool cond,const char* what){++checks;if(!cond){++failed;std::cerr<<"FAIL: "<<what<<'\n';}}
bool near(double a,double b,double relative=1e-9){return std::abs(a-b)<=relative*std::max({1.0,std::abs(a),std::abs(b)});}
void sections(){
    SectionResult r;std::string err;
    expect(sectionProfiles().size()==8,"eight section families");
    SectionSpec s{"rectangle",40,20,0,0,7850};
    expect(calculateSection(s,&r,&err),"rectangle valid");
    expect(near(r.areaMm2,800),"rectangle area");
    expect(near(r.centroidXmm,20)&&near(r.centroidYmm,10),"rectangle center");
    expect(near(r.secondMomentXmm4,40*std::pow(20.0,3)/12),"rectangle Ixx");
    expect(near(r.secondMomentYmm4,20*std::pow(40.0,3)/12),"rectangle Iyy");
    expect(near(r.modulusXmm3,r.secondMomentXmm4/10),"elastic section modulus");
    expect(near(r.massKgPerMeter,800*7850*1e-6),"section mass per metre");
    s={"circle",20,0,0,0,0};
    expect(calculateSection(s,&r,&err),"circle valid");
    expect(near(r.areaMm2,std::numbers::pi*100),"circle area");
    expect(near(r.secondMomentXmm4,std::numbers::pi*2500),"circle Ixx");
    expect(near(r.secondMomentXmm4,r.secondMomentYmm4),"circle equal inertia");
    s={"hollow_circle",20,0,2,0,0};
    expect(calculateSection(s,&r,&err),"tube valid");
    expect(near(r.areaMm2,std::numbers::pi*(100-64)),"tube area");
    expect(near(r.secondMomentXmm4,std::numbers::pi*(10000-4096)/4),"tube inertia");
    s={"box",60,40,4,0,0};
    expect(calculateSection(s,&r,&err),"box valid");
    expect(near(r.areaMm2,60*40-52*32),"box area");
    expect(near(r.secondMomentXmm4,(60*std::pow(40.0,3)-52*std::pow(32.0,3))/12),"box Ixx");
    expect(near(r.secondMomentYmm4,(40*std::pow(60.0,3)-32*std::pow(52.0,3))/12),"box Iyy");
    for(const std::string profile:{"i_beam","channel","tee","angle"}){
        s={profile,80,120,6,10,7850};
        expect(calculateSection(s,&r,&err),"open section valid");
        expect(r.areaMm2>0&&r.secondMomentXmm4>0&&r.secondMomentYmm4>0,"open section positive moments");
        expect(near(r.polarAreaMomentMm4,r.secondMomentXmm4+r.secondMomentYmm4),"polar identity");
        expect(r.gyrationXmm>0&&r.gyrationYmm>0,"radii of gyration");
        if(profile=="channel"||profile=="angle")expect(std::abs(r.centroidXmm-40)>0.1,"unsymmetric center x");
        if(profile=="angle")expect(std::abs(r.productMomentXYmm4)>1,"angle has nonzero product inertia");
        if(profile=="i_beam")expect(near(r.centroidXmm,40)&&near(r.centroidYmm,60),"I centroids");
    }
    const SectionResult previous=r;
    for(const auto& broken:std::vector<SectionSpec>{{"unknown",40,20,0,0,0},{"box",20,20,11,0,0},
          {"i_beam",50,80,51,5,0},{"i_beam",50,80,6,42,0},{"hollow_circle",20,0,11,0,0},
          {"rectangle",-1,20,0,0,0},{"rectangle",40,20,0,0,-1},
          {"rectangle",std::numeric_limits<double>::quiet_NaN(),20,0,0,0}}){
        expect(!calculateSection(broken,&r,&err),"reject invalid section");
        expect(near(r.areaMm2,previous.areaMm2),"reject without output mutation");
    }
    expect(!calculateSection(s,nullptr,&err),"reject null result");
    // Centroidal inertia can be checked by independently meshing the rectangle decomposition;
    // the geometry library is not involved, so numerical results are deterministic.
    for(int i=1;i<=100;++i){
        const double w=5+i,h=10+i*0.5;
        const SectionSpec v{"rectangle",w,h,0,0,0};
        expect(calculateSection(v,&r,&err),"parametric rectangle");
        expect(near(r.areaMm2,w*h),"rectangle analytic area parametric");
        expect(near(r.secondMomentXmm4,w*h*h*h/12),"rectangle analytic Ixx parametric");
        expect(near(r.secondMomentYmm4,h*w*w*w/12),"rectangle analytic Iyy parametric");
    }
}
void machining(){
    MachiningResult r;std::string err;
    expect(machiningOperations().size()==8,"eight machining families");
    MachiningSpec s; s.operation="drill";s.toolDiameterMm=10;s.travelMm=20;s.cuttingSpeedMMin=30;
    s.feedPerRevMm=0.2;s.passes=2;s.materialDensityKgM3=7850;
    expect(estimateMachining(s,&r,&err),"drilling estimate");
    const double desired=30000/(std::numbers::pi*10);
    expect(near(r.spindleRpm,desired),"drilling RPM");
    expect(near(r.feedMmMin,desired*0.2),"drilling feed");
    expect(near(r.cycleTimeMin,40/r.feedMmMin),"drilling cycle time");
    expect(near(r.nominalEngagementVolumeMm3,std::numbers::pi*25*40),"nominal cutter envelope");
    expect(near(r.nominalChipMassKg,r.nominalEngagementVolumeMm3*7850*1e-9),"nominal chip mass");
    s.maxSpindleRpm=500;
    expect(estimateMachining(s,&r,&err),"capped spindle estimate");
    expect(near(r.spindleRpm,500),"capped RPM");
    expect(!r.warnings.empty(),"capped warning");
    s.operation="mill_slot";s.feedPerToothMm=0.05;s.teeth=4;s.radialWidthMm=5;s.depthMm=2;
    expect(estimateMachining(s,&r,&err),"milling estimate");
    expect(near(r.feedMmMin,500*0.05*4),"milling feed per tooth");
    expect(near(r.nominalEngagementVolumeMm3,5*2*20*2),"milling envelope");
    s.specificCuttingEnergyJMm3=3.0;s.availablePowerKw=0.0001;
    expect(estimateMachining(s,&r,&err),"milling power");
    expect(r.approximatePowerKw>0,"positive power");
    expect(r.warnings.size()>=3,"power warning");
    for(const std::string& kind:machiningOperations()){
        s.operation=kind;
        expect(estimateMachining(s,&r,&err),"all machining operation templates");
        expect(r.cycleTimeMin>0&&std::isfinite(r.cycleTimeMin),"finite time");
    }
    const double old=r.spindleRpm;
    s.operation="broken";
    expect(!estimateMachining(s,&r,&err),"unknown operation rejected");
    expect(near(r.spindleRpm,old),"unknown unchanged");
    s.operation="tap";s.feedPerRevMm=0;
    expect(!estimateMachining(s,&r,&err),"zero feed rejected");
    s.feedPerRevMm=0.2;s.toolDiameterMm=0;
    expect(!estimateMachining(s,&r,&err),"zero diameter rejected");
    s.toolDiameterMm=10;s.passes=0;
    expect(!estimateMachining(s,&r,&err),"zero passes rejected");
    s.passes=1;s.cuttingSpeedMMin=std::numeric_limits<double>::infinity();
    expect(!estimateMachining(s,&r,&err),"infinity rejected");
    s.cuttingSpeedMMin=30;
    expect(!estimateMachining(s,nullptr,&err),"null result rejected");
    for(int n=1;n<=100;++n){
        s.operation="drill";s.toolDiameterMm=1.0+n/10.0;s.travelMm=n;s.passes=1;
        expect(estimateMachining(s,&r,&err),"parameterized drilling");
        expect(near(r.nominalEngagementVolumeMm3,std::numbers::pi/4*s.toolDiameterMm*s.toolDiameterMm*n),"analytical drill envelope");
    }
}
void holePositionInspection(){
    using namespace cad::inspection;
    HolePositionMeasurement h;
    h.holeId="BRACKET-H1";h.nominalXmm=20;h.nominalYmm=10;
    h.measuredDatumXmm=20.1;h.measuredDatumYmm=10;
    h.measuredProjectedXmm=20.1;h.measuredProjectedYmm=10;
    h.actualDiameterMm=10.2;h.minimumDiameterMm=10;h.maximumDiameterMm=10.4;
    h.diametralPositionToleranceMm=0.1;
    h.materialCondition=PositionMaterialCondition::MaximumMaterialCondition;
    h.maximumPinDiameterMm=9.5;
    HolePatternInspection report;std::string issue;
    expect(inspectHolePositionPattern({h},&report,&issue),"workshop hole metrology accepted");
    expect(report.allAccepted&&report.findings.size()==1,"MMC measured position + size accepted");
    h.measuredDatumXmm=20.3;h.measuredProjectedXmm=20.3;
    expect(inspectHolePositionPattern({h},&report,&issue),"shop bad position evaluated");
    expect(!report.allAccepted&&report.rejectedPosition==1,"bad hole placement rejected");
    auto saved=report;
    h.holeId.clear();
    expect(!inspectHolePositionPattern({h},&report,&issue),"bad shop hole ID rejected");
    expect(report.rejectedPosition==saved.rejectedPosition&&report.checkedHoles==saved.checkedHoles,
           "failed shop inspection does not overwrite previous results");
}
void revisionBoundHoleWorkflows(){
    using namespace cad::inspection;
    std::string issue;
    const std::vector<DesignHoleReference> design={
        {"PLATE","geom-rev-7",1,15,25,10},
        {"PLATE","geom-rev-7",2,45,25,10}};
    IndependentHoleMeasurement first;
    first.bodyId="PLATE";first.geometryToken="geom-rev-7";first.holeNumber=1;
    first.measuredDatumXmm=15.05;first.measuredDatumYmm=25;
    first.measuredProjectedXmm=15.05;first.measuredProjectedYmm=25;
    first.actualDiameterMm=10.2;first.minimumDiameterMm=10;
    first.maximumDiameterMm=10.5;first.diametralPositionToleranceMm=.2;
    first.materialCondition=PositionMaterialCondition::MaximumMaterialCondition;
    auto second=first;second.holeNumber=2;
    second.measuredDatumXmm=45.05;second.measuredProjectedXmm=45.05;
    HolePatternInspection report;
    expect(reviewRevisionBoundHoleMeasurements(design,{second,first},&report,&issue),
           "revision bound measurements join by body ID and hole number");
    expect(report.allAccepted&&report.checkedHoles==2,
           "CAD nominal locations used for metrology verdict");
    const auto saved=report;
    second.geometryToken="geom-rev-8";
    expect(!reviewRevisionBoundHoleMeasurements(design,{first,second},&report,&issue),
           "stale source geometry token must fail closed");
    expect(report.checkedHoles==saved.checkedHoles&&report.acceptedHoles==saved.acceptedHoles,
           "stale inspection cannot overwrite reviewed evidence");
    second=first;
    expect(!reviewRevisionBoundHoleMeasurements(design,{first,second},&report,&issue),
           "duplicate measured hole identity rejected");
    expect(!reviewRevisionBoundHoleMeasurements(design,{first},&report,&issue),
           "incomplete inspection is rejected, not silently approved");
}
void holeRevisionWorkflows(){
    using namespace cad::inspection;
    std::string issue;
    const std::vector<DesignHoleReference> old={
        {"BRACKET","geometry-7",1,10,20,5},
        {"BRACKET","geometry-7",2,30,20,5}};
    auto next=old;
    HoleRevisionImpact result;
    expect(compareDesignHoleRevisions(old,next,&result,&issue),
           "same CAD hole references must compare");
    expect(result.referenceMatches==2&&!result.evidenceApproval,
           "unchanged reference does not approve existing metrology");
    next[0].geometryToken="geometry-8";
    expect(compareDesignHoleRevisions(old,next,&result,&issue),
           "changing source revision must compare");
    expect(result.changed==1&&result.referenceMatches==1&&
           result.records.front().freshMeasurementRequired,
           "geometry revision invalidates hole measurement evidence");
    next.push_back({"BRACKET","geometry-8",3,40,20,5});
    expect(compareDesignHoleRevisions(old,next,&result,&issue)&&result.added==1,
           "new engineering hole requires measurement");
    const auto previous=result;
    next[0].nominalXmm=std::numeric_limits<double>::quiet_NaN();
    expect(!compareDesignHoleRevisions(old,next,&result,&issue),
           "nonfinite nominal geometry must be rejected");
    expect(result.records.size()==previous.records.size()&&result.added==previous.added,
           "invalid revision cannot overwrite earlier evidence");
}
void tolerances(){
    ToleranceResult r;std::string err;
    const std::vector<ToleranceTerm> terms={{"housing",20,-0.05,0.05,1},
        {"spacer",5,-0.02,0.03,-1},{"clearance",1,-0.01,0.02,1}};
    expect(calculateToleranceStack(terms,&r,&err),"tolerance chain valid");
    expect(near(r.nominalMm,16),"tolerance nominal");
    expect(near(r.worstMinMm,19.95-5.03+0.99),"tolerance min");
    expect(near(r.worstMaxMm,20.05-4.98+1.02),"tolerance max");
    expect(near(r.rssHalfWidthMm,std::hypot(std::hypot(.05,.025),.015)),"RSS halfwidth");
    const auto saved=r;
    expect(!calculateToleranceStack({},&r,&err),"empty chain rejected");
    expect(near(r.nominalMm,saved.nominalMm),"failed chain unchanged");
    expect(!calculateToleranceStack({{"a",1,.02,-.02,1}},&r,&err),"reversed tolerance rejected");
    expect(!calculateToleranceStack({{"a",1,-.02,.02,0}},&r,&err),"invalid sign rejected");
    expect(!calculateToleranceStack({{"",1,-.02,.02,1}},&r,&err),"empty label rejected");
    expect(!calculateToleranceStack(terms,nullptr,&err),"null result rejected");
    for(int n=1;n<=100;++n){
        const double half=n*0.001;
        expect(calculateToleranceStack({{"A",10,-half,half,1},{"B",2,-half,half,-1}},&r,&err),"symmetric stack");
        expect(near(r.worstMinMm,8-2*half)&&near(r.worstMaxMm,8+2*half),"symmetric worst limits");
        expect(near(r.rssHalfWidthMm,std::sqrt(2.0)*half),"symmetric RSS");
    }
}
}
int main(int argc,char** argv){
    const std::string name=argc>1?argv[1]:"all";
    if(name=="all"||name=="sections")sections();
    if(name=="all"||name=="machining")machining();
    if(name=="all"||name=="tolerances"){tolerances();verifyManufacturingFitBudget();holePositionInspection();revisionBoundHoleWorkflows();holeRevisionWorkflows();}
    if(checks==0){std::cerr<<"Unknown test suite\n";return 2;}
    std::cout<<(checks-failed)<<"/"<<checks<<" workshop checks passed\n";
    return failed?1:0;
}

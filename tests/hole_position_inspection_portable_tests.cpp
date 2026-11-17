#include "core/HolePositionInspection.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
using namespace cad::inspection;
static int checks=0;
static void test(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
static HolePositionMeasurement base(){
    HolePositionMeasurement h;
    h.holeId="H1";h.nominalXmm=10;h.nominalYmm=20;
    h.measuredDatumXmm=10.05;h.measuredDatumYmm=20;
    h.measuredProjectedXmm=10.05;h.measuredProjectedYmm=20;
    h.actualDiameterMm=10.2;h.minimumDiameterMm=10;h.maximumDiameterMm=10.5;
    h.diametralPositionToleranceMm=.15;
    h.materialCondition=PositionMaterialCondition::MaximumMaterialCondition;
    h.maximumPinDiameterMm=9.9;
    return h;
}
int main(){try{
    std::string error;HolePatternInspection report;
    auto h=base();
    test(inspectHolePositionPattern({h},&report,&error),"valid MMC inspection");
    test(report.allAccepted&&report.acceptedHoles==1,"valid hole accepted");
    const auto f=report.findings.front();
    test(std::abs(f.measuredDiametralPositionMm-.1)<1e-12,"diametral position");
    test(std::abs(f.materialBonusMm-.2)<1e-12,"MMC bonus");
    test(std::abs(f.allowedDiametralPositionMm-.35)<1e-12,"allowance");
    test(std::abs(f.minimumPinClearanceMm-.2)<1e-12,"pin clearance");
    h.materialCondition=PositionMaterialCondition::RegardlessOfFeatureSize;
    h.measuredDatumXmm=10.1;h.measuredProjectedXmm=10.1;
    test(inspectHolePositionPattern({h},&report,&error),"RFS inspection");
    test(!report.allAccepted&&report.rejectedPosition==1,"RFS excludes bonus");
    h.materialCondition=PositionMaterialCondition::MaximumMaterialCondition;
    h.projectedHeightMm=10;h.measuredProjectedXmm=10.25;
    test(inspectHolePositionPattern({h},&report,&error),"projected inspection");
    test(report.findings[0].maximumCenterDeviationMm>.24,"projected axis checked");
    test(!report.findings[0].positionPass,"projected bend position failure");
    h=base();h.maximumPinDiameterMm=10.3;
    test(inspectHolePositionPattern({h},&report,&error),"pin mismatch evaluated");
    test(!report.findings[0].pinClearancePass,"pin fit rejection");
    h=base();h.actualDiameterMm=9.9;
    test(inspectHolePositionPattern({h},&report,&error),"undersize hole inspected");
    test(report.rejectedSize==1,"undersize reported without failing API");
    h=base();h.actualDiameterMm=10.6;
    test(inspectHolePositionPattern({h},&report,&error),"oversize hole inspected");
    test(report.rejectedSize==1,"oversize reported");
    const auto saved=report;
    h.holeId="";
    test(!inspectHolePositionPattern({h},&report,&error),"empty hole id rejected");
    test(report.acceptedHoles==saved.acceptedHoles&&report.findings[0].holeId==saved.findings[0].holeId,"rollback on invalid ID");
    h=base();h.measuredDatumXmm=std::numeric_limits<double>::quiet_NaN();
    test(!inspectHolePositionPattern({h},&report,&error),"NaN rejected");
    h=base();h.projectedHeightMm=0;h.measuredProjectedXmm=10.1;
    test(!inspectHolePositionPattern({h},&report,&error),"inconsistent zero-height axis rejected");
    h=base();h.materialCondition=static_cast<PositionMaterialCondition>(98);
    test(!inspectHolePositionPattern({h},&report,&error),"unknown material condition rejected");
    h=base();
    test(!inspectHolePositionPattern({h,h},&report,&error),"duplicate ID rejected");
    test(!inspectHolePositionPattern({},&report,&error),"empty list rejected");
    test(!inspectHolePositionPattern({h},nullptr,&error),"null report rejected");
    std::mt19937 rng(45321);
    std::uniform_real_distribution<double> offset(-.8,.8),size(0,1),nominal(-100,100);
    for(int i=0;i<12000;++i){
        h=base();h.holeId="P"+std::to_string(i);
        h.nominalXmm=nominal(rng);h.nominalYmm=nominal(rng);
        double dx=offset(rng),dy=offset(rng),ex=offset(rng),ey=offset(rng);
        h.measuredDatumXmm=h.nominalXmm+dx;
        h.measuredDatumYmm=h.nominalYmm+dy;
        h.measuredProjectedXmm=h.nominalXmm+ex;
        h.measuredProjectedYmm=h.nominalYmm+ey;
        h.projectedHeightMm=10;
        h.minimumDiameterMm=10;h.maximumDiameterMm=11;h.actualDiameterMm=10+size(rng);
        h.maximumPinDiameterMm=9.5+size(rng);
        h.diametralPositionToleranceMm=.2+size(rng);
        h.materialCondition=i%2==0?PositionMaterialCondition::MaximumMaterialCondition:
                                        PositionMaterialCondition::RegardlessOfFeatureSize;
        test(inspectHolePositionPattern({h},&report,&error),"random valid hole");
        const auto& f=report.findings.front();
        const double expected=2*std::max(std::hypot(dx,dy),std::hypot(ex,ey));
        test(std::abs(f.measuredDiametralPositionMm-expected)<1e-10,"random independent geometry oracle");
        const double bonus=i%2==0?h.actualDiameterMm-10:0;
        test(std::abs(f.allowedDiametralPositionMm-h.diametralPositionToleranceMm-bonus)<1e-10,"random condition oracle");
        test(f.pinClearancePass==(f.minimumPinClearanceMm>=0),"random fit oracle");
        test(f.accepted==(f.sizePass&&f.positionPass&&f.pinClearancePass),"random overall decision");
    }
    std::cout<<checks<<" hole-position inspection checks passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

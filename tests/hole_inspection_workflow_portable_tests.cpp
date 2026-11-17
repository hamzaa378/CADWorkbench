#include "core/HoleInspectionWorkflow.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace cad::inspection;
int main(){
    int checks=0;
    auto expect=[&](bool ok,const char* msg){++checks;if(!ok){std::cerr<<msg<<"\n";std::abort();}};
    std::vector<DesignHoleReference> design;
    std::vector<IndependentHoleMeasurement> measured;
    for(int i=1;i<=512;++i){
        const double x=i*1.25,y=i*.8;
        design.push_back({"BODY-A","geometry-v1",i,x,y,8});
        IndependentHoleMeasurement m;
        m.bodyId="BODY-A";m.geometryToken="geometry-v1";m.holeNumber=i;
        m.measuredDatumXmm=x+.02;m.measuredDatumYmm=y;
        m.measuredProjectedXmm=x+.02;m.measuredProjectedYmm=y;
        m.actualDiameterMm=8.1;m.minimumDiameterMm=8;m.maximumDiameterMm=8.2;
        m.diametralPositionToleranceMm=.1;
        m.materialCondition=PositionMaterialCondition::MaximumMaterialCondition;
        measured.push_back(m);
    }
    std::string error;HolePatternInspection report;
    expect(reviewRevisionBoundHoleMeasurements(design,measured,&report,&error),"full inspection joined");
    expect(report.checkedHoles==512&&report.allAccepted,"512 verified measurements");
    const auto accepted=report;
    std::mt19937 gen(27);
    for(int repeat=0;repeat<80;++repeat){
        std::shuffle(measured.begin(),measured.end(),gen);
        expect(reviewRevisionBoundHoleMeasurements(design,measured,&report,&error),"shuffled metrology joins");
        expect(report.allAccepted&&report.findings.front().holeId==accepted.findings.front().holeId,
               "nominal order stable across external measurement order");
        for(int i=0;i<512;++i)
            expect(report.findings[i].accepted&&std::abs(report.findings[i].measuredDiametralPositionMm-.04)<1e-8,
                   "measurements bound to original CAD design coordinate");
    }
    auto broken=measured;
    broken.front().geometryToken="obsolete";
    expect(!reviewRevisionBoundHoleMeasurements(design,broken,&report,&error),"reject stale revision");
    expect(report.allAccepted&&report.checkedHoles==512,"stale rejection atomic");
    broken=measured;broken[0].holeNumber=broken[1].holeNumber;
    expect(!reviewRevisionBoundHoleMeasurements(design,broken,&report,&error),"reject duplicate measured hole");
    broken=measured;broken[0].measuredDatumXmm=std::nan("");
    expect(!reviewRevisionBoundHoleMeasurements(design,broken,&report,&error),"reject invalid metrology input");
    expect(!reviewRevisionBoundHoleMeasurements(design,{},&report,&error),"reject missing measurements");
    auto changed=design;changed[0].geometryToken="geometry-v2";
    expect(!reviewRevisionBoundHoleMeasurements(changed,measured,&report,&error),"reject design change");
    auto duplicate=design;duplicate[1].holeNumber=1;
    expect(!reviewRevisionBoundHoleMeasurements(duplicate,measured,&report,&error),"reject duplicate CAD hole");
    expect(!reviewRevisionBoundHoleMeasurements(design,measured,nullptr,&error),"reject null result");
    expect(report.allAccepted&&report.checkedHoles==512,"failure does not corrupt result");
    std::cout<<checks<<" revision-bound design-to-measurement assertions passed\n";
}

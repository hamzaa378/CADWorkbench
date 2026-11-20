#include "core/HoleRevisionImpact.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>
using namespace cad::inspection;
int main() {
    std::size_t checks=0;
    const auto require=[&](bool condition,const char* msg) {
        ++checks;if(!condition) {std::cerr<<msg<<"\n";std::abort();}
    };
    std::vector<DesignHoleReference> old,now;
    for(int i=1;i<=800;++i) {
        old.push_back({i%2?"A":"B","rev1",i,i*0.2,i*0.3,8});
        now.push_back(old.back());
    }
    HoleRevisionImpact report;std::string error;
    require(compareDesignHoleRevisions(old,now,&report,&error),"initial match");
    require(report.referenceMatches==800&&!report.evidenceApproval,"matching references are not approval");
    now[0].geometryToken="rev2";
    now[1].nominalDiameterMm=9;
    now.erase(now.begin()+2);
    now.push_back({"C","rev3",99,1,2,3});
    require(compareDesignHoleRevisions(old,now,&report,&error),"changed inventory");
    require(report.changed==2&&report.added==1&&report.removed==1,"count invalidations");
    for(const auto& h:report.records)
        require((h.disposition==HoleRevisionDisposition::ReferenceMatch)==!h.freshMeasurementRequired,
                "unchanged references require independent review but not an automatic remeasurement");
    const auto accepted=report;
    std::mt19937 gen(71);
    for(int repeat=0;repeat<100;++repeat) {
        std::shuffle(old.begin(),old.end(),gen);
        std::shuffle(now.begin(),now.end(),gen);
        require(compareDesignHoleRevisions(old,now,&report,&error),"order invariant comparison");
        require(report.records.size()==accepted.records.size(),"stable count");
        for(std::size_t j=0;j<report.records.size();++j)
            require(report.records[j].bodyId==accepted.records[j].bodyId&&
                    report.records[j].holeNumber==accepted.records[j].holeNumber&&
                    report.records[j].disposition==accepted.records[j].disposition,
                    "stable identity and disposition");
    }
    const auto snapshot=report;
    now[0].nominalDiameterMm=std::nan("");
    require(!compareDesignHoleRevisions(old,now,&report,&error),"reject nonfinite current design");
    require(report.records.size()==snapshot.records.size()&&
            report.referenceMatches==snapshot.referenceMatches,"atomic fail");
    now[0].nominalDiameterMm=8;
    now.push_back(now[0]);
    require(!compareDesignHoleRevisions(old,now,&report,&error),"duplicate identities fail");
    require(!compareDesignHoleRevisions(old,now,nullptr,&error),"null output fail");
    require(!error.empty(),"explain refusal");
    std::cout<<checks<<" revision change assertions passed\n";
}

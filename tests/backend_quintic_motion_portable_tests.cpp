#include "core/MotionDriveLimitReview.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cad::joint_motion;
static void expect(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
int main(){
try{
    int checks=0;
    std::string error;
    std::vector<Drive> drives{{1,0,120,-10,150},{2,40,-20,-30,50}};
    std::vector<DriveLimit> limits{{1,25,60},{2,15,30}};
    PlannedQuinticMotion planned;
    expect(planQuinticMotion(drives,limits,65,&planned,&error),"valid quintic plan");++checks;
    expect(planned.samples.size()==65&&planned.envelopes.size()==2,"plan dimensions");++checks;
    expect(planned.durationSeconds>=7.5,"plan covers second drive speed");++checks;
    expect(planned.samples.front().coordinates[0].second==0,"initial endpoint");++checks;
    expect(planned.samples.back().coordinates[0].second==120,"final endpoint");++checks;
    expect(planned.samples.back().coordinates[1].second==-20,"reverse final endpoint");++checks;
    for(const auto& env:planned.envelopes){
        const auto& lim=env.id==1?limits[0]:limits[1];
        expect(env.peakSpeed<=lim.maxSpeedUnitsPerSecond+1e-8,"speed limit");++checks;
        expect(env.peakAcceleration<=lim.maxAccelerationUnitsPerSecond2+1e-8,"acceleration limit");++checks;
        expect(!env.accelerationDiscontinuous,"quintic accelerations are continuous at endpoints");++checks;
    }
    for(std::size_t i=1;i<planned.samples.size();++i){
        expect(planned.samples[i].timeSeconds>planned.samples[i-1].timeSeconds,"strictly increasing times");++checks;
        expect(planned.samples[i].coordinates[0].second>=planned.samples[i-1].coordinates[0].second,"monotonic positive drive");++checks;
        expect(planned.samples[i].coordinates[1].second<=planned.samples[i-1].coordinates[1].second,"monotonic reverse drive");++checks;
    }
    std::vector<std::pair<int,double>> coordinates;
    expect(interpolate(drives,.5,Progression::QuinticSmoothstep,&coordinates,&error),"halfway evaluate");++checks;
    expect(std::abs(coordinates[0].second-60)<1e-9,"symmetry midpoint");++checks;
    const PlannedQuinticMotion old=planned;
    limits[0].maxSpeedUnitsPerSecond=0;
    expect(!planQuinticMotion(drives,limits,65,&planned,&error),"reject zero moving speed");++checks;
    expect(planned.samples.size()==old.samples.size(),"preserve output on failure");++checks;
    limits[0].maxSpeedUnitsPerSecond=25;
    limits.push_back(limits[0]);
    expect(!planQuinticMotion(drives,limits,65,&planned,&error),"reject mismatched duplicate limit");++checks;
    limits.pop_back();
    limits[1].id=1;
    expect(!planQuinticMotion(drives,limits,65,&planned,&error),"reject duplicate wrong ID");++checks;
    limits[1].id=2;
    expect(!planQuinticMotion(drives,limits,1,&planned,&error),"reject insufficient samples");++checks;
    expect(!planQuinticMotion(drives,limits,1025,&planned,&error),"reject huge samples");++checks;
    std::vector<Drive> still{{3,10,10,0,20}};
    std::vector<DriveLimit> stillLimit{{3,0,0}};
    expect(planQuinticMotion(still,stillLimit,2,&planned,&error),"stationary command requires no speed");++checks;
    expect(std::abs(planned.durationSeconds-0.001)<1e-12,"stationary duration");++checks;
    expect(std::abs(planned.samples[1].coordinates[0].second-10)<1e-9,"stationary coordinate");++checks;
    std::cout<<"backend quintic motion: "<<checks<<" checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}

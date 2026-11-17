#include "core/RigidMotionSpeedBound.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
using namespace cad::motion;
static int checks=0;
static void test(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
int main(){try{
    std::string error;RigidPairSpeedBound bound;
    RigidBodySpeedEnvelope a{{-10,-1,-1,10,1,1},0,0,0,2,3};
    RigidBodySpeedEnvelope b{{50,-1,-1,52,1,1},51,0,0,1,0};
    test(calculateRigidPairSpeedBound(a,b,&bound,&error),"rigid bound valid");
    test(std::abs(bound.firstBodyRadiusMm-std::sqrt(102.0))<1e-12,"origin-to-whole-body radius");
    test(std::abs(bound.secondBodyRadiusMm-std::sqrt(3.0))<1e-12,"fixture radius");
    test(std::abs(bound.relativeSurfaceSpeedBoundMmPerSecond-(3+3*std::sqrt(102.0)))<1e-10,"relative speed additive");
    const auto previous=bound;
    a.maximumAngularSpeedRadPerSecond=-1;
    test(!calculateRigidPairSpeedBound(a,b,&bound,&error),"negative rate rejected");
    test(bound.relativeSurfaceSpeedBoundMmPerSecond==previous.relativeSurfaceSpeedBoundMmPerSecond,"invalid result unchanged");
    a.maximumAngularSpeedRadPerSecond=3;
    a.originXmm=std::numeric_limits<double>::quiet_NaN();
    test(!calculateRigidPairSpeedBound(a,b,&bound,&error),"NaN origin rejected");
    a.originXmm=0;
    // Rigid point revolves once, touching a small stationary fixture only
    // at t=0.5. Both supplied endpoint samples are safely separated.
    const double pi=std::acos(-1.0);
    RigidBodySpeedEnvelope spinning{{-10.5,-.5,-.5,-9.5,.5,.5},0,0,0,0,2*pi};
    RigidBodySpeedEnvelope fixture{{9.5,-.5,-.5,10.5,.5,.5},10,0,0,0,0};
    // Enclosure is in the initial local reference frame; use 10 mm offset
    // about a fixed origin. This is a *user-established* angular speed cap.
    Settings s;s.maximumSamples=128;s.safetyClearanceMm=.1;
    s.coarseStepSeconds=.125;s.fineStepSeconds=.00390625;
    Result report;RigidPairSpeedBound proof;
    // The endpoint pair in this fixture is itself coincident. Use an
    // alternative angular phase with two clear endpoints and a hidden strike.
    const Evaluate hidden=[&](double t,Observation* sample,std::string*){
        const double angle=pi+2*pi*t;
        const double distance=std::hypot(10*std::cos(angle)-10,10*std::sin(angle));
        sample->clearanceMm=std::max(0.0,distance-1.0);
        sample->overlapVolumeMm3=distance<1?1:0;
        return true;
    };
    test(scanRigidPairWithSpeedBounds({0,1},s,spinning,fixture,hidden,&report,&proof,&error),"rotation interval scan");
    test(report.observations.size()>2,"rotation midpoints sampled");
    bool foundCollision=false;
    for(const auto& o:report.observations)if(o.overlapVolumeMm3>0)foundCollision=true;
    test(foundCollision,"transient rotated collision found");
    test(!report.clearanceBoundSatisfied,"cannot prove collision-free motion after observed overlap");
    test(proof.relativeSurfaceSpeedBoundMmPerSecond>62,"derived surface speed bound");
    const auto old=report.observations.size();
    spinning.maximumAngularSpeedRadPerSecond=-1;
    test(!scanRigidPairWithSpeedBounds({0,1},s,spinning,fixture,hidden,&report,&proof,&error),"invalid speed rejected");
    test(report.observations.size()==old,"failed scan leaves report unchanged");
    spinning.maximumAngularSpeedRadPerSecond=2*pi;
    // A stationary body far from another stationary body permits a genuine
    // conditional clearance bound when both declared maxima are zero.
    spinning.maximumAngularSpeedRadPerSecond=0;
    const Evaluate safe=[](double,Observation* sample,std::string*){
        sample->clearanceMm=20;sample->overlapVolumeMm3=0;return true;
    };
    test(scanRigidPairWithSpeedBounds({0,1},s,spinning,fixture,safe,&report,&proof,&error),"static clearance proof");
    test(report.clearanceBoundSatisfied&&report.intervalsWithoutClearanceProof==0,"static interval certified conditional on given speed bound");
    std::mt19937 rng(14235);
    std::uniform_real_distribution<double> velocity(0,500),radius(1,100);
    for(int i=0;i<10000;++i){
        const double v=velocity(rng),w=velocity(rng),r=radius(rng);
        a={{-r,-r,-r,r,r,r},0,0,0,v,w};
        b={{-1,-1,-1,1,1,1},0,0,0,0,0};
        test(calculateRigidPairSpeedBound(a,b,&bound,&error),"random speed bound valid");
        test(std::abs(bound.firstBodyRadiusMm-r*std::sqrt(3.0))<1e-10,"random reference radius");
        test(std::abs(bound.relativeSurfaceSpeedBoundMmPerSecond-(v+w*r*std::sqrt(3.0)))<1e-8,"random velocity envelope");
    }
    std::cout<<checks<<" rigid-motion speed-bound checks passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

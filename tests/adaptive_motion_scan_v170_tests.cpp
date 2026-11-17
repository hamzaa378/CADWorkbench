#include "core/AdaptiveMotionScan.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <iostream>
using namespace cad::motion;
int main(){
    int checks=0;
    const auto check=[&](bool value){if(!value){std::cerr<<"check "<<checks+1<<" failed\n";std::abort();}++checks;};
    const Evaluate clear=[](double t,Observation* o,std::string*){
        o->timeSeconds=t;o->clearanceMm=2;o->overlapVolumeMm3=0;return true;
    };
    Result original;original.observations={{17,2,0}};
    std::string error;
    Settings settings;settings.maximumSamples=50;settings.safetyClearanceMm=0.25;
    settings.coarseStepSeconds=0.25;settings.fineStepSeconds=0.015625;
    check(scanAdaptive({0,1},settings,clear,&original,&error));
    check(original.observations.size()==5);
    check(original.intervals.size()==4);
    check(original.intervals.front().startSeconds==0&&original.intervals.front().endSeconds==.25);
    check(original.intervals.front().minimumEndpointClearanceMm==2);
    check(!original.intervals.front().speedBoundAvailable);
    check(!original.intervals.front().provenAboveMargin);
    check(!original.intervals.front().sampledOverlap);
    for(std::size_t i=1;i<original.intervals.size();++i)
        check(original.intervals[i-1].endSeconds==original.intervals[i].startSeconds);
    check(!original.budgetExhausted&&original.intervalsNeedingRefinement==0);
    check(original.observations.front().timeSeconds==0);
    check(original.observations.back().timeSeconds==1);
    for(std::size_t i=1;i<original.observations.size();++i)
        check(original.observations[i].timeSeconds>original.observations[i-1].timeSeconds);
    check(error.empty());
    // A narrow collision is absent at both endpoints; midpoint refinement
    // finds it. This is NOT a proof for collisions narrower than the grid.
    int evaluations=0;
    const Evaluate collision=[&](double t,Observation* o,std::string*){
        ++evaluations;
        o->clearanceMm=std::max(0.0,std::abs(t-.5)*100.0-1.0);
        o->overlapVolumeMm3=(std::abs(t-.5)<.01?5.0:0.0);
        return true;
    };
    Result impact;
    check(scanAdaptive({0,1},settings,collision,&impact,&error));
    check(impact.observations.size()>5);
    check(std::any_of(impact.observations.begin(),impact.observations.end(),
                      [](const auto& o){return o.timeSeconds==.5&&o.overlapVolumeMm3>0;}));
    check(evaluations==static_cast<int>(impact.observations.size()));
    // Keyframe times are always measured and preserved, even if risk-free.
    Result keyed;
    check(scanAdaptive({0,.1,.7,1},settings,clear,&keyed,&error));
    for(double t:{0.,.1,.7,1.})
        check(std::any_of(keyed.observations.begin(),keyed.observations.end(),
                          [t](const auto& o){return o.timeSeconds==t;}));
    // Hard sample budget is explicit; no overshoot or false completeness.
    Settings limited=settings;limited.maximumSamples=3;
    Result bounded;
    check(scanAdaptive({0,1},limited,collision,&bounded,&error));
    check(bounded.observations.size()==3);
    check(bounded.budgetExhausted&&bounded.intervalsNeedingRefinement>0);
    const Result saved=bounded;
    check(!scanAdaptive({0,1,1},limited,clear,&bounded,&error));
    check(bounded.observations.size()==saved.observations.size());
    check(bounded.budgetExhausted==saved.budgetExhausted);
    check(!scanAdaptive({.5,0},limited,clear,&bounded,&error));
    check(!scanAdaptive({0,std::numeric_limits<double>::infinity()},limited,clear,&bounded,&error));
    check(!scanAdaptive({0,3601},limited,clear,&bounded,&error));
    check(!scanAdaptive({0,1},Settings{1,.1,.5,.01},clear,&bounded,&error));
    check(!scanAdaptive({0,1},Settings{50,-1,.5,.01},clear,&bounded,&error));
    check(!scanAdaptive({0,1},Settings{50,.1,.01,.5},clear,&bounded,&error));
    check(!scanAdaptive({0,1},limited,Evaluate{},&bounded,&error));
    check(!scanAdaptive({0,1},limited,clear,nullptr,&error));
    const Evaluate fails=[](double t,Observation* o,std::string* e){
        if(t>0&&t<1){if(e)*e="simulated solver failure";return false;}
        o->clearanceMm=10;return true;
    };
    check(!scanAdaptive({0,1},limited,fails,&bounded,&error));
    check(error=="simulated solver failure");
    check(bounded.observations.size()==saved.observations.size());
    const Evaluate bad=[](double,Observation* o,std::string*){
        o->clearanceMm=std::numeric_limits<double>::quiet_NaN();return true;
    };
    check(!scanAdaptive({0,1},limited,bad,&bounded,&error));
    check(!scanAdaptive({0,1},limited,[](double,Observation* o,std::string*){
        o->clearanceMm=0;o->overlapVolumeMm3=-1;return true;
    },&bounded,&error));
    check(bounded.observations.size()==saved.observations.size());
    // Risky intervals are refined more finely than remote ones.
    Settings risky=settings;risky.maximumSamples=32;risky.coarseStepSeconds=.5;risky.fineStepSeconds=.0625;
    Result gradient;
    const Evaluate leftRisk=[](double t,Observation* o,std::string*){
        o->clearanceMm=t<.5?0.1:100.0;o->overlapVolumeMm3=0;return true;
    };
    check(scanAdaptive({0,.5,1},risky,leftRisk,&gradient,&error));
    check(std::any_of(gradient.observations.begin(),gradient.observations.end(),
                      [](const auto& o){return o.timeSeconds==.25;}));
    check(gradient.observations.size()<=32);
    // CONDITIONAL continuous-clearance proof: the engineer must independently
    // establish a conservative upper bound for the moving surfaces.
    Settings proven=settings;
    proven.maximumSamples=2;
    proven.provenRelativeSpeedBoundMmPerSecond=2.0;
    Result safe;
    check(scanAdaptive({0,1},proven,[](double,Observation* o,std::string*){
        o->clearanceMm=8;o->overlapVolumeMm3=0;return true;
    },&safe,&error));
    check(safe.clearanceBoundSatisfied);
    check(safe.intervalsWithoutClearanceProof==0);
    check(safe.conservativeClearanceLowerBoundMm>=7.0-1e-9);
    check(safe.observations.size()==2&&!safe.budgetExhausted);
    check(safe.intervals.size()==1);
    check(safe.intervals.front().speedBoundAvailable&&safe.intervals.front().provenAboveMargin);
    check(std::abs(safe.intervals.front().clearanceLowerBoundMm-7.0)<1e-12);
    check(!safe.intervals.front().sampledOverlap&&!safe.intervals.front().refinementNeeded);
    // A fine-step sampling threshold is not a proof: the dip at t=0.5
    // requires refinement and must NOT be classified safe.
    Settings approach=settings;
    approach.maximumSamples=33;
    approach.provenRelativeSpeedBoundMmPerSecond=2.0;
    approach.safetyClearanceMm=0.2;
    const Evaluate dip=[](double t,Observation* o,std::string*){
        o->clearanceMm=std::abs(t-.5)*2.0;o->overlapVolumeMm3=0;return true;
    };
    Result nearMiss;
    check(scanAdaptive({0,1},approach,dip,&nearMiss,&error));
    check(!nearMiss.clearanceBoundSatisfied&&nearMiss.intervalsWithoutClearanceProof>0);
    check(std::any_of(nearMiss.observations.begin(),nearMiss.observations.end(),
         [](const Observation& o){return o.timeSeconds==.5&&o.clearanceMm==0;}));
    // Refined samples themselves can expose a false speed bound. No partial
    // result may escape when the measured clearance changes too quickly.
    Settings contradictory=settings;
    contradictory.provenRelativeSpeedBoundMmPerSecond=1;
    Result retained=nearMiss;
    check(!scanAdaptive({0,1},contradictory,[](double t,Observation* o,std::string*){
        o->clearanceMm=10*t;o->overlapVolumeMm3=0;return true;
    },&nearMiss,&error));
    check(error.find("contradicts measured clearance")!=std::string::npos);
    check(nearMiss.observations.size()==retained.observations.size());
    check(nearMiss.intervalsWithoutClearanceProof==retained.intervalsWithoutClearanceProof);
    check(nearMiss.intervals.size()==retained.intervals.size());
    check(nearMiss.intervals.front().startSeconds==retained.intervals.front().startSeconds);
    Settings impossible=proven;
    impossible.provenRelativeSpeedBoundMmPerSecond=std::numeric_limits<double>::quiet_NaN();
    check(!scanAdaptive({0,1},impossible,clear,&nearMiss,&error));
    impossible.provenRelativeSpeedBoundMmPerSecond=-2;
    check(!scanAdaptive({0,1},impossible,clear,&nearMiss,&error));
    // A hidden off-centre clearance dip (not at the midpoint) must NOT be
    // certified by two safe-looking endpoint observations under a large
    // but valid speed bound and an exhausted sampling budget.
    Settings narrow=proven;
    narrow.maximumSamples=2;
    narrow.provenRelativeSpeedBoundMmPerSecond=100.0;
    Result offCentre;
    check(scanAdaptive({0,1},narrow,[](double t,Observation* o,std::string*){
        o->clearanceMm=std::max(0.0,100.0*std::abs(t-.37)-0.5);
        o->overlapVolumeMm3=o->clearanceMm==0?1.0:0.0;
        return true;
    },&offCentre,&error));
    check(!offCentre.clearanceBoundSatisfied);
    check(offCentre.intervalsWithoutClearanceProof==1&&offCentre.budgetExhausted);
    check(offCentre.observations.size()==2);
    check(offCentre.intervals.size()==1);
    check(offCentre.intervals.front().speedBoundAvailable&&!offCentre.intervals.front().provenAboveMargin);
    check(offCentre.intervals.front().refinementNeeded);
        // Without an independent speed bound, even dense sampled-safe intervals
    // remain unproven. Retain compatibility for existing study callers.
    check(!original.clearanceBoundSatisfied);
    check(original.intervalsWithoutClearanceProof==static_cast<int>(original.observations.size())-1);
    // An exact threshold is not certified because numerical uncertainty and
    // a potentially touching configuration remain possible.
    Settings marginal=proven;marginal.safetyClearanceMm=7.0;
    check(scanAdaptive({0,1},marginal,[](double,Observation* o,std::string*){
        o->clearanceMm=8;return true;
    },&safe,&error));
    check(!safe.clearanceBoundSatisfied);
    check(safe.intervalsWithoutClearanceProof>0);
        std::cout<<"PASS: "<<checks<<" adaptive motion planner checks\n";
}

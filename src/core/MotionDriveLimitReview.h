#pragma once
#include "JointMotionInterpolation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

// Analytic envelope of the *commanded coordinate* time law, NOT the velocity
// or acceleration of solids, linkages or tool geometry. In linear mode a
// stopped-to-moving transition has discontinuous speed at endpoints, so a
// finite acceleration limit cannot be certified for a moving linear drive.
namespace cad::joint_motion {
struct DriveLimit {int id=0;double maxSpeedUnitsPerSecond=0,maxAccelerationUnitsPerSecond2=0;};
struct DriveEnvelope {int id=0;double peakSpeed=0,peakAcceleration=0;bool accelerationDiscontinuous=false;};
inline bool reviewDriveLimits(const std::vector<Drive>& drives,double durationSeconds,
        Progression progression,const std::vector<DriveLimit>& limits,
        std::vector<DriveEnvelope>* result,std::string* error=nullptr){
    const auto fail=[&](const char* msg){if(error)*error=msg;return false;};
    if(!result||!std::isfinite(durationSeconds)||durationSeconds<=0||durationSeconds>1e9||
       drives.empty()||drives.size()>128||drives.size()!=limits.size())
        return fail("Provide a finite positive motion duration and limits for every drive.");
    if(progression!=Progression::Linear&&progression!=Progression::Smoothstep&&
       progression!=Progression::QuinticSmoothstep)
        return fail("Unsupported drive interpolation mode.");
    std::vector<std::pair<int,double>> endpoints;
    std::string reason;
    if(!interpolate(drives,1.0,progression,&endpoints,&reason))
        return fail("Motion endpoints are invalid.");
    std::vector<DriveEnvelope> staged;staged.reserve(drives.size());
    for(const auto& d:drives){
        const auto it=std::find_if(limits.begin(),limits.end(),[&](const DriveLimit& l){return l.id==d.id;});
        if(it==limits.end()||std::count_if(limits.begin(),limits.end(),[&](const DriveLimit& l){return l.id==d.id;})!=1||
           !std::isfinite(it->maxSpeedUnitsPerSecond)||it->maxSpeedUnitsPerSecond<0||
           !std::isfinite(it->maxAccelerationUnitsPerSecond2)||it->maxAccelerationUnitsPerSecond2<0)
            return fail("Drive limits are missing, duplicated, or invalid.");
        const double travel=std::abs(d.target-d.initial);
        if(!std::isfinite(travel))return fail("Drive travel overflow.");
        // Quintic smoothstep: f(s)=10s^3-15s^4+6s^5.  Its exact
        // peak derivatives are 15/8 and 10/sqrt(3); both endpoint
        // velocity and acceleration are zero (unlike cubic smoothstep).
        const double speedFactor=progression==Progression::QuinticSmoothstep?1.875:
                                 progression==Progression::Smoothstep?1.5:1.0;
        const double accelerationFactor=progression==Progression::QuinticSmoothstep?
                    10.0/std::sqrt(3.0):progression==Progression::Smoothstep?6.0:0.0;
        const double speed=speedFactor*travel/durationSeconds;
        const double acceleration=accelerationFactor*(travel/durationSeconds)/durationSeconds;
        if(!std::isfinite(speed)||!std::isfinite(acceleration))return fail("Drive envelope overflow.");
        if(speed>it->maxSpeedUnitsPerSecond+1e-10)
            return fail("Drive exceeds commanded-coordinate speed limit.");
        // End-stop acceleration is undefined for a moving linear ramp;
        // do not advertise a finite acceleration check as passed.
        if(progression==Progression::Linear&&travel>0&&std::isfinite(it->maxAccelerationUnitsPerSecond2))
            return fail("Moving linear drive has endpoint velocity jumps; choose smoothstep for a finite acceleration review.");
        if(acceleration>it->maxAccelerationUnitsPerSecond2+1e-10)
            return fail("Drive exceeds commanded-coordinate acceleration limit.");
        staged.push_back({d.id,speed,acceleration,false});
    }
    *result=std::move(staged);
    if(error)error->clear();
    return true;
}

// Backend-only motion preparation. Returns the shortest *mathematically*
// feasible quintic command duration under per-coordinate speed/acceleration
// limits; no modification to a SceneDocument and no claim about collision,
// actuator torque, mechanism singularities or elastic loading. The caller
// must still run the articulated pose solver and clearance scan.
struct PlannedMotionSample {
    double timeSeconds=0.0;
    std::vector<std::pair<int,double>> coordinates;
};
struct PlannedQuinticMotion {
    double durationSeconds=0.0;
    std::vector<DriveEnvelope> envelopes;
    std::vector<PlannedMotionSample> samples;
};
inline bool planQuinticMotion(const std::vector<Drive>& drives,
        const std::vector<DriveLimit>& limits,int sampleCount,
        PlannedQuinticMotion* output,std::string* error=nullptr){
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!output||sampleCount<2||sampleCount>1024||drives.empty()||
       drives.size()>128||drives.size()!=limits.size())
        return fail("Quintic planning requires 1-128 drives, matching limits and 2-1024 samples.");
    std::vector<std::pair<int,double>> ends;
    std::string issue;
    if(!interpolate(drives,1.0,Progression::QuinticSmoothstep,&ends,&issue))
        return fail("Invalid motion drive inventory or travel interval.");
    double duration=0.0;
    for(const auto& drive:drives){
        const auto it=std::find_if(limits.begin(),limits.end(),
            [&](const DriveLimit& lim){return lim.id==drive.id;});
        if(it==limits.end()||std::count_if(limits.begin(),limits.end(),
               [&](const DriveLimit& lim){return lim.id==drive.id;})!=1||
           !std::isfinite(it->maxSpeedUnitsPerSecond)||
           !std::isfinite(it->maxAccelerationUnitsPerSecond2)||
           it->maxSpeedUnitsPerSecond<0||it->maxAccelerationUnitsPerSecond2<0)
            return fail("Each drive must have one finite, nonnegative, unique limit pair.");
        const double travel=std::abs(drive.target-drive.initial);
        if(!std::isfinite(travel))return fail("Motion travel overflow.");
        if(travel==0.0)continue;
        if(it->maxSpeedUnitsPerSecond==0.0||it->maxAccelerationUnitsPerSecond2==0.0)
            return fail("A moving drive requires positive speed and acceleration limits.");
        duration=std::max(duration,1.875*(travel/it->maxSpeedUnitsPerSecond));
        duration=std::max(duration,std::sqrt((10.0/std::sqrt(3.0))*
                                         (travel/it->maxAccelerationUnitsPerSecond2)));
    }
    // The motion sampler accepts a strictly positive duration. For a static
    // set of drives one millisecond is a neutral, explicit no-motion interval.
    duration=std::max(duration,0.001);
    if(!std::isfinite(duration)||duration>3600.0)
        return fail("The drive limits require a motion longer than one hour.");
    // Round upward by a tiny relative margin so tight boundary equality does
    // not fail the independent envelope check due to floating-point roundoff.
    duration=std::nextafter(duration,std::numeric_limits<double>::infinity());
    std::vector<DriveEnvelope> envelopes;
    if(!reviewDriveLimits(drives,duration,Progression::QuinticSmoothstep,
                          limits,&envelopes,&issue))
        return fail("Planned trajectory failed the independent drive envelope review.");
    PlannedQuinticMotion next;
    next.durationSeconds=duration;
    next.envelopes=std::move(envelopes);
    next.samples.reserve(static_cast<std::size_t>(sampleCount));
    for(int i=0;i<sampleCount;++i){
        // Calculate fraction from integer counters to include both endpoints.
        const double fraction=double(i)/double(sampleCount-1);
        PlannedMotionSample item;
        item.timeSeconds=fraction*duration;
        if(!interpolate(drives,fraction,Progression::QuinticSmoothstep,
                        &item.coordinates,&issue))
            return fail("Motion sampling failed after planning.");
        next.samples.push_back(std::move(item));
    }
    *output=std::move(next);
    if(error)error->clear();
    return true;
}
} // namespace cad::joint_motion

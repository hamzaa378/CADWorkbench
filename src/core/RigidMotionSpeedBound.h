#pragma once
// Rigid-body motion speed envelope for CAD clearance studies. The caller MUST
// independently establish global upper bounds on translation/angular speed
// during the entire interval. A sampled pose sequence alone cannot prove them.
// Reference AABBs enclose each *whole* solid in its initial rigid frame. A
// pure rotation about the supplied moving origin preserves its radius bound.
// No B-rep, joint limits, or collision-free certificate is inferred here.
#include "AdaptiveMotionScan.h"
#include "SimultaneousProximity.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace cad::motion {
struct RigidBodySpeedEnvelope {
    cad::proximity::Bounds referenceEnclosure;
    double originXmm=0,originYmm=0,originZmm=0;
    double maximumOriginTranslationSpeedMmPerSecond=0;
    double maximumAngularSpeedRadPerSecond=0;
};
struct RigidPairSpeedBound {
    double firstBodyRadiusMm=0;
    double secondBodyRadiusMm=0;
    double firstSurfaceSpeedBoundMmPerSecond=0;
    double secondSurfaceSpeedBoundMmPerSecond=0;
    double relativeSurfaceSpeedBoundMmPerSecond=0;
};
inline bool calculateRigidPairSpeedBound(const RigidBodySpeedEnvelope& first,
     const RigidBodySpeedEnvelope& second,RigidPairSpeedBound* result,
     std::string* error=nullptr) {
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!result)return fail("A speed-bound result is required.");
    const auto radius=[&](const RigidBodySpeedEnvelope& b,double* out)->bool{
        const auto& a=b.referenceEnclosure;
        if(!cad::proximity::valid(a))return false;
        const double values[]={a.x0,a.y0,a.z0,a.x1,a.y1,a.z1,
             b.originXmm,b.originYmm,b.originZmm,
             b.maximumOriginTranslationSpeedMmPerSecond,
             b.maximumAngularSpeedRadPerSecond};
        for(double value:values)if(!std::isfinite(value)||std::abs(value)>1e9)return false;
        if(b.maximumOriginTranslationSpeedMmPerSecond<0||
           b.maximumAngularSpeedRadPerSecond<0)return false;
        const double x=std::max(std::abs(a.x0-b.originXmm),std::abs(a.x1-b.originXmm));
        const double y=std::max(std::abs(a.y0-b.originYmm),std::abs(a.y1-b.originYmm));
        const double z=std::max(std::abs(a.z0-b.originZmm),std::abs(a.z1-b.originZmm));
        *out=std::hypot(x,y,z);
        return std::isfinite(*out);
    };
    RigidPairSpeedBound staged;
    if(!radius(first,&staged.firstBodyRadiusMm)||
       !radius(second,&staged.secondBodyRadiusMm))
        return fail("Rigid motion bounds need valid whole-body enclosures and finite physical limits.");
    staged.firstSurfaceSpeedBoundMmPerSecond=
        first.maximumOriginTranslationSpeedMmPerSecond+
        first.maximumAngularSpeedRadPerSecond*staged.firstBodyRadiusMm;
    staged.secondSurfaceSpeedBoundMmPerSecond=
        second.maximumOriginTranslationSpeedMmPerSecond+
        second.maximumAngularSpeedRadPerSecond*staged.secondBodyRadiusMm;
    staged.relativeSurfaceSpeedBoundMmPerSecond=
        staged.firstSurfaceSpeedBoundMmPerSecond+staged.secondSurfaceSpeedBoundMmPerSecond;
    if(!std::isfinite(staged.relativeSurfaceSpeedBoundMmPerSecond)||
       staged.relativeSurfaceSpeedBoundMmPerSecond>1e15)
        return fail("The derived motion speed bound exceeds the supported range.");
    *result=staged;
    if(error)error->clear();
    return true;
}
// Prepares the existing adaptive clearance scan with the derived speed limit.
// It does not magically validate the limits or the independent evaluation.
inline bool scanRigidPairWithSpeedBounds(const std::vector<double>& times,
     const Settings& requestedSettings,const RigidBodySpeedEnvelope& first,
     const RigidBodySpeedEnvelope& second,const Evaluate& evaluate,
     Result* report,RigidPairSpeedBound* proof,std::string* error=nullptr) {
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!report||!proof||!evaluate)
        return fail("Motion clearance review needs a callback and both outputs.");
    RigidPairSpeedBound stagedBound;
    if(!calculateRigidPairSpeedBound(first,second,&stagedBound,error))return false;
    Settings settings=requestedSettings;
    settings.provenRelativeSpeedBoundMmPerSecond=
        stagedBound.relativeSurfaceSpeedBoundMmPerSecond;
    Result stagedResult;
    if(!scanAdaptive(times,settings,evaluate,&stagedResult,error))return false;
    *proof=stagedBound;
    *report=std::move(stagedResult);
    if(error)error->clear();
    return true;
}
} // namespace cad::motion

#pragma once
// Exact speed bound of the stored, PIECEWISE-LINEAR joint-coordinate schedule.
// Revolute rates are in degrees/second; prismatic rates are in mm/second.
// This does not bound world-space solid speed, acceleration at keyframes, or
// prove continuous collision freedom. The CAD joint solver may amplify motion.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>
#include <utility>

namespace cad::motion {
struct RateKeyframe { double timeSeconds=0, coordinate=0; };
struct RateSegment {
    double beginSeconds=0, endSeconds=0, signedRatePerSecond=0;
};
struct RateAudit {
    std::vector<RateSegment> segments;
    double durationSeconds=0, totalAbsoluteTravel=0, maximumAbsoluteRatePerSecond=0;
    std::size_t maximumRateSegment=0;
    std::size_t directionReversals=0, velocityDiscontinuities=0;
    bool continuousCoordinate=true;
    bool boundedAcceleration=false; // piecewise linear velocities jump at knots
};
inline bool auditKeyframeRates(const std::vector<RateKeyframe>& keyframes,
        double lowerLimit,double upperLimit,RateAudit* report,std::string* error=nullptr) {
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!report||keyframes.size()<2||keyframes.size()>200||
       !std::isfinite(lowerLimit)||!std::isfinite(upperLimit)||
       lowerLimit>upperLimit||std::abs(lowerLimit)>1e9||std::abs(upperLimit)>1e9)
        return fail("Joint rate audit requires finite limits and 2-200 keyframes.");
    RateAudit next;next.segments.reserve(keyframes.size()-1);
    int priorDirection=0;
    for(std::size_t i=0;i<keyframes.size();++i) {
        const auto& key=keyframes[i];
        if(!std::isfinite(key.timeSeconds)||key.timeSeconds<0||key.timeSeconds>3600||
           !std::isfinite(key.coordinate)||key.coordinate<lowerLimit||key.coordinate>upperLimit||
           (i>0&&key.timeSeconds<=keyframes[i-1].timeSeconds))
            return fail("Joint keyframe has an invalid time, order, or travel coordinate.");
        if(!i)continue;
        const auto& previous=keyframes[i-1];
        const double interval=key.timeSeconds-previous.timeSeconds;
        const double change=key.coordinate-previous.coordinate;
        const double signedRate=change/interval;
        if(!std::isfinite(signedRate)||std::abs(signedRate)>1e12||
           !std::isfinite(next.totalAbsoluteTravel+std::abs(change)))
            return fail("Joint coordinate rate or total travel exceeds supported finite limits.");
        next.totalAbsoluteTravel+=std::abs(change);
        if(std::abs(signedRate)>next.maximumAbsoluteRatePerSecond){
            next.maximumAbsoluteRatePerSecond=std::abs(signedRate);
            next.maximumRateSegment=i-1;
        }
        if(!next.segments.empty()) {
            const double precedingRate=next.segments.back().signedRatePerSecond;
            // A velocity step means acceleration is unbounded in the ideal
            // piecewise-linear interpolation (unless rates are identical).
            if(signedRate!=precedingRate)++next.velocityDiscontinuities;
        }
        const int direction=change>0?1:(change<0?-1:0);
        if(direction&&priorDirection&&direction!=priorDirection)++next.directionReversals;
        if(direction)priorDirection=direction;
        next.segments.push_back({previous.timeSeconds,key.timeSeconds,signedRate});
    }
    next.durationSeconds=keyframes.back().timeSeconds-keyframes.front().timeSeconds;
    next.boundedAcceleration=next.velocityDiscontinuities==0;
    *report=std::move(next);
    if(error)error->clear();
    return true;
}
// The exact maximum joint-coordinate rate over a subinterval including its
// interior keyframe boundaries. Never extrapolates beyond the saved schedule.
inline bool maximumRateBetween(const RateAudit& audit,double begin,double end,
        double* maximum,std::string* error=nullptr) {
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!maximum||audit.segments.empty()||!std::isfinite(begin)||!std::isfinite(end)||
       begin< audit.segments.front().beginSeconds||
       end>audit.segments.back().endSeconds||end<=begin)
        return fail("Rate interval must lie strictly inside the saved motion schedule.");
    double candidate=0;
    for(const auto& segment:audit.segments) {
        if(segment.endSeconds>begin&&segment.beginSeconds<end)
            candidate=std::max(candidate,std::abs(segment.signedRatePerSecond));
    }
    *maximum=candidate;
    if(error)error->clear();
    return true;
}
} // namespace cad::motion

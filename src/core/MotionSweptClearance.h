#pragma once
// Continuous broad-phase clearance between TWO linearly translating axis-
// aligned bounding boxes. Exact for translating AABBs with fixed extents;
// not a B-rep narrow-phase, rotation, deforming-body or CCD certification.
// A transient collision between sampled poses must not be omitted.
#include "SimultaneousProximity.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace cad::proximity {
struct LinearSweepResult {
    double minimumGapMm=std::numeric_limits<double>::infinity();
    double fractionAtMinimum=0;
    bool everAabbContact=false;
    double firstContactFraction=0;
    double lastContactFraction=0;
    bool belowSpecifiedClearance=false;
};
namespace detail {
inline bool supported(const Bounds& b){
    return valid(b)&&
       std::max({std::abs(b.x0),std::abs(b.x1),std::abs(b.y0),
                 std::abs(b.y1),std::abs(b.z0),std::abs(b.z1)})<=1e9;
}
inline bool constantExtent(const Bounds& a,const Bounds& b){
    const double u[]={a.x1-a.x0,a.y1-a.y0,a.z1-a.z0};
    const double v[]={b.x1-b.x0,b.y1-b.y0,b.z1-b.z0};
    for(int i=0;i<3;++i){
        if(std::abs(u[i]-v[i])>1e-8*std::max({1.0,u[i],v[i]}))return false;
    }
    return true;
}
}
inline bool analyzeLinearBoxSweep(const Bounds& a0,const Bounds& a1,
        const Bounds& b0,const Bounds& b1,double clearanceMm,
        LinearSweepResult* result,std::string* error=nullptr){
    const auto fail=[&](const char* msg){if(error)*error=msg;return false;};
    if(!result||!std::isfinite(clearanceMm)||clearanceMm<0||clearanceMm>1e9||
       !detail::supported(a0)||!detail::supported(a1)||
       !detail::supported(b0)||!detail::supported(b1)||
       !detail::constantExtent(a0,a1)||!detail::constantExtent(b0,b1))
        return fail("Sweep needs finite ordered, bounded AABBs with constant extents and a valid clearance.");
    const double amin[]={a0.x0,a0.y0,a0.z0},amax[]={a0.x1,a0.y1,a0.z1};
    const double bmin[]={b0.x0,b0.y0,b0.z0},bmax[]={b0.x1,b0.y1,b0.z1};
    const double va[]={a1.x0-a0.x0,a1.y0-a0.y0,a1.z0-a0.z0};
    const double vb[]={b1.x0-b0.x0,b1.y0-b0.y0,b1.z0-b0.z0};
    const auto gapAt=[&](double t){
        double gaps[3];
        for(int axis=0;axis<3;++axis){
            const double drift=(vb[axis]-va[axis])*t;
            gaps[axis]=std::max({0.0,bmin[axis]-amax[axis]+drift,
                                      amin[axis]-bmax[axis]-drift});
        }
        return std::hypot(gaps[0],gaps[1],gaps[2]);
    };
    // Distance between linearly translating convex AABBs is convex in t.
    // Golden-section search finds a closest approach without assuming a
    // collision must occur at one of the supplied endpoints.
    double left=0.0,right=1.0;
    constexpr double invPhi=.6180339887498948482;
    double x1=right-invPhi*(right-left),x2=left+invPhi*(right-left);
    double f1=gapAt(x1),f2=gapAt(x2);
    for(int iteration=0;iteration<100;++iteration){
        if(f1<=f2){right=x2;x2=x1;f2=f1;x1=right-invPhi*(right-left);f1=gapAt(x1);}
        else{left=x1;x1=x2;f1=f2;x2=left+invPhi*(right-left);f2=gapAt(x2);}
    }
    LinearSweepResult staged;
    staged.fractionAtMinimum=(left+right)*.5;
    staged.minimumGapMm=gapAt(staged.fractionAtMinimum);
    if(gapAt(0)<staged.minimumGapMm){staged.fractionAtMinimum=0;staged.minimumGapMm=gapAt(0);}
    if(gapAt(1)<staged.minimumGapMm){staged.fractionAtMinimum=1;staged.minimumGapMm=gapAt(1);}
    // Slab intersection computes the EXACT contact interval for translation
    // (unlike the approximate closest-approach fraction above).
    double begin=0.0,end=1.0;
    for(int axis=0;axis<3;++axis){
        const double relative=(bmin[axis]+bmax[axis]-amin[axis]-amax[axis])*.5;
        const double extent=(amax[axis]-amin[axis]+bmax[axis]-bmin[axis])*.5;
        const double speed=vb[axis]-va[axis];
        if(speed==0){if(relative < -extent||relative > extent){begin=1;end=0;break;}}
        else {
            const double t0=(-extent-relative)/speed,t1=(extent-relative)/speed;
            begin=std::max(begin,std::min(t0,t1));
            end=std::min(end,std::max(t0,t1));
            if(begin>end)break;
        }
    }
    staged.everAabbContact=begin<=end;
    if(staged.everAabbContact){
        staged.firstContactFraction=begin;
        staged.lastContactFraction=end;
        staged.minimumGapMm=0;
        staged.fractionAtMinimum=(begin+end)*.5;
    }
    staged.belowSpecifiedClearance=staged.minimumGapMm<=clearanceMm;
    *result=staged;
    if(error)error->clear();
    return true;
}
struct SweptPairHistory {
    std::string firstId,secondId;
    double minimumGapMm=std::numeric_limits<double>::infinity();
    std::size_t closestSegment=0;
    double closestAbsoluteTime=0;
    std::size_t riskySegments=0;
    std::size_t contactSegments=0;
};
class SweptProximityTracker {
public:
    explicit SweptProximityTracker(double clearanceMm):clearanceMm_(clearanceMm){}
    bool observe(double time,const std::vector<BodyBox>& bodies,std::string* error=nullptr){
        const auto fail=[&](const char* msg){if(error)*error=msg;return false;};
        if(!std::isfinite(clearanceMm_)||clearanceMm_<0||clearanceMm_>1e9||
           !std::isfinite(time)||bodies.empty()||bodies.size()>20||sampleCount_>=100000)
            return fail("Invalid sweep threshold, timestamp or body inventory.");
        std::vector<std::string> ids;
        for(const auto& b:bodies){
            if(b.bodyId.empty()||!detail::supported(b.bounds)||
               std::find(ids.begin(),ids.end(),b.bodyId)!=ids.end())
                return fail("Invalid or duplicate assembly body in sampled pose.");
            ids.push_back(b.bodyId);
        }
        if(sampleCount_&&(!(time>previousTime_)||!std::isfinite(time-previousTime_)||ids!=ids_))
            return fail("Sweep timestamps must increase and body identities/order must not change.");
        auto staged=pairs_;
        if(sampleCount_==0){
            for(std::size_t i=0;i<bodies.size();++i)
                for(std::size_t j=i+1;j<bodies.size();++j)
                    staged.push_back({bodies[i].bodyId,bodies[j].bodyId});
        }else{
            std::size_t index=0;
            for(std::size_t i=0;i<bodies.size();++i){
                for(std::size_t j=i+1;j<bodies.size();++j){
                    LinearSweepResult result;
                    if(!analyzeLinearBoxSweep(previous_[i].bounds,bodies[i].bounds,
                            previous_[j].bounds,bodies[j].bounds,clearanceMm_,&result,error))
                        return false;
                    auto& pair=staged[index++];
                    if(result.belowSpecifiedClearance)++pair.riskySegments;
                    if(result.everAabbContact)++pair.contactSegments;
                    if(result.minimumGapMm<pair.minimumGapMm){
                        pair.minimumGapMm=result.minimumGapMm;
                        pair.closestSegment=sampleCount_-1;
                        pair.closestAbsoluteTime=previousTime_+
                            (time-previousTime_)*result.fractionAtMinimum;
                    }
                }
            }
        }
        previous_=bodies;ids_=std::move(ids);pairs_=std::move(staged);
        previousTime_=time;++sampleCount_;
        if(error)error->clear();
        return true;
    }
    std::size_t sampleCount() const noexcept{return sampleCount_;}
    const std::vector<SweptPairHistory>& pairs() const noexcept{return pairs_;}
private:
    double clearanceMm_=0;
    double previousTime_=0;
    std::size_t sampleCount_=0;
    std::vector<std::string> ids_;
    std::vector<BodyBox> previous_;
    std::vector<SweptPairHistory> pairs_;
};
// New in Sprint 11: keep a sweeping assembly's physical pair identities
// consistent across CAD imports/serialization that reorder body occurrences.
// The underlying swept solver still enforces strict, increasing timestamps,
// fixed AABB extent, finite values and atomic rejection on invalid input.
class OrderInvariantSweptTracker {
public:
    explicit OrderInvariantSweptTracker(double clearanceMm) : tracker_(clearanceMm) {}
    bool observe(double time,const std::vector<BodyBox>& bodies,std::string* error=nullptr) {
        std::vector<BodyBox> canonical=bodies;
        std::sort(canonical.begin(),canonical.end(),[](const BodyBox& a,const BodyBox& b){
            return a.bodyId<b.bodyId;
        });
        return tracker_.observe(time,canonical,error);
    }
    std::size_t sampleCount() const noexcept {return tracker_.sampleCount();}
    const std::vector<SweptPairHistory>& pairs() const noexcept {return tracker_.pairs();}
private:
    SweptProximityTracker tracker_;
};
} // namespace cad::proximity

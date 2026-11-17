#pragma once
// Same-timestamp articulated-body broad-phase proximity tracker. AABBs are
// conservative for the sampled solids, but zero AABB distance is NOT proof
// of 3D B-rep contact. The tracker never compares boxes from different times.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace cad::proximity {
struct Bounds {double x0,y0,z0,x1,y1,z1;};
struct BodyBox {std::string bodyId;Bounds bounds;};
struct PairHistory {
    std::string firstId,secondId;
    std::size_t simultaneousOverlapSamples=0;
    std::size_t closestSample=0;
    double minimumAabbGapMm=std::numeric_limits<double>::infinity();
};
inline bool valid(const Bounds& b){
    return std::isfinite(b.x0)&&std::isfinite(b.y0)&&std::isfinite(b.z0)&&
           std::isfinite(b.x1)&&std::isfinite(b.y1)&&std::isfinite(b.z1)&&
           b.x1>=b.x0&&b.y1>=b.y0&&b.z1>=b.z0;
}
inline double aabbGap(const Bounds& a,const Bounds& b){
    const double dx=std::max({0.0,b.x0-a.x1,a.x0-b.x1});
    const double dy=std::max({0.0,b.y0-a.y1,a.y0-b.y1});
    const double dz=std::max({0.0,b.z0-a.z1,a.z0-b.z1});
    return std::hypot(dx,dy,dz);
}
class Tracker {
public:
    bool observe(const std::vector<BodyBox>& boxes,std::string* error=nullptr){
        const auto fail=[&](const char* msg){if(error)*error=msg;return false;};
        if(boxes.empty()||boxes.size()>20)return fail("Expected 1-20 moving bodies per sampled pose.");
        std::vector<std::string> newIds;
        for(const auto& body:boxes){
            if(body.bodyId.empty()||!valid(body.bounds))return fail("Invalid body identity or bounding geometry.");
            if(std::find(newIds.begin(),newIds.end(),body.bodyId)!=newIds.end())
                return fail("Duplicate moving-body identity in a sampled pose.");
            newIds.push_back(body.bodyId);
        }
        if(samples_!=0&&newIds!=ids_)return fail("Moving-body identities or ordering changed between poses.");
        std::vector<PairHistory> staged=pairs_;
        if(samples_==0){
            for(std::size_t i=0;i<boxes.size();++i)
                for(std::size_t j=i+1;j<boxes.size();++j)
                    staged.push_back({boxes[i].bodyId,boxes[j].bodyId});
        }
        std::size_t pairIndex=0;
        for(std::size_t i=0;i<boxes.size();++i)for(std::size_t j=i+1;j<boxes.size();++j){
            const double gap=aabbGap(boxes[i].bounds,boxes[j].bounds);
            if(!std::isfinite(gap))return fail("Sampled AABB separation overflow.");
            PairHistory& pair=staged[pairIndex++];
            if(gap<=1e-9)++pair.simultaneousOverlapSamples;
            if(gap<pair.minimumAabbGapMm){
                pair.minimumAabbGapMm=gap;pair.closestSample=samples_;
            }
        }
        pairs_=std::move(staged);ids_=std::move(newIds);++samples_;
        if(error)error->clear();
        return true;
    }
    const std::vector<PairHistory>& pairs() const {return pairs_;}
    std::size_t sampleCount() const {return samples_;}
private:
    std::size_t samples_=0;
    std::vector<std::string> ids_;
    std::vector<PairHistory> pairs_;
};
// Assembly imports and staged regenerations can enumerate the same bodies in
// different orders. Normalize presentation order without weakening identity,
// AABB validation or atomic rejected-snapshot semantics in Tracker.
class OrderInvariantTracker {
public:
    bool observe(const std::vector<BodyBox>& boxes,std::string* error=nullptr) {
        std::vector<BodyBox> canonical=boxes;
        std::sort(canonical.begin(),canonical.end(),[](const BodyBox& a,const BodyBox& b){
            return a.bodyId<b.bodyId;
        });
        return tracker_.observe(canonical,error);
    }
    const std::vector<PairHistory>& pairs() const {return tracker_.pairs();}
    std::size_t sampleCount() const {return tracker_.sampleCount();}
private:
    Tracker tracker_;
};
} // namespace cad::proximity

#pragma once
// Kernel-independent consistency gate for persistent face/edge identifiers
// crossing multiple Boolean split/merge branches. Every route is resolved
// against the *current* OCCT builders by the caller. This gate independently
// verifies identical FINAL topology descriptors and transformed design points;
// approximate-looking geometry is not evidence of shared identity.
#include "BranchReferenceAgreement.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace cad::lineage {
struct ReferenceRouteEvidence {
    DesignPoint witness{};
    std::map<std::string,double> descriptor;
};
// Strong output guarantee: diagnostic and verdict are calculated without
// changing any committed feature descriptor; empty/oversized routes fail.
inline bool agreeReferenceRoutes(const std::vector<ReferenceRouteEvidence>& routes,
                                  std::string* why=nullptr,
                                  double witnessToleranceMm=1e-4){
    const auto fail=[&](const char* message){if(why)*why=message;return false;};
    if(routes.empty()||routes.size()>32||!std::isfinite(witnessToleranceMm)||
       witnessToleranceMm<=0.0)
        return fail("Persistent topology consensus requires 1-32 valid routes and positive tolerance.");
    const auto& first=routes.front();
    if(first.descriptor.empty()||first.descriptor.size()>256)
        return fail("Missing or unbounded persistent topology descriptor.");
    const auto valid=[](const ReferenceRouteEvidence& evidence){
        for(const double v:evidence.witness)if(!std::isfinite(v))return false;
        if(evidence.descriptor.empty()||evidence.descriptor.size()>256)return false;
        for(const auto& [key,value]:evidence.descriptor)
            if(key.empty()||key.size()>128||!std::isfinite(value))return false;
        return true;
    };
    if(!valid(first))return fail("Non-finite or malformed persistent topology witness.");
    for(std::size_t i=1;i<routes.size();++i){
        const auto& item=routes[i];
        if(!valid(item))return fail("Non-finite or malformed persistent topology witness.");
        if(!samePhysicalWitness(first.witness,item.witness,witnessToleranceMm))
            return fail("Boolean merge paths disagree on the original physical design location.");
        if(first.descriptor.size()!=item.descriptor.size())
            return fail("Boolean merge paths disagree on descriptor field count.");
        for(const auto& [key,value]:first.descriptor){
            const auto found=item.descriptor.find(key);
            if(found==item.descriptor.end())
                return fail("Boolean merge paths disagree on descriptor field names.");
            const double other=found->second;
            // Discrete topological indices, flags and feature-type tags must
            // be *identical*. Never allow a tolerance to mask wrong edge ID.
            const bool discrete=key=="edge_index"||key=="face_index"||
                key=="edge_curve_kind"||key=="edge_anchor_version"||
                key=="curved_type";
            if(discrete ? value!=other :
               std::abs(value-other)>1e-6*std::max({1.0,std::abs(value),std::abs(other)}))
                return fail("Boolean merge paths resolve different face/edge topology or placement.");
        }
    }
    // Pairwise agreement is essential. Testing only against the first route
    // can accept two opposite descendants whose separation exceeds the
    // engineering tolerance (A=0, B=+0.9e-4, C=-0.9e-4 mm). Neither a
    // split/merge nor path-ordering may make that ambiguous result current.
    for(std::size_t i=0;i<routes.size();++i){
        for(std::size_t j=i+1;j<routes.size();++j){
            if(!samePhysicalWitness(routes[i].witness,routes[j].witness,witnessToleranceMm))
                return fail("Boolean ancestry route witnesses are not pairwise consistent.");
            for(const auto& [key,value]:routes[i].descriptor){
                const auto it=routes[j].descriptor.find(key);
                if(it==routes[j].descriptor.end())
                    return fail("Boolean ancestry route descriptors have different fields.");
                const double other=it->second;
                const bool discrete=key=="edge_index"||key=="face_index"||
                    key=="edge_curve_kind"||key=="edge_anchor_version"||key=="curved_type";
                if(discrete?value!=other:
                   std::abs(value-other)>1e-6*std::max({1.0,std::abs(value),std::abs(other)}))
                    return fail("Boolean ancestry route descriptors are not pairwise consistent.");
            }
        }
    }
    if(why)why->clear();
    return true;
}
} // namespace cad::lineage

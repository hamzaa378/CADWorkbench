#pragma once
// Shop constrained WORST-CASE dimensional tolerance allocation, millimetres.
// A deterministic, cost-weighted greedy what-if planning service. The shop
// floor is a user-entered manufacturing constraint, never inferred capability.
// Cost is relative; this is NOT a mathematical global cost optimum or a
// process-capability, ASME/ISO fit, or drawing release certification.
#include "AssemblyToleranceBudget.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace cad::workshop {
struct ShopToleranceLimit {
    double minimumDeviationScale=0.0; // lower limit the shop claims is achievable
    double tighteningCostWeight=1.0; // relative weight per unit of (1-scale)
};
struct ShopToleranceEdit {
    std::size_t termIndex=0;
    double oldScale=1.0;
    double newScale=1.0;
};
struct ShopToleranceAllocation {
    bool shopFloorFeasible=false;
    bool fitsWorstCase=false;
    bool conservativeFallbackUsed=false;
    double relativeTighteningCost=0.0;
    AssemblyToleranceBudget before;
    AssemblyToleranceBudget floorBudget;
    AssemblyToleranceBudget after;
    std::vector<double> scales;
    std::vector<ToleranceTerm> proposedTerms;
    std::vector<ShopToleranceEdit> edits;
};

inline bool allocateShopTolerances(const std::vector<ToleranceTerm>& chain,
        const std::vector<ShopToleranceLimit>& shop,
        double requiredMinimumMm,double allowedMaximumMm,
        ShopToleranceAllocation* output,std::string* error=nullptr) {
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!output||chain.empty()||chain.size()!=shop.size()||chain.size()>1000||
       !std::isfinite(requiredMinimumMm)||!std::isfinite(allowedMaximumMm)||
       requiredMinimumMm>allowedMaximumMm)
        return fail("Tolerance allocation needs aligned dimensions, ordered fit limits and an output.");
    ShopToleranceAllocation staged;
    if(!assessAssemblyToleranceBudget(chain,requiredMinimumMm,allowedMaximumMm,&staged.before,error))return false;
    std::vector<double> lower(chain.size()),upper(chain.size());
    std::vector<ToleranceTerm> floorTerms=chain;
    for(std::size_t i=0;i<chain.size();++i){
        const auto& t=chain[i];
        if(!std::isfinite(t.lowerDeviationMm)||!std::isfinite(t.upperDeviationMm)||
           t.lowerDeviationMm>0||t.upperDeviationMm<0||
           !std::isfinite(shop[i].minimumDeviationScale)||
           shop[i].minimumDeviationScale<0||shop[i].minimumDeviationScale>1||
           !std::isfinite(shop[i].tighteningCostWeight)||shop[i].tighteningCostWeight<=0||
           shop[i].tighteningCostWeight>1e12)
            return fail("Tolerances must straddle nominal and shop scales/costs must be valid.");
        lower[i]=t.direction==1?-t.lowerDeviationMm:t.upperDeviationMm;
        upper[i]=t.direction==1?t.upperDeviationMm:-t.lowerDeviationMm;
        if(!std::isfinite(lower[i])||!std::isfinite(upper[i]))
            return fail("Tolerance contribution overflowed.");
        floorTerms[i].lowerDeviationMm*=shop[i].minimumDeviationScale;
        floorTerms[i].upperDeviationMm*=shop[i].minimumDeviationScale;
    }
    if(!assessAssemblyToleranceBudget(floorTerms,requiredMinimumMm,allowedMaximumMm,
                                      &staged.floorBudget,error))return false;
    staged.shopFloorFeasible=staged.floorBudget.decision==FitDecision::FeasibleWorstCase;
    staged.scales.assign(chain.size(),1.0);
    staged.proposedTerms=chain;
    if(staged.shopFloorFeasible){
        // Each unit of scale reduction makes the conservative lower clearance
        // larger by lower[i] and the conservative upper clearance smaller by
        // upper[i]. Both constraints can therefore be improved monotonically.
        double lowNeed=std::max(0.0,-staged.before.minimumMarginMm);
        double highNeed=std::max(0.0,-staged.before.maximumMarginMm);
        constexpr double epsilon=1e-12;
        for(std::size_t step=0;step<2*chain.size()+2 &&
              (lowNeed>epsilon||highNeed>epsilon);++step){
            std::size_t chosen=chain.size();
            double bestScore=-1.0;
            for(std::size_t i=0;i<chain.size();++i){
                const double capacity=staged.scales[i]-shop[i].minimumDeviationScale;
                if(capacity<=epsilon)continue;
                const double lowHelp=lowNeed>epsilon ? std::min(lowNeed,lower[i]*capacity) / lowNeed : 0.0;
                const double highHelp=highNeed>epsilon ? std::min(highNeed,upper[i]*capacity) / highNeed : 0.0;
                const double score=(lowHelp+highHelp)/(capacity*shop[i].tighteningCostWeight);
                if(std::isfinite(score)&&score>bestScore+1e-15){bestScore=score;chosen=i;}
            }
            if(chosen==chain.size()||bestScore<=0)break;
            const double capacity=staged.scales[chosen]-shop[chosen].minimumDeviationScale;
            double delta=capacity;
            if(lowNeed>epsilon&&lower[chosen]>0)delta=std::min(delta,lowNeed/lower[chosen]);
            if(highNeed>epsilon&&upper[chosen]>0)delta=std::min(delta,highNeed/upper[chosen]);
            if(delta<=epsilon)delta=capacity; // prevent stalled fractional iterations
            const double old=staged.scales[chosen];
            staged.scales[chosen]=std::max(shop[chosen].minimumDeviationScale,old-delta);
            const double reduction=old-staged.scales[chosen];
            lowNeed=std::max(0.0,lowNeed-lower[chosen]*reduction);
            highNeed=std::max(0.0,highNeed-upper[chosen]*reduction);
            staged.edits.push_back({chosen,old,staged.scales[chosen]});
        }
    } else {
        // Even the user-supplied shop floor cannot guarantee the requested fit.
        // Return the floor case explicitly for review without claiming success.
        for(std::size_t i=0;i<chain.size();++i){
            staged.scales[i]=shop[i].minimumDeviationScale;
            if(staged.scales[i]<1.0)
                staged.edits.push_back({i,1.0,staged.scales[i]});
        }
    }
    const auto materialize=[&](){
        for(std::size_t i=0;i<chain.size();++i){
            staged.proposedTerms[i].lowerDeviationMm=chain[i].lowerDeviationMm*staged.scales[i];
            staged.proposedTerms[i].upperDeviationMm=chain[i].upperDeviationMm*staged.scales[i];
        }
        return assessAssemblyToleranceBudget(staged.proposedTerms,requiredMinimumMm,allowedMaximumMm,
                                             &staged.after,error);
    };
    if(!materialize())return false;
    if(staged.shopFloorFeasible&&staged.after.decision!=FitDecision::FeasibleWorstCase){
        // Rounding or a greedy stall cannot make a feasible floor claim false.
        // Conservative fallback is a truthful, verified shop floor candidate.
        staged.conservativeFallbackUsed=true;
        staged.edits.clear();
        for(std::size_t i=0;i<chain.size();++i){
            staged.scales[i]=shop[i].minimumDeviationScale;
            if(staged.scales[i]<1.0)
                staged.edits.push_back({i,1.0,staged.scales[i]});
        }
        if(!materialize())return false;
    }
    staged.fitsWorstCase=staged.after.decision==FitDecision::FeasibleWorstCase;
    for(std::size_t i=0;i<chain.size();++i){
        staged.relativeTighteningCost+=(1.0-staged.scales[i])*shop[i].tighteningCostWeight;
        if(!std::isfinite(staged.relativeTighteningCost))return fail("Relative tightening cost overflowed.");
    }
    *output=std::move(staged);
    if(error)error->clear();
    return true;
}
} // namespace cad::workshop

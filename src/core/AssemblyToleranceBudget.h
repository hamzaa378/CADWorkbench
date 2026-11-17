#pragma once
// Manufacturing fit acceptance based on an explicit linear dimensional chain.
// Conservative worst-case fit bounds; independent RSS is advisory only and must
// never be used as a substitute for the guaranteed worst-case decision.
#include "WorkshopEngineering.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace cad::workshop {
enum class FitDecision { Rejected, FeasibleWorstCase, TooTight, TooLoose, BothLimitsViolated };
struct ToleranceContribution {
    std::string label;
    double effectOnWorstCaseWidthMm=0.0;
    double lowerSideEffectMm=0.0; // contribution to downward deviation from nominal
    double upperSideEffectMm=0.0; // contribution to upward deviation from nominal
};
struct AssemblyToleranceBudget {
    double minimumRequiredMm=0.0, maximumAllowedMm=0.0;
    double minimumMarginMm=0.0, maximumMarginMm=0.0;
    double targetCenterMm=0.0;
    double totalWorstCaseWidthMm=0.0;
    double rssAdvisoryHalfWidthMm=0.0;
    FitDecision decision=FitDecision::Rejected;
    std::vector<ToleranceContribution> rankedContributors;
};
inline const char* fitDecisionName(FitDecision status) noexcept {
    switch(status) {
    case FitDecision::Rejected: return "REJECTED";
    case FitDecision::FeasibleWorstCase: return "WORST_CASE_FEASIBLE";
    case FitDecision::TooTight: return "BELOW_MINIMUM";
    case FitDecision::TooLoose: return "ABOVE_MAXIMUM";
    case FitDecision::BothLimitsViolated: return "BOTH_LIMITS_VIOLATED";
    }
    return "UNKNOWN";
}

inline bool assessAssemblyToleranceBudget(const std::vector<ToleranceTerm>& chain,
                                          double requiredMinimumMm,
                                          double allowedMaximumMm,
                                          AssemblyToleranceBudget* output,
                                          std::string* error=nullptr) {
    const auto fail=[&](const char* msg){if(error)*error=msg;return false;};
    if(!output) return fail("A tolerance budget destination is required.");
    if(!std::isfinite(requiredMinimumMm)||!std::isfinite(allowedMaximumMm)||
       requiredMinimumMm>allowedMaximumMm)
        return fail("Required fit bounds must be finite, ordered millimetre values.");
    ToleranceResult stack;
    std::string stackError;
    if(!calculateToleranceStack(chain,&stack,&stackError)){
        if(error)*error=stackError;
        return false;
    }
    AssemblyToleranceBudget next;
    next.minimumRequiredMm=requiredMinimumMm;
    next.maximumAllowedMm=allowedMaximumMm;
    next.minimumMarginMm=stack.worstMinMm-requiredMinimumMm;
    next.maximumMarginMm=allowedMaximumMm-stack.worstMaxMm;
    next.targetCenterMm=std::midpoint(requiredMinimumMm,allowedMaximumMm);
    next.totalWorstCaseWidthMm=stack.worstMaxMm-stack.worstMinMm;
    next.rssAdvisoryHalfWidthMm=stack.rssHalfWidthMm;
    if(!std::isfinite(next.minimumMarginMm)||!std::isfinite(next.maximumMarginMm)||
       !std::isfinite(next.totalWorstCaseWidthMm)||!std::isfinite(next.targetCenterMm))
        return fail("Tolerance budget arithmetic overflowed.");
    const bool low=next.minimumMarginMm<0.0;
    const bool high=next.maximumMarginMm<0.0;
    next.decision=low?(high?FitDecision::BothLimitsViolated:FitDecision::TooTight):
                       (high?FitDecision::TooLoose:FitDecision::FeasibleWorstCase);
    next.rankedContributors.reserve(chain.size());
    for(const auto& term:chain){
        // A subtracting term reverses the direction of the lower/upper errors.
        const double down=term.direction==1?-term.lowerDeviationMm:term.upperDeviationMm;
        const double up=term.direction==1?term.upperDeviationMm:-term.lowerDeviationMm;
        const double width=term.upperDeviationMm-term.lowerDeviationMm;
        if(!std::isfinite(width)||!std::isfinite(down)||!std::isfinite(up))
            return fail("Individual tolerance contribution overflowed.");
        next.rankedContributors.push_back({term.label,width,down,up});
    }
    std::stable_sort(next.rankedContributors.begin(),next.rankedContributors.end(),
                     [](const auto& a,const auto& b){return a.effectOnWorstCaseWidthMm>b.effectOnWorstCaseWidthMm;});
    *output=std::move(next);
    if(error)error->clear();
    return true;
}
} // namespace cad::workshop

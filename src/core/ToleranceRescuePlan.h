#pragma once
// Preliminary tolerance-reduction WHAT-IF tool. This is not proof that a
// supplier can manufacture the proposed limits or that RSS is appropriate.
// Uniformly scales every signed deviation toward its existing nominal;
// never shifts a nominal dimension or silently changes a tolerance direction.
#include "AssemblyToleranceBudget.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace cad::workshop {
struct ToleranceRescuePlan {
    bool feasibleWithinShopFloor=false;
    bool nominalOutsideEnvelope=false;
    double originalNominalMm=0.0;
    double proposedScale=1.0; // 1 = unchanged limits; 0 = zero variation.
    double requiredMaximumScale=1.0;
    double minimumShopScale=0.0;
    AssemblyToleranceBudget before;
    AssemblyToleranceBudget after;
    std::vector<ToleranceTerm> proposedTerms;
};

inline bool planUniformToleranceRescue(const std::vector<ToleranceTerm>& terms,
    double requiredMinimumMm,double allowedMaximumMm,double minimumShopScale,
    ToleranceRescuePlan* output,std::string* error=nullptr) {
    const auto fail=[&](const char* reason){if(error)*error=reason;return false;};
    if(!output||!std::isfinite(minimumShopScale)||minimumShopScale<0||minimumShopScale>1||
       !std::isfinite(requiredMinimumMm)||!std::isfinite(allowedMaximumMm)||
       requiredMinimumMm>allowedMaximumMm)
        return fail("Rescue requires valid fit limits, shop scale in [0,1], and an output.");
    // Scaling both sides is monotonic only when every tolerance range
    // contains its nominal (zero signed deviation).
    for(const auto& t:terms)
        if(!std::isfinite(t.lowerDeviationMm)||!std::isfinite(t.upperDeviationMm)||
           t.lowerDeviationMm>0||t.upperDeviationMm<0)
            return fail("Each deviation interval must straddle the existing nominal.");
    AssemblyToleranceBudget before;
    if(!assessAssemblyToleranceBudget(terms,requiredMinimumMm,allowedMaximumMm,&before,error))return false;
    ToleranceResult stack;
    if(!calculateToleranceStack(terms,&stack,error))return false;
    ToleranceRescuePlan candidate;
    candidate.before=before;
    candidate.originalNominalMm=stack.nominalMm;
    candidate.minimumShopScale=minimumShopScale;
    candidate.nominalOutsideEnvelope=stack.nominalMm<requiredMinimumMm-1e-10 ||
                                      stack.nominalMm>allowedMaximumMm+1e-10;
    candidate.requiredMaximumScale=1.0;
    if(!candidate.nominalOutsideEnvelope){
        const double lowSpread=stack.nominalMm-stack.worstMinMm;
        const double highSpread=stack.worstMaxMm-stack.nominalMm;
        if(lowSpread>1e-12)
            candidate.requiredMaximumScale=std::min(candidate.requiredMaximumScale,
                (stack.nominalMm-requiredMinimumMm)/lowSpread);
        if(highSpread>1e-12)
            candidate.requiredMaximumScale=std::min(candidate.requiredMaximumScale,
                (allowedMaximumMm-stack.nominalMm)/highSpread);
        candidate.requiredMaximumScale=std::clamp(candidate.requiredMaximumScale,0.0,1.0);
        // Never propose tighter limits than the explicitly supplied shop floor.
        candidate.proposedScale=std::max(minimumShopScale,candidate.requiredMaximumScale);
    }
    candidate.proposedTerms=terms;
    for(auto& t:candidate.proposedTerms){
        t.lowerDeviationMm*=candidate.proposedScale;
        t.upperDeviationMm*=candidate.proposedScale;
    }
    if(!assessAssemblyToleranceBudget(candidate.proposedTerms,requiredMinimumMm,allowedMaximumMm,
                                      &candidate.after,error))return false;
    candidate.feasibleWithinShopFloor=!candidate.nominalOutsideEnvelope &&
        candidate.after.minimumMarginMm>=-1e-9 && candidate.after.maximumMarginMm>=-1e-9;
    *output=std::move(candidate);
    if(error)error->clear();
    return true;
}
} // namespace cad::workshop

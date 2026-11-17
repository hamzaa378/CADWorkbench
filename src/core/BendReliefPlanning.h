#pragma once
#include <cmath>
#include <string>
#include <vector>
#include "SheetMetalBendSequenceAudit.h"

// Preflight a user-designed, flat-blank end-relief against USER-SUPPLIED
// fabrication limits. Does not cut the B-rep, derive tooling requirements,
// certify foldability, or invent a material standard.
namespace cad::sheet {
struct ReliefPolicy {
    double minimumWidthToThickness=1.0;
    double minimumDepthBeyondInsideRadiusMm=0.0;
};
struct ReliefDecision {
    std::size_t bendNumber=0;
    double requestedWidthMm=0,requestedDepthMm=0;
    double minimumWidthMm=0,minimumDepthMm=0;
    bool widthPass=false,depthPass=false;
};
inline bool reviewEndReliefs(const std::vector<BendInput>& bends,double thicknessMm,
        double flatSheetWidthMm,double requestedWidthMm,double requestedDepthMm,
        ReliefPolicy policy,std::vector<ReliefDecision>* output,std::string* error=nullptr){
    const auto fail=[&](const char* msg){if(error)*error=msg;return false;};
    if(!output||bends.empty()||bends.size()>50)return fail("Expected 1-50 bends and an output report.");
    for(double value:{thicknessMm,flatSheetWidthMm,requestedWidthMm,requestedDepthMm,
            policy.minimumWidthToThickness,policy.minimumDepthBeyondInsideRadiusMm})
        if(!std::isfinite(value)||std::abs(value)>1e9)return fail("End relief dimensions must be finite and bounded.");
    if(thicknessMm<=0||flatSheetWidthMm<=0||requestedWidthMm<=0||requestedDepthMm<=0||
       policy.minimumWidthToThickness<0||policy.minimumDepthBeyondInsideRadiusMm<0||
       requestedWidthMm*2>=flatSheetWidthMm)
        return fail("End relief width/depth or remaining strip width is invalid.");
    std::vector<ReliefDecision> staged;staged.reserve(bends.size());
    for(std::size_t i=0;i<bends.size();++i){
        const auto& bend=bends[i];
        if(!std::isfinite(bend.insideRadiusMm)||bend.insideRadiusMm<=0||
           !std::isfinite(bend.signedDegrees)||std::abs(bend.signedDegrees)<15||
           std::abs(bend.signedDegrees)>165)
            return fail("Relief review requires valid individual bends.");
        const double minWidth=thicknessMm*policy.minimumWidthToThickness;
        const double minDepth=bend.insideRadiusMm+policy.minimumDepthBeyondInsideRadiusMm;
        if(!std::isfinite(minWidth)||!std::isfinite(minDepth))return fail("Relief limit overflow.");
        staged.push_back({i+1,requestedWidthMm,requestedDepthMm,minWidth,minDepth,
            requestedWidthMm+1e-9>=minWidth,requestedDepthMm+1e-9>=minDepth});
    }
    *output=std::move(staged);
    if(error)error->clear();return true;
}
} // namespace cad::sheet

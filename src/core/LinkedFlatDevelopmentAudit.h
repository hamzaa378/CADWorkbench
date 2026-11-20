#pragma once
// Independent, standard-library-only consistency check between a saved folded
// definition and the DXF/SVG neutral development. This checks dimensional
// bookkeeping, not 3D bend tooling, springback or collision/strain.
#include "SheetMetalBendSequenceAudit.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
namespace cad::sheet {
struct LinkedFlatAudit {
    double nominalDevelopedLengthMm=0;
    double minimumBendCenterSpacingMm=0;
    std::vector<double> bendStationsMm;
    std::size_t reversalCount=0;
    bool releaseApproved=false;
};
inline bool auditLinkedFlatDevelopment(const std::vector<double>& flanges,
    double widthMm,double thicknessMm,const std::vector<BendInput>& bends,
    double publishedLengthMm,double publishedWidthMm,
    const std::vector<double>& publishedBendStationsMm,
    const std::vector<double>& publishedAllowancesMm,
    LinkedFlatAudit* out,std::string* error=nullptr) {
    const auto fail=[&](const char* reason){if(error)*error=reason;return false;};
    if(!out||!std::isfinite(widthMm)||widthMm<=0||widthMm>1e6||
       !std::isfinite(publishedLengthMm)||!std::isfinite(publishedWidthMm)||
       !std::isfinite(thicknessMm)||publishedLengthMm<=0||publishedWidthMm<=0)
        return fail("Finite positive physical flat-pattern dimensions are required.");
    if(publishedBendStationsMm.size()!=bends.size()||
       publishedAllowancesMm.size()!=bends.size())
        return fail("Saved drawing bend inventory differs from folded definition.");
    BendSequenceReport expected;
    if(!inspectBendSequence(flanges,thicknessMm,bends,&expected,error))return false;
    const auto match=[](double a,double b){
        return std::isfinite(a)&&std::isfinite(b)&&
               std::abs(a-b)<=1e-7*std::max({1.0,std::abs(a),std::abs(b)});
    };
    if(!match(publishedLengthMm,expected.developedLengthMm)||
       !match(publishedWidthMm,widthMm))
        return fail("Flat blank dimensions disagree with source neutral development.");
    LinkedFlatAudit staged;staged.nominalDevelopedLengthMm=expected.developedLengthMm;
    staged.reversalCount=expected.reverseDirectionCount;
    for(std::size_t i=0;i<bends.size();++i) {
        const auto& s=expected.stations[i];
        if(!match(publishedBendStationsMm[i],s.stationMm)||
           !match(publishedAllowancesMm[i],s.neutralAllowanceMm))
            return fail("Bend stations or allowances disagree with saved folded definition.");
        staged.bendStationsMm.push_back(s.stationMm);
    }
    BendStationSpacingCheck spacing;
    if(!checkBendStationSpacing(expected,0.0,&spacing,error))return false;
    staged.minimumBendCenterSpacingMm=spacing.actualMinimumMm;
    *out=std::move(staged);
    if(error)error->clear();return true;
}
} // namespace cad::sheet

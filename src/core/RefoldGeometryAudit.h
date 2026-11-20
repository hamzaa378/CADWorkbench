#pragma once

// An independent geometrical conservation check for CADWorkbench's faceted
// 2-D folded sheet cross-section. Does not purport to invert arbitrary B-reps,
// model corner relief in three dimensions, or certify a manufactured bend.
// The folded model uses the mid-thickness centreline (K = 0.5); a shop K-factor
// is a separate development assumption and is NOT a geometrical invariant.
#include "FoldedContourTopology.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace cad::refold {
struct Bend { double signedDegrees=0, insideRadiusMm=0; };
struct Audit {
    double exactMidSurfaceDevelopedLengthMm=0;
    double facetedCrossSectionAreaMm2=0;
    double exactCrossSectionAreaMm2=0;
    double relativeAreaError=0;
    double predictedSolidVolumeMm3=0;
    int bendCount=0;
    int reversedBendCount=0;
    bool closedSimpleContour=false;
};
inline bool inspect(const std::vector<double>& straightFlangesMm,
        double thicknessMm,double widthMm,const std::vector<Bend>& bends,
        const std::vector<detail::ContourPoint>& closedOutline,
        Audit* output,std::string* error=nullptr){
    auto reject=[&](const char* why){if(error)*error=why;return false;};
    if(!output||bends.empty()||bends.size()>50||straightFlangesMm.size()!=bends.size()+1||
       closedOutline.size()<6||closedOutline.size()>10000||
       !std::isfinite(thicknessMm)||!std::isfinite(widthMm)||
       thicknessMm<=0||widthMm<=0||thicknessMm>1000000||widthMm>1000000)
        return reject("Invalid refold dimensions, bend sequence, polygon size or output.");
    long double length=0;
    for(double flange:straightFlangesMm){
        if(!std::isfinite(flange)||flange<=0||flange>1000000)
            return reject("Invalid straight neutral flange length.");
        length+=flange;
    }
    int reversed=0;
    int lastSign=0;
    for(const auto& bend:bends){
        if(!std::isfinite(bend.signedDegrees)||std::abs(bend.signedDegrees)<15||
           std::abs(bend.signedDegrees)>165||!std::isfinite(bend.insideRadiusMm)||
           bend.insideRadiusMm<=0||bend.insideRadiusMm>1000000)
            return reject("Invalid refold bend angle or inside radius.");
        const int sign=bend.signedDegrees>0?1:-1;
        if(lastSign&&sign!=lastSign)++reversed;
        lastSign=sign;
        length+=std::abs(bend.signedDegrees)*
                (3.141592653589793238462643383279502884L/180.0L)*
                (static_cast<long double>(bend.insideRadiusMm)+thicknessMm/2);
    }
    if(!detail::simpleClosedContour(closedOutline))
        return reject("Refold polygon is open, degenerate, overlapping or self-intersecting.");
    // Shoelace in extended precision; subtract the first point to reduce
    // cancellation for profiles with a large absolute coordinate origin.
    long double signedTwiceArea=0;
    const auto& origin=closedOutline.front();
    for(std::size_t i=0;i<closedOutline.size();++i){
        const auto& a=closedOutline[i];
        const auto& b=closedOutline[(i+1)%closedOutline.size()];
        signedTwiceArea+=(static_cast<long double>(a[0])-origin[0])* 
                         (static_cast<long double>(b[1])-origin[1])-
                         (static_cast<long double>(b[0])-origin[0])*
                         (static_cast<long double>(a[1])-origin[1]);
    }
    const long double area=std::abs(signedTwiceArea)/2;
    const long double expectedArea=length*thicknessMm;
    const long double errorRatio=std::abs(area-expectedArea)/expectedArea;
    if(!std::isfinite(static_cast<double>(expectedArea))||
       !std::isfinite(static_cast<double>(area))||
       !std::isfinite(static_cast<double>(errorRatio)))
        return reject("Refold area or length is outside finite numeric limits.");
    // 18 chord segments/90 degrees: 0.5% is a deliberately wider bound than
    // the typical <0.02% polygonal arc-area loss. Outliers require review.
    if(errorRatio>0.005L)
        return reject("Folded cross-section does not conserve developed mid-surface area (0.5% limit).");
    const long double volume=area*widthMm;
    if(!std::isfinite(static_cast<double>(volume)))
        return reject("Folded volume exceeds finite numeric limits.");
    Audit candidate;
    candidate.exactMidSurfaceDevelopedLengthMm=static_cast<double>(length);
    candidate.facetedCrossSectionAreaMm2=static_cast<double>(area);
    candidate.exactCrossSectionAreaMm2=static_cast<double>(expectedArea);
    candidate.relativeAreaError=static_cast<double>(errorRatio);
    candidate.predictedSolidVolumeMm3=static_cast<double>(volume);
    candidate.bendCount=static_cast<int>(bends.size());
    candidate.reversedBendCount=reversed;
    candidate.closedSimpleContour=true;
    *output=candidate;
    if(error)error->clear();
    return true;
}
} // namespace cad::refold

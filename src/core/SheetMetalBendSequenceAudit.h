#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <limits>
#include <string>
#include <vector>

// Geometric development audit, NOT a press-brake sequence feasibility,
// material springback, tooling clearance, or collision certification.
namespace cad::sheet {
struct BendInput {double signedDegrees=0,insideRadiusMm=0,kFactor=0.33;};
struct BendStation {
    std::size_t number=0;
    double signedDegrees=0;
    double cumulativeDirectionDegrees=0;
    double neutralAllowanceMm=0;
    double stationMm=0;
    // Neutral-axis tangent locations on the developed blank: start/end of
    // each bend allowance interval, not tool clearance or a bend-order plan.
    double flatTangentStartMm=0;
    double flatTangentEndMm=0;
    double insideRadiusToThickness=0;
};
struct FlatFlangeInterval {
    std::size_t number=0;
    double startMm=0.0;
    double endMm=0.0;
    double straightLengthMm=0.0;
};
struct BendSequenceReport {
    double developedLengthMm=0;
    double minimumStraightFlangeMm=0;
    std::size_t reverseDirectionCount=0;
    double maximumAbsoluteCumulativeDirectionDegrees=0;
    std::vector<BendStation> stations;
    std::vector<FlatFlangeInterval> flatFlanges;
};
inline bool inspectBendSequence(const std::vector<double>& straightLengths,
      double thicknessMm,const std::vector<BendInput>& bends,
      BendSequenceReport* report,std::string* error=nullptr){
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!report||bends.empty()||bends.size()>50||straightLengths.size()!=bends.size()+1||
       !std::isfinite(thicknessMm)||thicknessMm<=0||thicknessMm>1e6)
        return fail("Provide 1-50 bends, one additional straight flange, and a positive sheet thickness.");
    BendSequenceReport output;output.minimumStraightFlangeMm=1e300;
    double cursor=0,direction=0;int previousSign=0;
    for(double length:straightLengths){
        if(!std::isfinite(length)||length<=0||length>1e6)
            return fail("Straight flange lengths must be finite positive values up to 1,000,000 mm.");
        output.minimumStraightFlangeMm=std::min(output.minimumStraightFlangeMm,length);
    }
    cursor=straightLengths.front();
    output.flatFlanges.push_back({1,0.0,cursor,straightLengths.front()});
    for(std::size_t i=0;i<bends.size();++i){
        const auto& b=bends[i];const double absAngle=std::abs(b.signedDegrees);
        if(!std::isfinite(absAngle)||absAngle<15||absAngle>165||
           !std::isfinite(b.insideRadiusMm)||b.insideRadiusMm<=0||b.insideRadiusMm>1e6||
           !std::isfinite(b.kFactor)||b.kFactor<0||b.kFactor>1)
            return fail("Each bend requires a signed 15-165 degree angle, positive inside radius, and K-factor in [0,1].");
        const double allowance=absAngle*std::numbers::pi/180.0*(b.insideRadiusMm+b.kFactor*thicknessMm);
        if(!std::isfinite(allowance))return fail("Bend allowance overflowed.");
        const int sign=b.signedDegrees>0?1:-1;
        if(previousSign!=0&&sign!=previousSign)++output.reverseDirectionCount;
        previousSign=sign;direction+=b.signedDegrees;
        output.maximumAbsoluteCumulativeDirectionDegrees=std::max(
            output.maximumAbsoluteCumulativeDirectionDegrees,std::abs(direction));
        const double tangentStart=cursor;
        const double tangentEnd=tangentStart+allowance;
        const double station=tangentStart+allowance/2;
        output.stations.push_back({i+1,b.signedDegrees,direction,allowance,station,
                                   tangentStart,tangentEnd,b.insideRadiusMm/thicknessMm});
        cursor=tangentEnd+straightLengths[i+1];
        if(!std::isfinite(cursor)||cursor>1e12)return fail("Developed blank exceeds the supported numeric range.");
        output.flatFlanges.push_back({i+2,tangentEnd,cursor,straightLengths[i+1]});
    }
    output.developedLengthMm=cursor;
    *report=std::move(output);if(error)error->clear();return true;
}
// Shop-supplied minimum distance between *bend centerlines* on the flat
// development. This is a dimensional planning preflight, not proof that a
// press-brake tool can access the bends or that the folding sequence is safe.
struct BendStationSpacingCheck {
    double requiredMinimumMm=0;
    double actualMinimumMm=0;
    std::vector<std::size_t> tooClosePairs; // 1-based first bend in each pair
};
inline bool checkBendStationSpacing(const BendSequenceReport& development,
                                    double minimumCenterDistanceMm,
                                    BendStationSpacingCheck* output,
                                    std::string* error=nullptr){
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!output||!std::isfinite(minimumCenterDistanceMm)||
       minimumCenterDistanceMm<0||minimumCenterDistanceMm>1e9||
       development.stations.empty()||development.stations.size()>50||
       !std::isfinite(development.developedLengthMm)||
       development.developedLengthMm<=0)
        return fail("Provide a valid bend development and a nonnegative shop bend-center spacing limit.");
    BendStationSpacingCheck staged;
    staged.requiredMinimumMm=minimumCenterDistanceMm;
    staged.actualMinimumMm=0;
    if(development.stations.size()>1){
        staged.actualMinimumMm=std::numeric_limits<double>::infinity();
        for(std::size_t i=1;i<development.stations.size();++i){
            const auto& prev=development.stations[i-1];
            const auto& next=development.stations[i];
            const double distance=next.stationMm-prev.stationMm;
            if(!std::isfinite(distance)||distance<=0||
               !std::isfinite(prev.flatTangentEndMm)||
               !std::isfinite(next.flatTangentStartMm)||
               prev.flatTangentEndMm>next.flatTangentStartMm+1e-8)
                return fail("Bend stations are invalid, reordered, or overlapping.");
            staged.actualMinimumMm=std::min(staged.actualMinimumMm,distance);
            if(distance+1e-9<minimumCenterDistanceMm)
                staged.tooClosePairs.push_back(i);
        }
    }
    *output=std::move(staged);
    if(error)error->clear();return true;
}
} // namespace cad::sheet

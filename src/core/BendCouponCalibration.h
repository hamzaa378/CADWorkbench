#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

// Shop coupon measurements must refer to one bend with measured flat blank
// length and tangent-to-tangent straight portions. These values are empirical;
// the calculation cannot certify springback, tooling, grain direction, or a
// forming sequence. It never changes a part or a saved development by itself.
namespace cad::sheet {
struct MeasuredBendCoupon {
    double thicknessMm=0;
    double insideRadiusMm=0;
    double absoluteBendDegrees=0;
    double straightTotalMm=0; // summed tangent-to-tangent straight lengths
    double measuredBlankMm=0;
};
struct BendCouponFactor {
    double radiusToThickness=0;
    double absoluteBendDegrees=0;
    double kFactor=0;
};
struct BendCalibrationTarget {
    double thicknessMm=0;
    double insideRadiusMm=0;
    double absoluteBendDegrees=0;
};
struct CalibratedBendFactor {
    double radiusToThickness=0;
    double absoluteBendDegrees=0;
    double kFactor=0;
    std::size_t lowerCoupon=0; // 1-based index in sorted coupon table
    std::size_t upperCoupon=0;
};
inline bool inferBendCouponFactors(const std::vector<MeasuredBendCoupon>& samples,
          std::vector<BendCouponFactor>* output,std::string* error=nullptr) {
    const auto fail=[&](const char* issue){if(error)*error=issue;return false;};
    if(!output||samples.empty()||samples.size()>50)
        return fail("Provide between 1 and 50 measured single-bend coupons and an output.");
    std::vector<BendCouponFactor> staged; staged.reserve(samples.size());
    constexpr double pi=3.141592653589793238462643383279502884;
    for(const auto& c:samples) {
        if(!std::isfinite(c.thicknessMm)||c.thicknessMm<=0||c.thicknessMm>1e6||
           !std::isfinite(c.insideRadiusMm)||c.insideRadiusMm<=0||c.insideRadiusMm>1e6||
           !std::isfinite(c.absoluteBendDegrees)||c.absoluteBendDegrees<15||c.absoluteBendDegrees>165||
           !std::isfinite(c.straightTotalMm)||c.straightTotalMm<=0||c.straightTotalMm>1e7||
           !std::isfinite(c.measuredBlankMm)||c.measuredBlankMm<=c.straightTotalMm||
           c.measuredBlankMm>1e8)
            return fail("Coupon geometry must be finite: positive thickness/radius/straights, 15-165 degrees, and a longer measured blank.");
        const double ratio=c.insideRadiusMm/c.thicknessMm;
        const double allowance=c.measuredBlankMm-c.straightTotalMm;
        const double k=(allowance/(c.absoluteBendDegrees*pi/180.0)-c.insideRadiusMm)/c.thicknessMm;
        if(!std::isfinite(ratio)||!std::isfinite(k)||k < -1e-9||k>1+1e-9)
            return fail("Measured coupon implies an impossible neutral-axis K-factor outside [0,1]. Check tangent lengths and bend angle.");
        staged.push_back({ratio,c.absoluteBendDegrees,std::clamp(k,0.0,1.0)});
    }
    std::stable_sort(staged.begin(),staged.end(),[](const auto& a,const auto& b){
        if(a.radiusToThickness!=b.radiusToThickness)return a.radiusToThickness<b.radiusToThickness;
        return a.absoluteBendDegrees<b.absoluteBendDegrees;
    });
    *output=std::move(staged);if(error)error->clear();return true;
}
// The schedule interpolates only within measured R/t coverage. Coupon angles
// must lie within 15 degrees of the requested bend. Duplicate R/t samples must
// agree to within 0.05 K and are averaged; conflicting measurements are rejected.
// A physical shop's own qualified material/tooling specification remains primary.
inline bool interpolateCouponKFactors(const std::vector<BendCouponFactor>& coupons,
             const std::vector<BendCalibrationTarget>& targets,
             std::vector<CalibratedBendFactor>* output,std::string* error=nullptr) {
    const auto fail=[&](const char* issue){if(error)*error=issue;return false;};
    if(!output||coupons.empty()||coupons.size()>50||targets.empty()||targets.size()>50)
        return fail("Provide valid coupon factors and 1-50 bend targets.");
    struct Node {double ratio=0,k=0;std::size_t sample=0;};
    std::vector<CalibratedBendFactor> staged;staged.reserve(targets.size());
    for(const auto& t:targets) {
        if(!std::isfinite(t.thicknessMm)||t.thicknessMm<=0||t.thicknessMm>1e6||
           !std::isfinite(t.insideRadiusMm)||t.insideRadiusMm<=0||t.insideRadiusMm>1e6||
           !std::isfinite(t.absoluteBendDegrees)||t.absoluteBendDegrees<15||t.absoluteBendDegrees>165)
            return fail("Bend targets need positive finite thickness/radius and 15-165 degree angles.");
        const double ratio=t.insideRadiusMm/t.thicknessMm;
        if(!std::isfinite(ratio))return fail("Bend target radius/thickness ratio overflowed.");
        std::vector<Node> compatible;
        for(std::size_t j=0;j<coupons.size();++j) {
            const auto& c=coupons[j];
            if(!std::isfinite(c.radiusToThickness)||c.radiusToThickness<=0||
               !std::isfinite(c.absoluteBendDegrees)||c.absoluteBendDegrees<15||c.absoluteBendDegrees>165||
               !std::isfinite(c.kFactor)||c.kFactor<0||c.kFactor>1)
                return fail("Coupon table contains an invalid or uncalibrated entry.");
            if(std::abs(c.absoluteBendDegrees-t.absoluteBendDegrees)<=15.0+1e-10)
                compatible.push_back({c.radiusToThickness,c.kFactor,j+1});
        }
        if(compatible.empty())return fail("No measured coupon is within 15 degrees of the target bend angle.");
        std::stable_sort(compatible.begin(),compatible.end(),[](auto a,auto b){return a.ratio<b.ratio;});
        // Collapse duplicate R/t values only if measured K is consistent.
        std::vector<Node> unique;
        for(const auto& n:compatible) {
            if(!unique.empty()&&std::abs(unique.back().ratio-n.ratio)<=1e-9) {
                if(std::abs(unique.back().k-n.k)>0.05)
                    return fail("Repeat coupons at the same radius/thickness disagree by more than 0.05 K.");
                unique.back().k=(unique.back().k+n.k)/2.0;
            } else unique.push_back(n);
        }
        if(ratio<unique.front().ratio-1e-9||ratio>unique.back().ratio+1e-9)
            return fail("Target R/t lies outside the measured coupon range; extrapolation is prohibited.");
        const auto it=std::lower_bound(unique.begin(),unique.end(),ratio,
                             [](const Node& a,double b){return a.ratio<b;});
        Node first,second;
        if(it==unique.end()){first=unique.back();second=first;}
        else if(std::abs(it->ratio-ratio)<=1e-9){first=*it;second=first;}
        else if(it==unique.begin()){first=*it;second=first;}
        else {first=*(it-1);second=*it;}
        double k=first.k;
        if(second.ratio>first.ratio+1e-12)
            k=first.k+(second.k-first.k)*(ratio-first.ratio)/(second.ratio-first.ratio);
        if(!std::isfinite(k)||k<0||k>1)return fail("Calibrated K-factor interpolation overflowed.");
        staged.push_back({ratio,t.absoluteBendDegrees,k,first.sample,second.sample});
    }
    *output=std::move(staged);if(error)error->clear();return true;
}
} // namespace cad::sheet

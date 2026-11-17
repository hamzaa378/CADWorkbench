#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

// Pure numerical interpolation of saved, bounded mechanism drive coordinates.
// The sampled poses MUST still be solved and tested by the CAD kernel. A smooth
// time law is not a collision, velocity, torque, or acceleration certificate.
namespace cad::joint_motion {
enum class Progression { Linear, Smoothstep, QuinticSmoothstep };
struct Drive {
    int id=0;
    double initial=0, target=0, minimum=0, maximum=0;
};
inline bool interpolate(const std::vector<Drive>& drives,double fraction,
                        Progression progression,
                        std::vector<std::pair<int,double>>* result,
                        std::string* error=nullptr){
    auto fail=[&](const char* reason){if(error)*error=reason;return false;};
    if(!result||drives.empty()||drives.size()>128||!std::isfinite(fraction)||
       fraction<0||fraction>1)return fail("Invalid motion fraction or drive inventory.");
    if(progression!=Progression::Linear&&progression!=Progression::Smoothstep&&
       progression!=Progression::QuinticSmoothstep)
        return fail("Unrecognized drive interpolation mode.");
    const double t=progression==Progression::Smoothstep?
        fraction*fraction*(3.0-2.0*fraction):
        progression==Progression::QuinticSmoothstep?
        fraction*fraction*fraction*(10.0+fraction*(-15.0+6.0*fraction)):
        fraction;
    std::vector<std::pair<int,double>> proposed;
    proposed.reserve(drives.size());
    for(const auto& drive:drives){
        if(drive.id<0||!std::isfinite(drive.initial)||!std::isfinite(drive.target)||
           !std::isfinite(drive.minimum)||!std::isfinite(drive.maximum)||
           drive.minimum>drive.maximum||drive.initial<drive.minimum||
           drive.initial>drive.maximum||drive.target<drive.minimum||
           drive.target>drive.maximum)
            return fail("A driven coordinate is outside finite travel limits.");
        for(const auto& previous:proposed)
            if(previous.first==drive.id)return fail("Duplicate driven coordinate index.");
        // Weighted interpolation avoids calculating target-initial when the
        // two limits are large and opposite-signed; validate the result too.
        const double value=(1.0-t)*drive.initial+t*drive.target;
        if(!std::isfinite(value))return fail("Interpolated drive is not finite.");
        proposed.emplace_back(drive.id,value);
    }
    *result=std::move(proposed);
    if(error)error->clear();
    return true;
}
} // namespace cad::joint_motion

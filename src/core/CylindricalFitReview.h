#pragma once
#include <cmath>
#include <string>

// Geometry-only diametral fit worksheet. Caller MUST explicitly identify the
// hole and shaft: two cylindrical B-rep faces alone reveal neither intent nor
// manufacturing tolerances. No automatic mate, clearance certification, or cut.
namespace cad::mates {
struct CylindricalFitReview {
    double nominalDiametralClearanceMm=0;
    double smallestPossibleClearanceMm=0;
    double largestPossibleClearanceMm=0;
    bool alwaysClearance=false;
    bool alwaysInterference=false;
    bool transitionFit=false;
};
inline bool reviewCylindricalFit(double holeDiameterMm,double shaftDiameterMm,
        double holeLowerDeviationMm,double holeUpperDeviationMm,
        double shaftLowerDeviationMm,double shaftUpperDeviationMm,
        CylindricalFitReview* result,std::string* error=nullptr){
    const auto fail=[&](const char* msg){if(error)*error=msg;return false;};
    if(!result)return fail("Missing fit review result.");
    for(const double value:{holeDiameterMm,shaftDiameterMm,holeLowerDeviationMm,
            holeUpperDeviationMm,shaftLowerDeviationMm,shaftUpperDeviationMm})
        if(!std::isfinite(value)||std::abs(value)>1e8)return fail("Non-finite or unsupported fit dimension.");
    if(holeDiameterMm<=0||shaftDiameterMm<=0||
       holeLowerDeviationMm>holeUpperDeviationMm||shaftLowerDeviationMm>shaftUpperDeviationMm||
       holeDiameterMm+holeLowerDeviationMm<=0||
       shaftDiameterMm+shaftLowerDeviationMm<=0)
        return fail("Invalid diameter, deviation interval, or nonpositive minimum diameter.");
    const double minimum=holeDiameterMm+holeLowerDeviationMm-
                         shaftDiameterMm-shaftUpperDeviationMm;
    const double maximum=holeDiameterMm+holeUpperDeviationMm-
                         shaftDiameterMm-shaftLowerDeviationMm;
    if(!std::isfinite(minimum)||!std::isfinite(maximum)||minimum>maximum)
        return fail("Diametral fit interval is invalid.");
    CylindricalFitReview proposed;
    proposed.nominalDiametralClearanceMm=holeDiameterMm-shaftDiameterMm;
    proposed.smallestPossibleClearanceMm=minimum;
    proposed.largestPossibleClearanceMm=maximum;
    proposed.alwaysClearance=minimum>0;
    proposed.alwaysInterference=maximum<0;
    proposed.transitionFit=!proposed.alwaysClearance&&!proposed.alwaysInterference;
    *result=proposed;
    if(error)error->clear();return true;
}
} // namespace cad::mates

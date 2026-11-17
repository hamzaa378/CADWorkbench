#pragma once
#include <cmath>

namespace cad::fit {
// Volume tolerance is in mm^3; distance tolerance is in mm.
enum class Status { Interference, InsufficientClearance, Contact, Clear };
inline bool validThreshold(double mm) noexcept {
    return std::isfinite(mm) && mm >= 0.0 && mm <= 1000000.0;
}
inline bool classify(double distanceMm, double overlapMm3, double minClearanceMm,
                     Status* out) noexcept {
    if(!out || !validThreshold(minClearanceMm) || !std::isfinite(distanceMm) ||
       !std::isfinite(overlapMm3) || distanceMm < -1e-8 || overlapMm3 < -1e-8)
        return false;
    if(overlapMm3 > 1e-7) *out=Status::Interference;
    else if(distanceMm <= 1e-7) *out=Status::Contact;
    else if(distanceMm + 1e-8 < minClearanceMm) *out=Status::InsufficientClearance;
    else *out=Status::Clear;
    return true;
}
inline const char* statusName(Status status) noexcept {
    switch(status){
    case Status::Interference: return "Interference";
    case Status::InsufficientClearance: return "Below clearance";
    case Status::Contact: return "Contact";
    case Status::Clear: return "Clear";
    }
    return "Unknown";
}
// Conservative body-envelope calculation: each real part must lie inside its
// nominal solid expanded by its user-specified maximum geometric deviation.
// This is NOT a statistical tolerance stack, ISO fit certification, nor a
// penetration-depth or swept-motion estimate.
enum class ToleranceStatus {
    NominalInterference, NoGuaranteedSeparation, BelowRequiredClearance, GuaranteedClearance
};
struct ToleranceResult {
    double lowerBoundGapMm=0.0;
    double signedClearanceMarginMm=0.0;
    ToleranceStatus status=ToleranceStatus::NoGuaranteedSeparation;
};
inline bool assessTolerance(double nominalDistanceMm,double overlapMm3,
                            double requiredClearanceMm,double firstDeviationMm,
                            double secondDeviationMm,ToleranceResult* out) noexcept {
    if(!out || !validThreshold(requiredClearanceMm) ||
       !validThreshold(firstDeviationMm) || !validThreshold(secondDeviationMm))return false;
    Status nominal=Status::Clear;
    if(!classify(nominalDistanceMm,overlapMm3,requiredClearanceMm,&nominal))return false;
    const double combined=firstDeviationMm+secondDeviationMm;
    const double rawLowerBound=nominalDistanceMm-combined;
    const double margin=rawLowerBound-requiredClearanceMm;
    if(!std::isfinite(rawLowerBound) || !std::isfinite(margin))return false;
    ToleranceResult staged;
    staged.lowerBoundGapMm=nominal==Status::Interference?0.0:
        (rawLowerBound>0.0?rawLowerBound:0.0);
    staged.signedClearanceMarginMm=margin;
    if(nominal==Status::Interference)
        staged.status=ToleranceStatus::NominalInterference;
    else if(nominal==Status::Contact || rawLowerBound<=1e-7)
        staged.status=ToleranceStatus::NoGuaranteedSeparation;
    else if(rawLowerBound<requiredClearanceMm)
        staged.status=ToleranceStatus::BelowRequiredClearance;
    else staged.status=ToleranceStatus::GuaranteedClearance;
    *out=staged;
    return true;
}
inline const char* toleranceStatusName(ToleranceStatus status) noexcept {
    switch(status){
    case ToleranceStatus::NominalInterference: return "Nominal interference";
    case ToleranceStatus::NoGuaranteedSeparation: return "Separation not guaranteed";
    case ToleranceStatus::BelowRequiredClearance: return "Clearance at risk";
    case ToleranceStatus::GuaranteedClearance: return "Clearance guaranteed within bounds";
    }
    return "Unknown";
}

}

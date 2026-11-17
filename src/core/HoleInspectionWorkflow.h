#pragma once
// Revision-bound adapter from actual CAD design-hole coordinates to independently
// supplied inspection measurements. No nominal coordinate or revision token is
// accepted from the measurement as a substitute for the saved CAD design.
// A passing worksheet is not a GD&T or metrology certification.
#include "HolePositionInspection.h"
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cad::inspection {
struct DesignHoleReference {
    std::string bodyId;
    std::string geometryToken;
    int holeNumber=0;
    double nominalXmm=0,nominalYmm=0,nominalDiameterMm=0;
};
struct IndependentHoleMeasurement {
    std::string bodyId;
    std::string geometryToken;
    int holeNumber=0;
    double measuredDatumXmm=0,measuredDatumYmm=0;
    double measuredProjectedXmm=0,measuredProjectedYmm=0;
    double projectedHeightMm=0;
    double actualDiameterMm=0,minimumDiameterMm=0,maximumDiameterMm=0;
    double diametralPositionToleranceMm=0;
    PositionMaterialCondition materialCondition=PositionMaterialCondition::RegardlessOfFeatureSize;
    double maximumPinDiameterMm=0;
};
// Requires exactly one independent measurement for each design hole. Results
// remain untouched if the design/measurement join fails or revision is stale.
inline bool reviewRevisionBoundHoleMeasurements(
    const std::vector<DesignHoleReference>& nominal,
    const std::vector<IndependentHoleMeasurement>& measured,
    HolePatternInspection* report,std::string* error=nullptr) {
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!report||nominal.empty()||nominal.size()>1000||nominal.size()!=measured.size())
        return fail("A one-to-one set of 1-1000 design holes and independent measurements is required.");
    using Key=std::pair<std::string,int>;
    std::map<Key,const IndependentHoleMeasurement*> measurements;
    for(const auto& m:measured) {
        if(m.bodyId.empty()||m.bodyId.size()>128||m.geometryToken.empty()||
           m.geometryToken.size()>256||m.holeNumber<=0||
           !measurements.emplace(Key{m.bodyId,m.holeNumber},&m).second)
            return fail("Missing or duplicated measured hole identity or geometry revision.");
    }
    std::map<Key,bool> designKeys;
    std::vector<HolePositionMeasurement> joined;
    joined.reserve(nominal.size());
    for(const auto& d:nominal) {
        const Key key{d.bodyId,d.holeNumber};
        if(d.bodyId.empty()||d.bodyId.size()>128||d.geometryToken.empty()||
           d.geometryToken.size()>256||d.holeNumber<=0||
           !std::isfinite(d.nominalXmm)||!std::isfinite(d.nominalYmm)||
           !std::isfinite(d.nominalDiameterMm)||d.nominalDiameterMm<=0||
           std::abs(d.nominalXmm)>1e9||std::abs(d.nominalYmm)>1e9||
           d.nominalDiameterMm>1e6||!designKeys.emplace(key,true).second)
            return fail("Invalid or duplicated CAD design-hole reference.");
        auto it=measurements.find(key);
        if(it==measurements.end()||it->second->geometryToken!=d.geometryToken)
            return fail("Inspection measurement is missing or belongs to a stale geometry revision.");
        const auto& m=*it->second;
        HolePositionMeasurement h;
        // The length prefix makes the composite ID unambiguous even if a body
        // identifier contains punctuation or decimal digits.
        h.holeId=std::to_string(d.bodyId.size())+":"+d.bodyId+":"+
            std::to_string(d.holeNumber);
        h.nominalXmm=d.nominalXmm;
        h.nominalYmm=d.nominalYmm;
        h.measuredDatumXmm=m.measuredDatumXmm;
        h.measuredDatumYmm=m.measuredDatumYmm;
        h.measuredProjectedXmm=m.measuredProjectedXmm;
        h.measuredProjectedYmm=m.measuredProjectedYmm;
        h.projectedHeightMm=m.projectedHeightMm;
        h.actualDiameterMm=m.actualDiameterMm;
        h.minimumDiameterMm=m.minimumDiameterMm;
        h.maximumDiameterMm=m.maximumDiameterMm;
        h.diametralPositionToleranceMm=m.diametralPositionToleranceMm;
        h.materialCondition=m.materialCondition;
        h.maximumPinDiameterMm=m.maximumPinDiameterMm;
        joined.push_back(std::move(h));
    }
    return inspectHolePositionPattern(joined,report,error);
}
} // namespace cad::inspection

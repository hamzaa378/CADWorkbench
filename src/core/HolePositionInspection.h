#pragma once
// Dimensional inspection worksheet for a planar nominal hole pattern, in mm.
// The hole axis is approximated by the measured straight centreline between
// datum and projected planes. Not metrology certification or automatic CMM data
// import. All nominal coordinates, datum alignment and diameter measurements
// are provided by the engineer (never inferred from the B-rep).
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad::inspection {
enum class PositionMaterialCondition { RegardlessOfFeatureSize, MaximumMaterialCondition };
struct HolePositionMeasurement {
    std::string holeId;
    double nominalXmm=0, nominalYmm=0;
    double measuredDatumXmm=0, measuredDatumYmm=0;
    double measuredProjectedXmm=0, measuredProjectedYmm=0;
    double projectedHeightMm=0; // 0 means a single measured plane
    double actualDiameterMm=0;
    double minimumDiameterMm=0; // lower size limit: internal-hole MMC
    double maximumDiameterMm=0;
    double diametralPositionToleranceMm=0;
    PositionMaterialCondition materialCondition=PositionMaterialCondition::RegardlessOfFeatureSize;
    double maximumPinDiameterMm=0; // 0 disables fit-envelope evaluation
};
struct HolePositionFinding {
    std::string holeId;
    double maximumCenterDeviationMm=0;
    double measuredDiametralPositionMm=0;
    double materialBonusMm=0;
    double allowedDiametralPositionMm=0;
    double minimumPinClearanceMm=0; // diametral clearance for supplied pin
    double virtualConditionDiameterMm=0; // hole MMC minus positional tolerance
    bool sizePass=false;
    bool positionPass=false;
    bool matingPinEvaluated=false;
    bool pinClearancePass=false;
    bool accepted=false;
};
struct HolePatternInspection {
    std::size_t checkedHoles=0;
    std::size_t acceptedHoles=0;
    std::size_t rejectedSize=0;
    std::size_t rejectedPosition=0;
    std::size_t rejectedPinFit=0;
    bool allAccepted=false;
    double worstPositionUtilization=0;
    std::vector<HolePositionFinding> findings;
};

inline bool inspectHolePositionPattern(const std::vector<HolePositionMeasurement>& holes,
                    HolePatternInspection* report,std::string* error=nullptr) {
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!report||holes.empty()||holes.size()>1000)
        return fail("Provide 1-1000 inspected holes and an output report.");
    std::set<std::string> ids;
    HolePatternInspection staged;
    staged.findings.reserve(holes.size());
    for(const auto& h:holes) {
        const double measurements[]={h.nominalXmm,h.nominalYmm,
            h.measuredDatumXmm,h.measuredDatumYmm,h.measuredProjectedXmm,
            h.measuredProjectedYmm,h.projectedHeightMm,h.actualDiameterMm,
            h.minimumDiameterMm,h.maximumDiameterMm,h.diametralPositionToleranceMm,
            h.maximumPinDiameterMm};
        for(const double value:measurements) {
            if(!std::isfinite(value)||std::abs(value)>1e9)
                return fail("Hole measurements must be finite bounded millimetre quantities.");
        }
        if(h.holeId.empty()||h.holeId.size()>128||!ids.insert(h.holeId).second||
           h.projectedHeightMm<0||h.projectedHeightMm>1e6||
           h.actualDiameterMm<=0||h.minimumDiameterMm<=0||
           h.minimumDiameterMm>h.maximumDiameterMm||h.maximumDiameterMm>1e6||
           h.diametralPositionToleranceMm<0||h.diametralPositionToleranceMm>1e6||
           h.maximumPinDiameterMm<0||h.maximumPinDiameterMm>1e6||
           (h.projectedHeightMm==0&&
            (h.measuredDatumXmm!=h.measuredProjectedXmm||
             h.measuredDatumYmm!=h.measuredProjectedYmm))||
           (h.materialCondition!=PositionMaterialCondition::RegardlessOfFeatureSize&&
            h.materialCondition!=PositionMaterialCondition::MaximumMaterialCondition))
            return fail("Invalid hole ID, dimension bounds, projected axis, or material-condition policy.");
        const double datumRadial=std::hypot(h.measuredDatumXmm-h.nominalXmm,
                                           h.measuredDatumYmm-h.nominalYmm);
        const double projectedRadial=std::hypot(h.measuredProjectedXmm-h.nominalXmm,
                                               h.measuredProjectedYmm-h.nominalYmm);
        HolePositionFinding f;
        f.holeId=h.holeId;
        // For a straight axis, norm of the offset varies convexly with height.
        // The maximum over the projected segment is therefore at an endpoint.
        f.maximumCenterDeviationMm=std::max(datumRadial,projectedRadial);
        f.measuredDiametralPositionMm=2*f.maximumCenterDeviationMm;
        f.materialBonusMm=h.materialCondition==PositionMaterialCondition::MaximumMaterialCondition
            ?std::max(0.0,h.actualDiameterMm-h.minimumDiameterMm):0.0;
        f.allowedDiametralPositionMm=h.diametralPositionToleranceMm+f.materialBonusMm;
        f.virtualConditionDiameterMm=h.minimumDiameterMm-h.diametralPositionToleranceMm;
        f.sizePass=h.actualDiameterMm>=h.minimumDiameterMm&&
                   h.actualDiameterMm<=h.maximumDiameterMm;
        f.positionPass=f.measuredDiametralPositionMm<=f.allowedDiametralPositionMm;
        f.matingPinEvaluated=h.maximumPinDiameterMm>0;
        f.minimumPinClearanceMm=f.matingPinEvaluated ?
            h.actualDiameterMm-h.maximumPinDiameterMm-f.measuredDiametralPositionMm : 0.0;
        f.pinClearancePass=!f.matingPinEvaluated||f.minimumPinClearanceMm>=0;
        f.accepted=f.sizePass&&f.positionPass&&f.pinClearancePass;
        if(!std::isfinite(f.measuredDiametralPositionMm)||
           !std::isfinite(f.allowedDiametralPositionMm)||
           !std::isfinite(f.minimumPinClearanceMm)||
           !std::isfinite(f.virtualConditionDiameterMm))
            return fail("Hole position calculations exceeded the safe numeric range.");
        if(f.allowedDiametralPositionMm==0) {
            if(f.measuredDiametralPositionMm>0)
                staged.worstPositionUtilization=std::numeric_limits<double>::infinity();
        } else {
            staged.worstPositionUtilization=std::max(staged.worstPositionUtilization,
                f.measuredDiametralPositionMm/f.allowedDiametralPositionMm);
        }
        ++staged.checkedHoles;
        if(f.accepted)++staged.acceptedHoles;
        if(!f.sizePass)++staged.rejectedSize;
        if(!f.positionPass)++staged.rejectedPosition;
        if(!f.pinClearancePass)++staged.rejectedPinFit;
        staged.findings.push_back(std::move(f));
    }
    staged.allAccepted=staged.acceptedHoles==staged.checkedHoles;
    *report=std::move(staged);
    if(error)error->clear();
    return true;
}
} // namespace cad::inspection

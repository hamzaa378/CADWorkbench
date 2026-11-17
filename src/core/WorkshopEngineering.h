#pragma once
// Portable engineering worksheets for preliminary design and process estimation.
// The calculations do not represent certified section data or executable NC paths.
#include <string>
#include <vector>

namespace cad::workshop {

struct SectionSpec {
    std::string profile; // rectangle, circle, hollow_circle, box, i_beam, channel, tee, angle
    double widthMm=0.0; // outside width; for circles, outside diameter
    double heightMm=0.0; // ignored for circles
    double wallMm=0.0; // box wall or annular wall, or web thickness
    double flangeMm=0.0; // flange thickness for I/C/T/angle (box uses wall)
    double densityKgPerM3=0.0; // 0 = not specified; mass per metre then zero
};
struct SectionResult {
    double areaMm2=0.0, centroidXmm=0.0, centroidYmm=0.0;
    double secondMomentXmm4=0.0, secondMomentYmm4=0.0;
    double productMomentXYmm4=0.0, polarAreaMomentMm4=0.0;
    double modulusXmm3=0.0, modulusYmm3=0.0;
    double gyrationXmm=0.0, gyrationYmm=0.0;
    double massKgPerMeter=0.0;
};
std::vector<std::string> sectionProfiles();
bool calculateSection(const SectionSpec& spec,SectionResult* result,std::string* error=nullptr);

struct MachiningSpec {
    std::string operation; // drill, ream, tap, counterbore, countersink, mill_slot, mill_face, turn
    double toolDiameterMm=0.0;
    double travelMm=0.0; // nominal tool travel PER pass, not confirmed material engagement
    double cuttingSpeedMMin=0.0;
    double feedPerRevMm=0.0;
    double feedPerToothMm=0.0;
    int teeth=0;
    int passes=1;
    double radialWidthMm=0.0; // milling or turning engagement (mm)
    double depthMm=0.0; // milling/turning engagement (mm)
    double maxSpindleRpm=0.0; // 0 means no specified machine limit
    double materialDensityKgM3=0.0; // 0 means not available
    double specificCuttingEnergyJMm3=0.0; // 0 means not available
    double availablePowerKw=0.0; // 0 means not available
};
struct MachiningResult {
    double spindleRpm=0.0, actualCuttingSpeedMMin=0.0, feedMmMin=0.0;
    double cycleTimeMin=0.0, nominalEngagementVolumeMm3=0.0;
    double estimatedRemovalRateMm3Min=0.0, nominalChipMassKg=0.0;
    double approximatePowerKw=0.0;
    std::vector<std::string> warnings;
};
std::vector<std::string> machiningOperations();
bool estimateMachining(const MachiningSpec& spec,MachiningResult* result,std::string* error=nullptr);

struct ToleranceTerm {
    std::string label;
    double nominalMm=0.0;
    double lowerDeviationMm=0.0; // <= upper deviation; offsets from nominal
    double upperDeviationMm=0.0;
    int direction=1; // +1 or -1 in the assembly chain
};
struct ToleranceResult {
    double nominalMm=0.0, worstMinMm=0.0, worstMaxMm=0.0;
    double rssCenterMm=0.0, rssHalfWidthMm=0.0;
    std::vector<std::string> warnings;
};
bool calculateToleranceStack(const std::vector<ToleranceTerm>& terms,
                             ToleranceResult* result,std::string* error=nullptr);
}

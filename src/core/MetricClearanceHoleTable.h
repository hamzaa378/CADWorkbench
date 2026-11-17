#pragma once
// Nominal clearance-hole sizes in mm: ISO 273 fine, medium, coarse series.
// Reference dimensional selections only: no hole tolerance, positional
// tolerance, fixture clearance or fastener strength is certified here.
#include <array>
#include <cmath>
#include <string_view>

namespace cad::metric_holes {
struct ClearanceRow {
    std::string_view designation;
    double majorDiameterMm;
    std::array<double,3> nominalHoleDiameterMm;
};
inline constexpr std::array<ClearanceRow,12> kClearanceRows{{
    {"M2",   2.0,  {2.2,2.4,2.6}},
    {"M2.5", 2.5,  {2.7,2.9,3.1}},
    {"M3",   3.0,  {3.2,3.4,3.6}},
    {"M4",   4.0,  {4.3,4.5,4.8}},
    {"M5",   5.0,  {5.3,5.5,5.8}},
    {"M6",   6.0,  {6.4,6.6,7.0}},
    {"M8",   8.0,  {8.4,9.0,10.0}},
    {"M10", 10.0, {10.5,11.0,12.0}},
    {"M12", 12.0, {13.0,13.5,14.5}},
    {"M14", 14.0, {15.0,15.5,16.5}},
    {"M16", 16.0, {17.0,17.5,18.5}},
    {"M20", 20.0, {21.0,22.0,24.0}}
}};
inline bool lookup(std::string_view designation,int fitIndex,double* diameterMm) {
    if (!diameterMm || fitIndex<0 || fitIndex>2) return false;
    for (const auto& row:kClearanceRows) {
        if (row.designation==designation) {
            const double value=row.nominalHoleDiameterMm[fitIndex];
            if (!std::isfinite(value)||value<=row.majorDiameterMm) return false;
            *diameterMm=value;
            return true;
        }
    }
    return false;
}
} // namespace cad::metric_holes

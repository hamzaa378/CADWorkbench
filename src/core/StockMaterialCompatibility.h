#pragma once
// Stock-issue preflight for a single shared manufacturing sheet. Rejects
// incompatible thicknesses and assigned material grades; unassigned grades
// remain explicitly flagged for human verification, never inferred.
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>
namespace cad::stock_material {
struct Part {std::string id,material;double thicknessMm=0;};
struct Result {double thicknessMm=0;std::string material;bool materialAssigned=false;
    std::size_t count=0;};
inline bool validate(const std::vector<Part>& parts,Result* result,std::string* error=nullptr){
    auto fail=[&](const char* detail){if(error)*error=detail;return false;};
    if(!result||parts.empty()||parts.size()>24)
        return fail("Stock issue requires 1-24 parts and a separate result.");
    const double gauge=parts.front().thicknessMm;
    const std::string material=parts.front().material;
    if(!std::isfinite(gauge)||gauge<=0)
        return fail("Stock thickness must be positive and finite.");
    std::set<std::string> seen;
    for(const Part& part:parts){
        if(!seen.insert(part.id).second)return fail("Duplicate flat-blank ID in stock issue.");
        if(part.id.empty()||!std::isfinite(part.thicknessMm)||part.thicknessMm<=0)
            return fail("Each stock item needs an ID and a positive finite gauge.");
        if(std::abs(part.thicknessMm-gauge)>std::max(1e-6,1e-6*gauge))
            return fail("Cannot nest mixed sheet thicknesses on one physical stock sheet.");
        // Both empty is an explicitly unassigned grade. Assigned vs missing
        // is not accepted as proof of material compatibility.
        if(part.material!=material)
            return fail("Cannot nest different or unassigned material grades together.");
    }
    *result={gauge,material,!material.empty(),parts.size()};
    if(error)error->clear();return true;
}
} // namespace cad::stock_material

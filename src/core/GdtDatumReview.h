#pragma once
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

// Advisory engineering review of MANUALLY authored GD&T frames, not a
// standard-conformance or tolerance-stack certification. No datum features
// are inferred from a B-rep. Saved frames and legacy scene data are unchanged.
namespace cad::drawing {
struct GdtFrame {
    std::string id,characteristic,datumA,datumB,datumC;
    double toleranceMm=0;
};
struct GdtAdvice {std::string frameId;std::string warning;};
inline bool reviewGdtDatumPolicy(const std::vector<GdtFrame>& frames,
        std::vector<GdtAdvice>* result,std::string* error=nullptr){
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!result||frames.size()>256)return fail("Missing GD&T report or too many frames.");
    std::set<std::string> ids;
    std::vector<GdtAdvice> staged;
    const std::set<std::string> supported={"POSITION","FLATNESS","PARALLELISM",
        "PERPENDICULARITY","PROFILE","CIRCULARITY"};
    for(const auto& f:frames){
        const auto validDatum=[](const std::string& d){return d.empty()||
            (d.size()==1&&d[0]>='A'&&d[0]<='Z');};
        if(f.id.empty()||!ids.insert(f.id).second||!supported.count(f.characteristic)||
           !std::isfinite(f.toleranceMm)||f.toleranceMm<=0||
           !validDatum(f.datumA)||!validDatum(f.datumB)||!validDatum(f.datumC)||
           (!f.datumB.empty()&&f.datumA.empty())||
           (!f.datumC.empty()&&f.datumB.empty())||
           (!f.datumA.empty()&&(f.datumA==f.datumB||f.datumA==f.datumC))||
           (!f.datumB.empty()&&f.datumB==f.datumC))
            return fail("Invalid manual GD&T frame, identifier, tolerance, or datum sequence.");
        const bool form=f.characteristic=="FLATNESS"||f.characteristic=="CIRCULARITY";
        const bool needsDatum=f.characteristic=="POSITION"||
            f.characteristic=="PARALLELISM"||f.characteristic=="PERPENDICULARITY";
        if(form&&!f.datumA.empty())
            staged.push_back({f.id,"Form control normally does not use a datum reference; review this frame."});
        if(needsDatum&&f.datumA.empty())
            staged.push_back({f.id,"Orientation/position characteristic usually needs a defined datum scheme; review this frame."});
        if(f.characteristic=="PROFILE"&&f.datumA.empty())
            staged.push_back({f.id,"Datum-free profile control may be valid; verify design intent and drawing standard."});
    }
    *result=std::move(staged);
    if(error)error->clear();return true;
}
} // namespace cad::drawing

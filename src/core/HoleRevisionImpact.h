#pragma once
// Engineering change evidence: classify revision-bound CAD hole references.
// Equal positions alone do not establish unchanged topology or metrology validity.
// Even an UNCHANGED result is only a reference match, never a sign-off.
#include "HoleInspectionWorkflow.h"
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cad::inspection {
enum class HoleRevisionDisposition { Added, Removed, Changed, ReferenceMatch };
struct HoleRevisionRecord {
    std::string bodyId;
    int holeNumber=0;
    HoleRevisionDisposition disposition=HoleRevisionDisposition::Changed;
    bool geometryTokenChanged=false;
    bool nominalChanged=false;
    bool freshMeasurementRequired=true;
};
struct HoleRevisionImpact {
    std::vector<HoleRevisionRecord> records;
    std::size_t added=0, removed=0, changed=0, referenceMatches=0;
    bool evidenceApproval=false; // deliberately never true
};
namespace detail {
inline bool validDesignHole(const DesignHoleReference& r) {
    return !r.bodyId.empty() && r.bodyId.size()<=128 &&
           !r.geometryToken.empty() && r.geometryToken.size()<=256 &&
           r.holeNumber>0 && std::isfinite(r.nominalXmm) &&
           std::isfinite(r.nominalYmm) && std::isfinite(r.nominalDiameterMm) &&
           std::abs(r.nominalXmm)<=1e9 && std::abs(r.nominalYmm)<=1e9 &&
           r.nominalDiameterMm>0 && r.nominalDiameterMm<=1e6;
}
} // namespace detail
inline bool compareDesignHoleRevisions(const std::vector<DesignHoleReference>& before,
        const std::vector<DesignHoleReference>& after,HoleRevisionImpact* out,
        std::string* error=nullptr) {
    auto fail=[&](const char* reason) {if(error)*error=reason;return false;};
    if(!out||before.size()>1000||after.size()>1000)
        return fail("Expected two finite CAD design-hole inventories (at most 1000 each).");
    using Key=std::pair<std::string,int>;
    std::map<Key,const DesignHoleReference*> a,b;
    for(const auto& h:before)
        if(!detail::validDesignHole(h)||!a.emplace(Key{h.bodyId,h.holeNumber},&h).second)
            return fail("Invalid or duplicate previous design-hole reference.");
    for(const auto& h:after)
        if(!detail::validDesignHole(h)||!b.emplace(Key{h.bodyId,h.holeNumber},&h).second)
            return fail("Invalid or duplicate current design-hole reference.");
    HoleRevisionImpact staged;
    auto iterA=a.cbegin(),iterB=b.cbegin();
    while(iterA!=a.cend()||iterB!=b.cend()) {
        if(iterB==b.cend()||(iterA!=a.cend()&&iterA->first<iterB->first)) {
            staged.records.push_back({iterA->first.first,iterA->first.second,
                                      HoleRevisionDisposition::Removed,true,true,true});
            ++staged.removed;++iterA;
        } else if(iterA==a.cend()||iterB->first<iterA->first) {
            staged.records.push_back({iterB->first.first,iterB->first.second,
                                      HoleRevisionDisposition::Added,true,true,true});
            ++staged.added;++iterB;
        } else {
            const auto& old=*iterA->second;
            const auto& cur=*iterB->second;
            const bool changedToken=old.geometryToken!=cur.geometryToken;
            const bool changedNominal=old.nominalXmm!=cur.nominalXmm||
                       old.nominalYmm!=cur.nominalYmm||
                       old.nominalDiameterMm!=cur.nominalDiameterMm;
            const bool changed=changedToken||changedNominal;
            staged.records.push_back({iterA->first.first,iterA->first.second,
                       changed?HoleRevisionDisposition::Changed:HoleRevisionDisposition::ReferenceMatch,
                       changedToken,changedNominal,changed});
            if(changed)++staged.changed;else ++staged.referenceMatches;
            ++iterA;++iterB;
        }
    }
    *out=std::move(staged);
    if(error)error->clear();
    return true;
}
} // namespace cad::inspection

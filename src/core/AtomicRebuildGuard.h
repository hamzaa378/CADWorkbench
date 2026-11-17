#pragma once
// Commit-time, kernel-independent topology/revision invariant gate for
// detached multi-body feature regeneration. The *caller* rebuilds real OCCT
// B-reps and proves shape identities; this verifier checks the journal against
// the committed scene, before touching undo history or changing any part.
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
namespace cad::transaction {
struct BodyRevisionEvidence {
    std::string id;
    std::string geometryRevision;
    // For staged records, proves whether its TopoDS_Shape is EXACTLY IsSame()
    // as the committed shape. Not geometric equivalence or hash equality.
    bool sameCommittedTopology=true;
};
inline bool validateRebuildCommit(const std::vector<BodyRevisionEvidence>& before,
                                  const std::vector<BodyRevisionEvidence>& proposed,
                                  const std::vector<std::string>& roots,
                                  std::vector<std::string>* changed,
                                  std::string* error=nullptr){
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!changed||before.empty()||before.size()!=proposed.size()||before.size()>4096||
       roots.size()<2||roots.size()>16)
        return fail("Atomic rebuild must preserve 1-4096 bodies and declare 2-16 roots.");
    std::map<std::string,std::string> original;
    for(const auto& item:before){
        if(item.id.empty()||item.geometryRevision.empty()||
           !original.emplace(item.id,item.geometryRevision).second)
            return fail("Committed body identifiers or revision tokens are missing or duplicated.");
    }
    std::set<std::string> rootIds;
    for(const auto& root:roots)
        if(root.empty()||!rootIds.insert(root).second||original.find(root)==original.end())
            return fail("A proposed feature root is missing or duplicated.");
    std::set<std::string> seen;
    std::vector<std::string> updated;
    for(const auto& item:proposed){
        const auto prior=original.find(item.id);
        if(item.id.empty()||item.geometryRevision.empty()||prior==original.end()||
           !seen.insert(item.id).second)
            return fail("Staging lost, duplicated or invalidated a persisted body.");
        if(item.geometryRevision==prior->second){
            if(!item.sameCommittedTopology)
                return fail("Staged B-rep changed without a geometry revision update.");
        }else updated.push_back(item.id);
    }
    if(seen.size()!=original.size())return fail("Staging deleted a persisted body.");
    std::sort(updated.begin(),updated.end());
    *changed=std::move(updated);
    if(error)error->clear();
    return true;
}
} // namespace cad::transaction

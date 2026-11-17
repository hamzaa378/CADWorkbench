#pragma once
#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

// Audit an explicit parent-child occurrence tree. Parent indices refer to
// occurrence positions and -1 denotes a root. No inference from mate links:
// assembly hierarchy is NOT the same thing as the constraint network.
namespace cad::assembly {
struct HierarchyReport {
    std::size_t rootCount=0;
    std::size_t maximumDepth=0;
    std::vector<std::size_t> rootIndices;
    std::vector<std::size_t> leafIndices;
    std::vector<std::size_t> depthByOccurrence;
};
inline bool auditHierarchy(const std::vector<std::string>& identifiers,
        const std::vector<int>& parentIndices,HierarchyReport* result,
        std::string* error=nullptr){
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    const auto n=identifiers.size();
    if(!result||n==0||n>1024||parentIndices.size()!=n)
        return fail("Hierarchy requires 1-1024 matching occurrences and parents.");
    std::set<std::string> ids;
    std::vector<std::size_t> children(n,0);
    for(std::size_t i=0;i<n;++i){
        if(identifiers[i].empty()||!ids.insert(identifiers[i]).second)
            return fail("Hierarchy contains an empty or duplicate occurrence ID.");
        const int parent=parentIndices[i];
        if(parent < -1||parent >= static_cast<int>(n)||parent==static_cast<int>(i))
            return fail("Hierarchy contains an invalid or self-parenting occurrence.");
        if(parent>=0)++children[static_cast<std::size_t>(parent)];
    }
    HierarchyReport report;
    report.depthByOccurrence.assign(n,0);
    std::vector<unsigned char> state(n,0);
    const auto visit=[&](const auto& self,std::size_t i)->bool{
        if(state[i]==1)return false;
        if(state[i]==2)return true;
        state[i]=1;
        const int parent=parentIndices[i];
        if(parent>=0){
            if(!self(self,static_cast<std::size_t>(parent)))return false;
            report.depthByOccurrence[i]=report.depthByOccurrence[static_cast<std::size_t>(parent)]+1;
        }
        state[i]=2;
        return true;
    };
    for(std::size_t i=0;i<n;++i){
        if(!visit(visit,i))return fail("Hierarchy contains a parent cycle.");
        if(parentIndices[i]==-1)report.rootIndices.push_back(i);
        if(children[i]==0)report.leafIndices.push_back(i);
        report.maximumDepth=std::max(report.maximumDepth,report.depthByOccurrence[i]);
    }
    report.rootCount=report.rootIndices.size();
    if(report.rootCount==0)return fail("Hierarchy has no root.");
    *result=std::move(report);
    if(error)error->clear();return true;
}
} // namespace cad::assembly

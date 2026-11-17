#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// A fail-closed, kernel-independent ancestry planner. Every key denotes a
// completed kernel Boolean OR an exact rigid transformation, with its source
// stage(s) as values. The caller provides only supported operations and must
// rebase each hop against this rebuild's geometry. The returned path is an
// ordered sequence of supported stages from ancestor to target.
// The legacy single-route API rejects diamonds; allTrackedPaths provides
// bounded full-path enumeration with downstream geometric agreement checks.
namespace cad::lineage {
inline bool uniqueBooleanPath(const std::map<std::string,std::vector<std::string>>& parents,
                              const std::string& ancestor,const std::string& target,
                              std::vector<std::string>* path,std::string* reason=nullptr){
    if(!path||ancestor.empty()||target.empty()||ancestor==target){
        if(reason)*reason="A tracked reference needs a distinct source and derived stage.";
        return false;
    }
    // The maximum persisted feature graph contains 40 stages. Guard against
    // malformed cycles even though the document also checks forward links.
    const auto visit=[&](const auto& self,const std::string& current,
                         std::vector<std::string>& active,std::vector<std::string>& found,
                         int depth)->int{
        if(current==ancestor){found.clear();return 1;}
        if(depth>40||parents.find(current)==parents.end())return 0;
        for(const auto& item:active)if(item==current)return 0;
        active.push_back(current);
        int routes=0;
        for(const auto& operand:parents.at(current)){
            std::vector<std::string> candidate;
            const int count=self(self,operand,active,candidate,depth+1);
            if(count>0&&routes==0){
                found=std::move(candidate);
                found.push_back(current);
            }
            routes+=count;
            if(routes>1)break;
        }
        active.pop_back();
        return routes>1?2:routes;
    };
    std::vector<std::string> active,proposed;
    const int count=visit(visit,target,active,proposed,0);
    if(count!=1||proposed.empty()){
        if(reason)*reason=count>1?
            "Reference ancestor reaches the result through multiple branches; select an unambiguous source.":
            "Reference ancestor has no fully tracked Boolean/rigid path to its selected parent.";
        return false;
    }
    *path=std::move(proposed);
    return true;
}
}

namespace cad::lineage {
// Enumerate every supported route through a bounded DAG, rather than
// choosing the first route at a Boolean diamond. The caller must independently
// retarget a witness through EACH route and accept it only if all routes
// terminate at the identical face/edge and design placement. This permits
// unambiguous multi-source merges while failing closed for real ambiguities.
inline bool allTrackedPaths(const std::map<std::string,std::vector<std::string>>& parents,
                            const std::string& ancestor,const std::string& target,
                            std::vector<std::vector<std::string>>* paths,
                            std::string* reason=nullptr){
    if(!paths||ancestor.empty()||target.empty()||ancestor==target){
        if(reason)*reason="A tracked reference needs a distinct source and derived stage.";
        return false;
    }
    constexpr std::size_t maxPaths=32;
    std::vector<std::vector<std::string>> complete;
    std::vector<std::string> active,reversePath;
    bool overflow=false,cycle=false;
    const auto visit=[&](const auto& self,const std::string& current,int depth)->void{
        if(overflow||depth>40){overflow=true;return;}
        for(const auto& name:active)if(name==current){cycle=true;return;}
        if(current==ancestor){
            if(complete.size()>=maxPaths){overflow=true;return;}
            complete.emplace_back(reversePath.rbegin(),reversePath.rend());
            return;
        }
        const auto found=parents.find(current);
        if(found==parents.end())return;
        active.push_back(current);reversePath.push_back(current);
        // A corrupt file can list the same operand more than once. Such an
        // operand is one graph edge, not multiple legitimate ancestry paths.
        std::set<std::string> uniqueParents;
        for(const auto& parent:found->second)
            if(!parent.empty()&&uniqueParents.insert(parent).second)
                self(self,parent,depth+1);
        reversePath.pop_back();active.pop_back();
    };
    visit(visit,target,0);
    if(overflow||cycle||complete.empty()){
        if(reason)*reason=overflow?"Reference ancestry exceeds 32 routes or forty stages.":
            cycle?"Reference ancestry contains a cycle.":
            "There is no fully tracked ancestry route to the selected parent.";
        return false;
    }
    *paths=std::move(complete);
    return true;
}
} // namespace cad::lineage

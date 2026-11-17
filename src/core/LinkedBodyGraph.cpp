#include "LinkedBodyGraph.h"

#include <algorithm>
#include <initializer_list>
#include <functional>
#include <queue>
#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace cad::linkedgraph {
namespace {
bool fail(std::string* error, const std::string& message) {
    if(error) *error = message;
    return false;
}
}

bool plan(const std::vector<Node>& nodes, std::vector<std::size_t>* order,
          std::string* error) {
    if(!order) return fail(error, "Missing output for linked-body plan.");
    std::unordered_map<std::string, std::size_t> byId;
    byId.reserve(nodes.size());
    for(std::size_t i=0; i<nodes.size(); ++i) {
        if(nodes[i].id.empty() || !byId.emplace(nodes[i].id, i).second)
            return fail(error, "Linked-body IDs must be nonempty and unique.");
    }
    std::vector<std::size_t> degree(nodes.size(), 0);
    std::vector<std::vector<std::size_t>> dependents(nodes.size());
    for(std::size_t i=0; i<nodes.size(); ++i) {
        const auto& node=nodes[i];
        if(!node.derived) {
            if(!node.primary.empty() || !node.tool.empty() || node.unary || !node.additionalSources.empty())
                return fail(error, "Ordinary body cannot carry linked-body inputs.");
            continue;
        }
        if(node.primary.empty() || (node.unary ? !node.tool.empty() : (node.tool.empty() || node.primary==node.tool)))
            return fail(error, "Derived body requires one sketch source or two distinct Boolean sources.");
        std::unordered_set<std::string> uniqueInputs;
        std::vector<std::string> allInputs={node.primary};
        if(!node.tool.empty())allInputs.push_back(node.tool);
        allInputs.insert(allInputs.end(),node.additionalSources.begin(),node.additionalSources.end());
        for(const auto& input:allInputs){
            if(input.empty()||!uniqueInputs.insert(input).second)
                return fail(error,"Linked-body sources must be nonempty and distinct.");
            const auto found=byId.find(input);
            if(found==byId.end())
                return fail(error,"Linked-body dependency refers to a missing source.");
            if(found->second==i)
                return fail(error,"A linked body cannot depend on itself.");
            ++degree[i];
            dependents[found->second].push_back(i);
        }
    }
    std::vector<std::size_t> result;
    result.reserve(nodes.size());
    // A min-heap picks the smallest ready document index, preserving the
    // *exact* stable order of the old repeated linear scan. This reduces
    // topological planning from O(V^2 + E) to O((V + E) log V) for projects
    // containing thousands of linked bodies.
    std::priority_queue<std::size_t,std::vector<std::size_t>,
                        std::greater<std::size_t>> ready;
    for(std::size_t i=0;i<nodes.size();++i)
        if(degree[i]==0)ready.push(i);
    for(std::size_t step=0;step<nodes.size();++step){
        if(ready.empty())
            return fail(error,"Linked-body dependency graph contains a cycle.");
        const std::size_t chosen=ready.top();ready.pop();
        result.push_back(chosen);
        for(const std::size_t user:dependents[chosen])
            if(--degree[user]==0)ready.push(user);
    }
    *order=std::move(result);
    return true;
}

bool affectedMany(const std::vector<Node>& nodes,
                  const std::vector<std::string>& changedIds,
                  std::vector<std::size_t>* indices, std::string* error) {
    if(!indices) return fail(error,"Missing output for linked-body impact analysis.");
    std::vector<std::size_t> order;
    if(!plan(nodes,&order,error)) return false;
    std::unordered_set<std::string> selected;
    selected.reserve(changedIds.size());
    for(const auto& id : changedIds) {
        if(id.empty() || !selected.insert(id).second)
            return fail(error,"Impact source IDs must be nonempty and unique.");
    }
    std::unordered_set<std::string> known;
    known.reserve(nodes.size());
    for(const auto& node:nodes) known.insert(node.id);
    for(const auto& id:selected)
        if(!known.count(id)) return fail(error,"Affected body does not exist.");
    std::vector<std::size_t> result;
    result.reserve(nodes.size());
    for(std::size_t i:order){
        const auto& node=nodes[i];
        if(selected.count(node.id) || (node.derived &&
           (selected.count(node.primary)||(!node.unary&&selected.count(node.tool))||
            std::any_of(node.additionalSources.begin(),node.additionalSources.end(),
                [&](const auto& source){return selected.count(source)>0;})))){
            selected.insert(node.id);
            result.push_back(i);
        }
    }
    *indices=std::move(result);
    return true;
}

bool explainAffectedMany(const std::vector<Node>& nodes,
                         const std::vector<std::string>& changedIds,
                         std::vector<ImpactCause>* causes,
                         std::string* error) {
    if(!causes) return fail(error,"Missing output for linked-body cause analysis.");
    std::vector<std::size_t> affectedOrder;
    if(!affectedMany(nodes,changedIds,&affectedOrder,error))return false;
    const std::unordered_set<std::string> changed(changedIds.begin(),changedIds.end());
    std::unordered_map<std::string,ImpactCause> known;
    known.reserve(affectedOrder.size());
    std::vector<ImpactCause> result;
    result.reserve(affectedOrder.size());
    for(std::size_t index:affectedOrder){
        const Node& node=nodes[index];
        ImpactCause candidate;
        candidate.nodeIndex=index;
        if(changed.count(node.id)){
            candidate.changedSourceId=node.id;
            candidate.pathIds={node.id};
        }else{
            std::vector<std::string> sources{node.primary};
            if(!node.tool.empty())sources.push_back(node.tool);
            sources.insert(sources.end(),node.additionalSources.begin(),node.additionalSources.end());
            bool found=false;
            for(const auto& source:sources){
                const auto previous=known.find(source);
                if(previous==known.end())continue;
                ImpactCause proposed=previous->second;
                proposed.nodeIndex=index;
                proposed.pathIds.push_back(node.id);
                if(!found||proposed.pathIds.size()<candidate.pathIds.size()||
                   (proposed.pathIds.size()==candidate.pathIds.size()&&
                    proposed.pathIds<candidate.pathIds)){
                    candidate=std::move(proposed);
                    found=true;
                }
            }
            if(!found)return fail(error,"An affected body has no causally tracked predecessor.");
        }
        known.emplace(node.id,candidate);
        result.push_back(std::move(candidate));
    }
    *causes=std::move(result);
    if(error)error->clear();
    return true;
}

bool planAffectedWaves(const std::vector<Node>& nodes,
                       const std::vector<std::string>& changedIds,
                       std::vector<RebuildWave>* waves,
                       std::string* error){
    if(!waves)return fail(error,"Missing destination for rebuild-wave plan.");
    std::vector<std::size_t> ordered;
    if(!affectedMany(nodes,changedIds,&ordered,error))return false;
    std::unordered_map<std::string,std::size_t> stagedDepth;
    stagedDepth.reserve(ordered.size());
    std::vector<RebuildWave> result;
    for(std::size_t index:ordered){
        const Node& node=nodes[index];
        std::size_t depth=0;
        if(node.derived){
            std::vector<std::string> inputs{node.primary};
            if(!node.unary)inputs.push_back(node.tool);
            inputs.insert(inputs.end(),node.additionalSources.begin(),node.additionalSources.end());
            for(const auto& input:inputs){
                const auto previous=stagedDepth.find(input);
                if(previous!=stagedDepth.end())depth=std::max(depth,previous->second+1);
            }
        }
        stagedDepth.emplace(node.id,depth);
        if(result.size()<=depth)result.resize(depth+1);
        result[depth].dependencyDepth=depth;
        result[depth].nodeIndices.push_back(index);
    }
    *waves=std::move(result);
    if(error)error->clear();
    return true;
}

bool affected(const std::vector<Node>& nodes, const std::string& changedId,
              std::vector<std::size_t>* indices, std::string* error) {
    return affectedMany(nodes,{changedId},indices,error);
}

}  // namespace cad::linkedgraph

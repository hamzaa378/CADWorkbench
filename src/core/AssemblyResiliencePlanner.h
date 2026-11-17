#pragma once

#include "AssemblyNetworkStructureAudit.h"
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// A graph-theoretic proposal for distinct mechanical support paths. It does
// NOT create constraints, assert their geometric solvability, or certify
// safety/reliability. Every suggested connection requires a real assembly mate
// and a separate geometric-closure and motion/collision review.
namespace cad::mates {
struct StructuralReinforcement {
    std::vector<std::pair<std::size_t,std::size_t>> suggestedConnections;
    std::size_t vulnerableFixedFrameLinksBefore=0;
    std::size_t vulnerableFixedFrameLinksAfter=0; // simulated graph only
    std::vector<std::size_t> alreadyUnreachable;
};

// Minimal number ceil(L/2) of extra graph links for a connected, bridged
// undirected graph with L leaf blocks in its bridge-block tree. All endpoints
// are actual occurrences, not synthetic block IDs. Works independently from
// the kinematics/mate-constraint solver and never changes the document.
inline bool planStructuralReinforcement(
        std::size_t nodeCount,std::size_t fixedNode,
        const std::vector<StructuralMateLink>& links,
        StructuralReinforcement* output,std::string* error=nullptr){
    if(!output){if(error)*error="Missing structural-reinforcement destination.";return false;}
    StructuralMateAudit audit;
    if(!auditMateStructure(nodeCount,fixedNode,links,&audit,error))return false;
    StructuralReinforcement pending;
    pending.alreadyUnreachable=audit.unreachableNodes;
    // Ignore disconnected subgraphs: adding a graph edge is not a substitute
    // for the engineer selecting a real mate between physical occurrences.
    std::vector<bool> reachable(nodeCount,true);
    for(auto index:audit.unreachableNodes)reachable[index]=false;
    std::set<std::size_t> bridgeIndices;
    for(const auto& impact:audit.bridgeImpacts)
        if(!impact.detachedFromFixed.empty())bridgeIndices.insert(impact.linkIndex);
    pending.vulnerableFixedFrameLinksBefore=bridgeIndices.size();
    if(bridgeIndices.empty()){
        *output=std::move(pending);if(error)error->clear();return true;
    }
    // Contract every maximal 2-edge-connected block. The resulting bridge
    // graph is a tree for the connected component containing the fixed node.
    std::vector<std::size_t> parent(nodeCount);
    for(std::size_t i=0;i<nodeCount;++i)parent[i]=i;
    const auto root=[&](const auto& self,std::size_t i)->std::size_t{
        if(parent[i]==i)return i;
        return parent[i]=self(self,parent[i]);
    };
    for(std::size_t i=0;i<links.size();++i){
        const auto& link=links[i];
        if(!reachable[link.parent]||!reachable[link.child]||bridgeIndices.count(i))continue;
        auto a=root(root,link.parent),b=root(root,link.child);
        if(a!=b)parent[b]=a;
    }
    std::map<std::size_t,std::size_t> blockIds;
    std::vector<std::size_t> nodeBlocks(nodeCount,nodeCount);
    std::vector<std::size_t> representative;
    for(std::size_t node=0;node<nodeCount;++node){
        if(!reachable[node])continue;
        const auto key=root(root,node);
        auto located=blockIds.find(key);
        if(located==blockIds.end()){
            const std::size_t index=blockIds.size();
            located=blockIds.emplace(key,index).first;
            representative.push_back(node);
        }
        nodeBlocks[node]=located->second;
    }
    representative[nodeBlocks[fixedNode]]=fixedNode;
    std::vector<std::vector<std::size_t>> tree(blockIds.size());
    for(auto i:bridgeIndices){
        const auto& link=links[i];
        const auto a=nodeBlocks[link.parent],b=nodeBlocks[link.child];
        if(a==b||a>=tree.size()||b>=tree.size()){
            if(error)*error="Inconsistent bridge-block tree.";return false;
        }
        tree[a].push_back(b);tree[b].push_back(a);
    }
    for(auto& row:tree){std::sort(row.begin(),row.end());row.erase(std::unique(row.begin(),row.end()),row.end());}
    std::vector<std::size_t> leaves;
    std::vector<bool> seen(tree.size(),false);
    const auto dfs=[&](const auto& self,std::size_t index)->void{
        seen[index]=true;
        if(tree[index].size()==1)leaves.push_back(index);
        for(auto next:tree[index])if(!seen[next])self(self,next);
    };
    dfs(dfs,nodeBlocks[fixedNode]);
    if(leaves.size()<2){if(error)*error="Bridged assembly has no pair of leaf blocks.";return false;}
    const std::size_t half=(leaves.size()+1)/2;
    for(std::size_t i=0;i<half;++i){
        const auto a=representative[leaves[i]];
        const auto b=representative[leaves[(i+half)%leaves.size()]];
        if(a==b){if(error)*error="Invalid self-reinforcement candidate.";return false;}
        pending.suggestedConnections.emplace_back(a,b);
    }
    // Validate the proposed block-tree closure through the same audit engine
    // used for the original assembly. Do not report simulated success unless
    // every former fixed-component bridge is eliminated.
    auto simulated=links;
    for(const auto& [a,b]:pending.suggestedConnections)
        simulated.push_back({a,b,1});
    StructuralMateAudit updated;
    std::string reason;
    if(!auditMateStructure(nodeCount,fixedNode,simulated,&updated,&reason)){
        if(error)*error="Cannot verify proposed graph links: "+reason;return false;
    }
    for(const auto& impact:updated.bridgeImpacts)
        if(!impact.detachedFromFixed.empty())++pending.vulnerableFixedFrameLinksAfter;
    if(pending.vulnerableFixedFrameLinksAfter!=0){
        if(error)*error="Proposed connections did not eliminate all fixed-frame bridge links.";
        return false;
    }
    *output=std::move(pending);
    if(error)error->clear();
    return true;
}
} // namespace cad::mates

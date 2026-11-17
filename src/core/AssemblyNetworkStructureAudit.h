#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

// Pure structural diagnostics. This is NOT a replacement for the 6-DOF
// geometric mate solver and does not certify kinematic closure.
namespace cad::mates {
struct StructuralMateLink {
    std::size_t parent=0,child=0,observations=0;
};
struct BridgeImpact {
    std::size_t linkIndex=0;
    // Nodes no longer reachable from the fixed occurrence when this single
    // link is removed. Components already disconnected are excluded.
    std::vector<std::size_t> detachedFromFixed;
};
struct OccurrenceImpact {
    std::size_t occurrenceIndex=0;
    // Already-disconnected occurrences and the removed occurrence are not
    // counted. These are graph paths only, not joint-coordinate constraints.
    std::vector<std::size_t> detachedFromFixed;
};
struct StructuralMateAudit {
    std::size_t connectedComponents=0;
    std::size_t independentCycles=0;
    std::vector<std::size_t> unreachableNodes;
    std::vector<std::size_t> bridgeLinkIndices;
    std::vector<BridgeImpact> bridgeImpacts;
    std::vector<OccurrenceImpact> criticalOccurrenceImpacts;
    std::vector<std::size_t> cycleLinkIndices;
    // Number of independent physical mate-link paths to the fixed occurrence,
    // capped at 3 (3 means >=3). 0 means disconnected or fixed itself.
    // This is graph redundancy, NOT a load/kinematic constraint certification.
    std::vector<unsigned char> independentPathsToFixed;
    bool connected=false;
};
inline bool auditMateStructure(std::size_t nodeCount,std::size_t fixedNode,
        const std::vector<StructuralMateLink>& links,StructuralMateAudit* output,
        std::string* error=nullptr){
    const auto fail=[&](const char* text){if(error)*error=text;return false;};
    if(!output||nodeCount==0||nodeCount>256||fixedNode>=nodeCount||links.size()>4096)
        return fail("Rigid-mate structural audit expects 1-256 nodes, a valid fixed node, and at most 4096 links.");
    for(const auto& link:links)if(link.parent>=nodeCount||link.child>=nodeCount||
            link.parent==link.child||link.observations==0)
        return fail("Mate link has a missing endpoint, self-reference, or no observations.");
    std::vector<std::vector<std::pair<std::size_t,std::size_t>>> edges(nodeCount);
    for(std::size_t i=0;i<links.size();++i){
        edges[links[i].parent].push_back({links[i].child,i});
        edges[links[i].child].push_back({links[i].parent,i});
    }
    for(auto& row:edges)std::sort(row.begin(),row.end());
    std::vector<int> entered(nodeCount,-1),low(nodeCount,-1),component(nodeCount,-1);
    std::vector<bool> bridges(links.size(),false);int tick=0,group=0;
    std::function<void(std::size_t,std::size_t)> walk = [&](std::size_t v,std::size_t incoming){
        entered[v]=low[v]=tick++;component[v]=group;
        for(const auto& [next,index]:edges[v]){
            if(index==incoming)continue;
            if(entered[next]==-1){
                walk(next,index);low[v]=std::min(low[v],low[next]);
                if(low[next]>entered[v])bridges[index]=true;
            } else low[v]=std::min(low[v],entered[next]);
        }
    };
    for(std::size_t i=0;i<nodeCount;++i)if(entered[i]==-1){
        walk(i,std::numeric_limits<std::size_t>::max());++group;
    }
    StructuralMateAudit result;
    result.connectedComponents=static_cast<std::size_t>(group);
    result.connected=group==1;
    result.independentCycles=links.size()+static_cast<std::size_t>(group)-nodeCount;
    for(std::size_t i=0;i<nodeCount;++i)if(component[i]!=component[fixedNode])
        result.unreachableNodes.push_back(i);
    for(std::size_t i=0;i<links.size();++i){
        if(bridges[i])result.bridgeLinkIndices.push_back(i);
        else result.cycleLinkIndices.push_back(i);
    }
    // Report real component isolation caused by losing a single bridge,
    // relative to the fixed node. Never call this a geometric mate result.
    for(const auto bridgeIndex:result.bridgeLinkIndices){
        std::vector<bool> seen(nodeCount,false);
        std::vector<std::size_t> pending{fixedNode};seen[fixedNode]=true;
        for(std::size_t head=0;head<pending.size();++head){
            for(const auto& [next,index]:edges[pending[head]]){
                if(index==bridgeIndex||seen[next])continue;
                seen[next]=true;pending.push_back(next);
            }
        }
        BridgeImpact impact;impact.linkIndex=bridgeIndex;
        for(std::size_t node=0;node<nodeCount;++node)
            if(component[node]==component[fixedNode]&&!seen[node])
                impact.detachedFromFixed.push_back(node);
        result.bridgeImpacts.push_back(std::move(impact));
    }
    // Single-occurrence failure analysis is distinct from single-link
    // bridges: losing one component may remove *multiple* otherwise-redundant
    // connections. Only examine nodes reachable from the fixed occurrence.
    // At most 256 nodes and 4096 links: bounded O(V*(V+E)) traversal.
    for(std::size_t removed=0;removed<nodeCount;++removed){
        if(removed==fixedNode||component[removed]!=component[fixedNode])continue;
        std::vector<bool> seen(nodeCount,false);
        std::vector<std::size_t> pending{fixedNode};seen[fixedNode]=true;
        for(std::size_t head=0;head<pending.size();++head){
            for(const auto& [next,index]:edges[pending[head]]){
                (void)index;
                if(next==removed||seen[next])continue;
                seen[next]=true;pending.push_back(next);
            }
        }
        OccurrenceImpact impact;impact.occurrenceIndex=removed;
        for(std::size_t node=0;node<nodeCount;++node)
            if(node!=removed&&component[node]==component[fixedNode]&&!seen[node])
                impact.detachedFromFixed.push_back(node);
        if(!impact.detachedFromFixed.empty())
            result.criticalOccurrenceImpacts.push_back(std::move(impact));
    }
    // For each reachable occurrence, find up to three edge-disjoint paths to
    // the fixed occurrence by unit-capacity max flow. Parallel saved links
    // count independently. The residual capacities preserve undirected link
    // usage and prevent one shared connection from being double counted.
    // The cap of three keeps this bounded for dense, 256-node CAD assemblies.
    result.independentPathsToFixed.assign(nodeCount,0);
    for(std::size_t target=0;target<nodeCount;++target){
        if(target==fixedNode||component[target]!=component[fixedNode])continue;
        std::vector<std::vector<int>> residual(nodeCount,std::vector<int>(nodeCount,0));
        for(const auto& link:links){
            ++residual[link.parent][link.child];
            ++residual[link.child][link.parent];
        }
        unsigned char flow=0;
        for(;flow<3;){
            std::vector<int> parent(nodeCount,-1);
            std::vector<std::size_t> queue{fixedNode};
            parent[fixedNode]=static_cast<int>(fixedNode);
            for(std::size_t head=0;head<queue.size()&&parent[target]<0;++head){
                const std::size_t from=queue[head];
                for(std::size_t next=0;next<nodeCount;++next){
                    if(parent[next]>=0||residual[from][next]<=0)continue;
                    parent[next]=static_cast<int>(from);
                    queue.push_back(next);
                    if(next==target)break;
                }
            }
            if(parent[target]<0)break;
            for(std::size_t v=target;v!=fixedNode;){
                const std::size_t u=static_cast<std::size_t>(parent[v]);
                --residual[u][v];++residual[v][u];v=u;
            }
            ++flow;
        }
        result.independentPathsToFixed[target]=flow;
    }
    *output=std::move(result);if(error)error->clear();return true;
}
} // namespace cad::mates

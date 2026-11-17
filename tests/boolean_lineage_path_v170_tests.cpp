#include "core/BooleanLineagePath.h"
#include "core/BranchReferenceAgreement.h"
#include <limits>
#include <stdexcept>
#include <iostream>
#include <map>
#include <string>
#include <vector>
int main(){
    using cad::lineage::uniqueBooleanPath;
    std::map<std::string,std::vector<std::string>> ancestry{
        {"cut-1",{"@base","cutter"}},
        {"cut-2",{"cut-1","cutter-2"}},
        {"fused",{"cut-2","other"}},
        {"left",{"@base","tool-left"}},
        {"right",{"@base","tool-right"}},
        {"diamond",{"left","right"}},
        {"broken",{"transform","tool"}},
        {"cycle-1",{"cycle-2"}},
        {"cycle-2",{"cycle-1"}}
    };
    std::vector<std::string> path={"sentinel"};std::string error;
    int checks=0;
    const auto must=[&](bool condition){++checks;if(!condition)throw std::runtime_error("Boolean lineage path check " + std::to_string(checks) + " failed: " + error);};
    must(uniqueBooleanPath(ancestry,"@base","fused",&path,&error));
    must(path==std::vector<std::string>({"cut-1","cut-2","fused"}));
    must(uniqueBooleanPath(ancestry,"cut-1","fused",&path,&error));
    must(path==std::vector<std::string>({"cut-2","fused"}));
    must(uniqueBooleanPath(ancestry,"other","fused",&path,&error));
    must(path==std::vector<std::string>({"fused"}));
    const auto previous=path;
    must(!uniqueBooleanPath(ancestry,"@base","diamond",&path,&error));
    must(error.find("multiple branches")!=std::string::npos);
    must(path==previous);
    must(!uniqueBooleanPath(ancestry,"@base","broken",&path,&error));
    must(path==previous);
    must(!uniqueBooleanPath(ancestry,"@base","missing",&path,&error));
    must(!uniqueBooleanPath(ancestry,"@base","@base",&path,&error));
    must(!uniqueBooleanPath(ancestry,"@base","cycle-1",&path,&error));
    must(!uniqueBooleanPath(ancestry,"@base","fused",nullptr,&error));
    // Earlier source branch embedded in a shared ancestry chain is valid.
    ancestry["cut-3"]={"fused","another"};
    must(uniqueBooleanPath(ancestry,"cut-1","cut-3",&path,&error));
    must(path==std::vector<std::string>({"cut-2","fused","cut-3"}));
    // The graph planner can follow an exact rigid stage interleaved between
    // Boolean operations, provided the caller records its source and can
    // perform a deterministic transformed-witness remap for that stage.
    ancestry["rigid-turn"]={"cut-2"};
    ancestry["cut-after-turn"]={"rigid-turn","independent-tool"};
    must(uniqueBooleanPath(ancestry,"@base","cut-after-turn",&path,&error));
    must(path==std::vector<std::string>({"cut-1","cut-2","rigid-turn","cut-after-turn"}));
    ancestry["ambiguous-rigid"]={"@base","rigid-turn"};
    must(!uniqueBooleanPath(ancestry,"@base","ambiguous-rigid",&path,&error));
    // A second route to the same source must fail rather than pick one.
    ancestry["fused"]={"cut-2","cut-1"};
    must(!uniqueBooleanPath(ancestry,"cut-1","fused",&path,&error));
    must(path==std::vector<std::string>({"cut-1","cut-2","rigid-turn","cut-after-turn"}));
    // Multi-parent Boolean merges are only eligible for acceptance after
    // the kernel rebase agrees for EVERY enumerated route.
    std::vector<std::vector<std::string>> routes;
    ancestry["fused"]={"cut-2","other"};
    must(cad::lineage::allTrackedPaths(ancestry,"@base","diamond",&routes,&error));
    must(routes.size()==2);
    must(routes[0]==std::vector<std::string>({"left","diamond"}));
    must(routes[1]==std::vector<std::string>({"right","diamond"}));
    must(cad::lineage::allTrackedPaths(ancestry,"@base","cut-after-turn",&routes,&error));
    must(routes.size()==1);
    must(routes[0]==std::vector<std::string>({"cut-1","cut-2","rigid-turn","cut-after-turn"}));
    const auto retainedRoutes=routes;
    must(!cad::lineage::allTrackedPaths(ancestry,"@base","missing",&routes,&error));
    must(routes==retainedRoutes);
    must(!cad::lineage::allTrackedPaths(ancestry,"@base","cycle-1",&routes,&error));
    must(routes==retainedRoutes);
    must(!cad::lineage::allTrackedPaths(ancestry,"@base","diamond",nullptr,&error));
    ancestry["source"]={"@base"};
    for(int i=0;i<10;++i)ancestry["b"+std::to_string(i)]={"source","@base"};
    ancestry["fan"]={"b0","b1","b2","b3","b4","b5","b6","b7","b8","b9"};
    // Twenty paths are supported for a more complex industrial assembly DAG.
    must(cad::lineage::allTrackedPaths(ancestry,"@base","fan",&routes,&error));
    must(routes.size()==20);
    const auto twentyRoutes=routes;
    // Repeating a parent edge is not a second real ancestry route.
    ancestry["deduplicated"]={"source","source"};
    must(cad::lineage::allTrackedPaths(ancestry,"@base","deduplicated",&routes,&error));
    must(routes.size()==1);
    // A malformed or very wide dependency graph remains bounded and must not
    // overwrite a previously returned valid result on rejection.
    const auto deduplicatedRoutes=routes;
    for(int i=10;i<17;++i)ancestry["b"+std::to_string(i)]={"source","@base"};
    for(int i=10;i<17;++i)ancestry["fan"].push_back("b"+std::to_string(i));
    must(!cad::lineage::allTrackedPaths(ancestry,"@base","fan",&routes,&error));
    must(error.find("32 routes")!=std::string::npos);
    must(routes==deduplicatedRoutes);
    // Distinct upstream machining locations must not be accepted merely
    // because two Boolean branches end on the same full-length edge.
    using cad::lineage::samePhysicalWitness;
    must(samePhysicalWitness({6.0,0.0,0.0},{6.0,0.0,0.0}));
    must(samePhysicalWitness({6.0,0.0,0.0},{6.00001,0.0,0.0}));
    must(!samePhysicalWitness({6.0,0.0,0.0},{9.0,0.0,0.0}));
    must(!samePhysicalWitness({0.0,0.0,0.0},{0.0,0.0,0.0002}));
    must(!samePhysicalWitness({0.0,0.0,0.0},{0.0,0.0,0.0},0.0));
    must(!samePhysicalWitness({std::numeric_limits<double>::quiet_NaN(),0.0,0.0},
                              {0.0,0.0,0.0}));
    must(!samePhysicalWitness({0.0,0.0,0.0},
                              {std::numeric_limits<double>::infinity(),0.0,0.0}));
    (void)twentyRoutes;
    std::cout<<checks<<" Boolean ancestry planner/witness checks passed\n";
    return 0;
}

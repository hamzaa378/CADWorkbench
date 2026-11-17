#include "core/LinkedBodyGraph.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace {
int checks=0;
void check(bool condition,const char* description) {
    ++checks;
    if(!condition) throw std::runtime_error(description);
}
using cad::linkedgraph::Node;
std::size_t index(const std::vector<std::size_t>& order,std::size_t value) {
    const auto it=std::find(order.begin(),order.end(),value);
    return it==order.end()?order.size():static_cast<std::size_t>(it-order.begin());
}
}
int main(){
    try{
        std::string error;
        std::vector<std::size_t> order;
        const std::vector<Node> nodes={{"A",false,{},{}}
                                      ,{"child",true,"late","A"}
                                      ,{"grandchild",true,"child","B"}
                                      ,{"B",false,{},{}}
                                      ,{"late",true,"A","B"}};
        check(cad::linkedgraph::plan(nodes,&order,&error),"Out-of-order DAG was rejected");
        check(order.size()==nodes.size(),"Incomplete graph plan");
        check(index(order,0)<index(order,4),"A must precede late");
        check(index(order,3)<index(order,4),"B must precede late");
        check(index(order,4)<index(order,1),"Later source must precede earlier child");
        check(index(order,1)<index(order,2),"Child must precede grandchild");
        std::vector<std::size_t> impacted;
        check(cad::linkedgraph::affected(nodes,"late",&impacted,&error),"Impact analysis failed");
        check((impacted==std::vector<std::size_t>{4,1,2}),"Impact analysis contained unrelated nodes");
        check(cad::linkedgraph::affected(nodes,"B",&impacted,&error),"Second impact analysis failed");
        check(impacted.size()==4,"B should affect three linked dependents");
        check(cad::linkedgraph::affected(nodes,"A",&impacted,&error),"Root impact failed");
        check(impacted.size()==4,"A should affect exactly three linked dependents");
        // Architecture regression: the impact explanation shares the same
        // source graph and preserves exact rebuild order and short causality.
        std::vector<cad::linkedgraph::ImpactCause> causes;
        check(cad::linkedgraph::explainAffectedMany(nodes,{"late"},&causes,&error),
              "Detailed linked-body impact failed");
        check(causes.size()==3&&causes[0].nodeIndex==4&&causes[1].nodeIndex==1&&
              causes[2].nodeIndex==2,"Impact cause order differs from rebuild order");
        check((causes[2].pathIds==std::vector<std::string>{"late","child","grandchild"}),
              "Deep transitive cause is missing a dependency hop");
        check(cad::linkedgraph::explainAffectedMany(nodes,{"A","B"},&causes,&error),
              "Multi-source cause analysis failed");
        check(causes.size()==5,"Multiple roots did not cover every affected result");
        for(const auto& item:causes){
            check(!item.pathIds.empty()&&item.pathIds.back()==nodes[item.nodeIndex].id,
                  "Cause path must terminate at its affected body");
            check(item.changedSourceId=="A"||item.changedSourceId=="B",
                  "Cause path must originate from a selected root");
        }
        const auto beforeCauses=causes;
        check(!cad::linkedgraph::explainAffectedMany(nodes,{"A","A"},&causes,&error),
              "Cause graph accepted duplicated root");
        check(causes.size()==beforeCauses.size()&&causes[0].pathIds==beforeCauses[0].pathIds,
              "Rejected cause analysis changed previous results");
        check(!cad::linkedgraph::explainAffectedMany(nodes,{"missing"},&causes,&error),
              "Cause graph accepted missing root");
        check(!cad::linkedgraph::explainAffectedMany(nodes,{"A"},nullptr,&error),
              "Cause graph accepted missing destination");
        // A sketch-driven body with four distinct sources must invalidate when
        // ANY one of its ordered stages changes, regardless of document order.
        auto multigraph=nodes;
        Node multi{"stack",true,"A","B"};
        multi.additionalSources={"late","grandchild"};
        multigraph.push_back(multi);
        std::vector<std::size_t> multiOrder;
        check(cad::linkedgraph::plan(multigraph,&multiOrder,&error),"Multi-source graph plan rejected");
        check(index(multiOrder,1)<index(multiOrder,5),"Derived stage source must precede its target");
        check(index(multiOrder,2)<index(multiOrder,5),"Late stage source must precede target");
        for(const char* input : {"A","B","late","grandchild"}){
            std::vector<std::size_t> multiAffected;
            check(cad::linkedgraph::affected(multigraph,input,&multiAffected,&error),"Stage impact failed");
            check(std::find(multiAffected.begin(),multiAffected.end(),5)!=multiAffected.end(),
                  "Extra sketch source does not invalidate target");
        }
        auto multiInvalid=multigraph;
        multiInvalid[5].additionalSources.push_back("A");
        check(!cad::linkedgraph::plan(multiInvalid,&multiOrder,&error),"Repeated extra stage input accepted");
        multiInvalid=multigraph;multiInvalid[5].additionalSources.push_back("missing");
        check(!cad::linkedgraph::plan(multiInvalid,&multiOrder,&error),"Missing extra stage input accepted");
        multiInvalid=multigraph;multiInvalid[5].additionalSources.push_back("stack");
        check(!cad::linkedgraph::plan(multiInvalid,&multiOrder,&error),"Self-dependent extra stage accepted");
        multiInvalid=multigraph;multiInvalid[0].additionalSources.push_back("B");
        check(!cad::linkedgraph::plan(multiInvalid,&multiOrder,&error),"Ordinary body extra input accepted");
        // Coordinated batch impact: deduplicated transitive cones, stable
        // topological order, and no mutation if any source is invalid.
        std::vector<std::size_t> many;
        check(cad::linkedgraph::affectedMany(nodes,{"late","A"},&many,&error),
              "Multiple source impact failed");
        check(many.size()==4,"Union of affected cones lost a body");
        check(index(many,4)<index(many,1)&&index(many,1)<index(many,2),
              "Multiple source impact is not topological");
        check(cad::linkedgraph::affectedMany(nodes,{},&many,&error),
              "Empty edit batch should be valid");
        check(many.empty(),"Empty batch returned false positives");
        const auto savedMany=many;
        check(!cad::linkedgraph::affectedMany(nodes,{"A","A"},&many,&error),
              "Duplicate batch sources accepted");
        check(many==savedMany,"Invalid batch clobbered impact output");
        check(!cad::linkedgraph::affectedMany(nodes,{"missing"},&many,&error),
              "Unknown batch source accepted");
        check(many==savedMany,"Unknown batch source clobbered impact output");
        check(!cad::linkedgraph::affectedMany(nodes,{"A"},nullptr,&error),
              "Missing batch destination accepted");
        auto invalid=nodes;
        invalid[4].primary="child";  // child -> late -> child cycle
        const std::vector<std::size_t> savedOrder=order;
        check(!cad::linkedgraph::plan(invalid,&order,&error),"Cycle accepted");
        check(order==savedOrder,"Failed graph plan changed output");
        invalid=nodes;invalid[1].tool="not-found";
        check(!cad::linkedgraph::plan(invalid,&order,&error),"Missing input accepted");
        invalid=nodes;invalid[1].tool="late";
        check(!cad::linkedgraph::plan(invalid,&order,&error),"Duplicate source accepted");
        invalid=nodes;invalid[1].primary="child";
        check(!cad::linkedgraph::plan(invalid,&order,&error),"Self-reference accepted");
        invalid=nodes;invalid[2].id="A";
        check(!cad::linkedgraph::plan(invalid,&order,&error),"Duplicate node ID accepted");
        invalid=nodes;invalid[0].id="";
        check(!cad::linkedgraph::plan(invalid,&order,&error),"Empty node ID accepted");
        invalid=nodes;invalid[0].tool="B";
        check(!cad::linkedgraph::plan(invalid,&order,&error),"Ordinary body inputs accepted");
        const std::vector<std::size_t> oldImpact=impacted;
        check(!cad::linkedgraph::affected(nodes,"missing",&impacted,&error),"Unknown impact source accepted");
        check(impacted==oldImpact,"Failed impact analysis changed output");
        check(!cad::linkedgraph::plan(nodes,nullptr,&error),"Missing output accepted");
        check(cad::linkedgraph::plan({},&order,&error),"Empty graph rejected");
        check(order.empty(),"Empty graph generated nodes");
        // Exercise large independent graphs and deeply nested dependencies.
        for(int count: {2,3,20,250}) {
            std::vector<Node> large;
            large.reserve(static_cast<std::size_t>(count));
            for(int i=0;i<count;++i){
                Node n; n.id=std::to_string(i);
                if(i>=2){ n.derived=true;n.primary=std::to_string(i-1);n.tool="0"; }
                large.push_back(std::move(n));
            }
            check(cad::linkedgraph::plan(large,&order,&error),"Large graph planning failed");
            check(order.size()==large.size(),"Large graph missing elements");
            check(cad::linkedgraph::affected(large,"0",&impacted,&error),"Large impact analysis failed");
            check(impacted.size()==large.size()-static_cast<std::size_t>(count>1?1:0),
                  "Large impact analysis has incorrect reachability");
        }
        // Unary sketch-derived bodies and binary Booleans share one DAG.
        std::vector<Node> sketchGraph={{"source",false,{},{}},
                                       {"result",true,"sketch", "source"},
                                       {"sketch",true,"source",{},true}};
        check(cad::linkedgraph::plan(sketchGraph,&order,&error),"Unary sketch chain rejected");
        check((order==std::vector<std::size_t>{0,2,1}),"Unary graph order wrong");
        check(cad::linkedgraph::affected(sketchGraph,"source",&impacted,&error),
              "Unary graph impact failed");
        check((impacted==std::vector<std::size_t>{0,2,1}),
              "Unary sketch descendant missing");
        sketchGraph[2].primary="result";
        check(!cad::linkedgraph::plan(sketchGraph,&order,&error),
              "Mixed unary and Boolean dependency cycle accepted");
        sketchGraph[2].primary="source";
        // Two independently editable in-place modifier sources must both
        // invalidate the same target, followed by its Boolean descendants.
        const std::vector<Node> twoStage={{"A",false,{},{}},
                                           {"dependent",true,"target","A"},
                                           {"target",true,"A","B"},
                                           {"B",false,{},{}}};
        check(cad::linkedgraph::plan(twoStage,&order,&error),
              "Two-source in-place modifier was rejected");
        check(index(order,0)<index(order,2)&&index(order,3)<index(order,2),
              "Both source sketches must precede in-place target");
        check(index(order,2)<index(order,1),"In-place result must precede downstream Boolean");
        check(cad::linkedgraph::affected(twoStage,"B",&impacted,&error),
              "Second modifier source did not trigger impact analysis");
        check((impacted==std::vector<std::size_t>{3,2,1}),
              "Second source did not invalidate its target and descendants");
        check(cad::linkedgraph::affected(twoStage,"A",&impacted,&error),
              "First modifier source failed impact analysis");
        check(impacted.size()==3,"First modifier source lost linked descendants");
        auto cycle=twoStage;cycle[2].tool="dependent";
        check(!cad::linkedgraph::plan(cycle,&order,&error),
              "Second modifier cyclic reference was accepted");
        sketchGraph[2].tool="invalid-second-source";
        check(!cad::linkedgraph::plan(sketchGraph,&order,&error),
              "Unary sketch source accepted a second input");
        // Backend v3: changed ancestor and descendant must be scheduled
        // in separate dependency waves, even if both are direct edit roots.
        std::vector<cad::linkedgraph::RebuildWave> waves;
        check(cad::linkedgraph::planAffectedWaves(twoStage,{"A","target"},&waves,&error),
              "Backend rebuild-wave planner rejected valid edits");
        check(waves.size()==3&&
              (waves[0].nodeIndices==std::vector<std::size_t>{0})&&
              (waves[1].nodeIndices==std::vector<std::size_t>{2})&&
              (waves[2].nodeIndices==std::vector<std::size_t>{1}),
              "Changed descendants must wait for their edited ancestors");
        std::cout<<"PASS: "<<checks<<" linked-body DAG/impact checks\n";
    }catch(const std::exception& ex){
        std::cerr<<"FAIL: "<<ex.what()<<"\n";
        return 1;
    }
    return 0;
}

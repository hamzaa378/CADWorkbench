#include "core/LinkedBodyGraph.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cad::linkedgraph;
static void expect(bool good,const char* why){if(!good)throw std::runtime_error(why);}
int main(){
try{
    int checks=0;std::string error;
    std::vector<Node> graph={{"base",false,"",""},
        {"left",true,"base","tool"},
        {"right",true,"base","tool"},
        {"final",true,"left","right"},
        {"independent",false,"",""},
        {"tool",false,"",""},
        {"downstream",true,"final","independent"}};
    std::vector<RebuildWave> waves;
    expect(planAffectedWaves(graph,{"base"},&waves,&error),"plan from base");++checks;
    expect(waves.size()==4,"four regeneration waves");++checks;
    expect((waves[0].nodeIndices==std::vector<std::size_t>{0}),"source wave");++checks;
    expect((waves[1].nodeIndices==std::vector<std::size_t>{1,2}),"parallel independent branches");++checks;
    expect((waves[2].nodeIndices==std::vector<std::size_t>{3}),"join after branches");++checks;
    expect((waves[3].nodeIndices==std::vector<std::size_t>{6}),"downstream after join");++checks;
    expect(planAffectedWaves(graph,{"base","left"},&waves,&error),"ancestor and descendant edited");++checks;
    expect(waves[1].nodeIndices==std::vector<std::size_t>({1,2}),"descendant cannot rebuild before changed parent");++checks;
    expect(planAffectedWaves(graph,{"tool","independent"},&waves,&error),"multiple independent roots");++checks;
    expect((waves[0].nodeIndices==std::vector<std::size_t>{4,5}),"roots stable document order");++checks;
    expect((waves[1].nodeIndices==std::vector<std::size_t>{1,2}),"affected branches on wave one");++checks;
    expect((waves[2].nodeIndices==std::vector<std::size_t>{3}),"join wave two");++checks;
    expect((waves[3].nodeIndices==std::vector<std::size_t>{6}),"new dependent wave three");++checks;
    const auto old=waves;
    expect(!planAffectedWaves(graph,{"base","base"},&waves,&error),"duplicated roots must fail");++checks;
    expect(waves.size()==old.size()&&waves[0].nodeIndices==old[0].nodeIndices,"failed plan cannot overwrite output");++checks;
    expect(!planAffectedWaves(graph,{"not-here"},&waves,&error),"unknown root must fail");++checks;
    expect(planAffectedWaves(graph,{},&waves,&error),"empty changes valid");++checks;
    expect(waves.empty(),"empty changes give no rebuilds");++checks;
    expect(planAffectedWaves(graph,{"independent"},&waves,&error),"single independent root");++checks;
    expect(waves.size()==2&&waves[0].nodeIndices==std::vector<std::size_t>{4}&&
           waves[1].nodeIndices==std::vector<std::size_t>{6},"only truly dependent final result");++checks;
    // A long unary feature chain with an additional source at the end.
    std::vector<Node> chain{{"p0",false,"",""},{"tool",false,"",""}};
    for(int i=1;i<=200;++i){
        Node n;n.id="p"+std::to_string(i);n.derived=true;n.unary=true;
        n.primary="p"+std::to_string(i-1);chain.push_back(n);
    }
    chain.back().additionalSources.push_back("tool");
    expect(planAffectedWaves(chain,{"p0"},&waves,&error),"long chain valid");++checks;
    expect(waves.size()==201&&waves.back().nodeIndices==std::vector<std::size_t>{201},"long chain depths");++checks;
    expect(!planAffectedWaves(chain,{"p0"},nullptr,&error),"reject missing destination");++checks;
    std::cout<<"dependency rebuild waves: "<<checks<<" checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}

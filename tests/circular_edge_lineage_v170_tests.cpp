#include "core/CircularEdgeLineage.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using cad::circular_lineage::Reference;
using cad::circular_lineage::Point3;
static int checks=0;
static void check(bool correct,const char* message){++checks;if(!correct)throw std::runtime_error(message);}
int main(){
try {
    const Reference original{{12,15,4},{0,0,1},5};
    check(cad::circular_lineage::compatible(original,{{12,15,4},{0,0,1},6}),"20% diameter increase");
    check(cad::circular_lineage::compatible(original,{{12,15,4},{0,0,-1},4.2}),"axis reversal and shrink");
    check(!cad::circular_lineage::compatible(original,{{12,15,4},{0,0,1},6.01}),"large radius change");
    check(!cad::circular_lineage::compatible(original,{{12.03,15,4},{0,0,1},5}),"translated circle");
    check(!cad::circular_lineage::compatible(original,{{12,15,4.02},{0,0,1},5}),"different axial plane");
    check(!cad::circular_lineage::compatible(original,{{12,15,4},{0,1,0},5}),"different axis");
    check(!cad::circular_lineage::compatible(original,{{12,15,4},{0,0,0},5}),"zero normal");
    check(!cad::circular_lineage::compatible(original,{{12,15,4},{0,0,1},0}),"zero radius");
    check(!cad::circular_lineage::compatible(original,{{12,15,4},{0,0,1},5},1.0),"unsafe tolerance");
    const auto nan=std::numeric_limits<double>::quiet_NaN();
    check(!cad::circular_lineage::compatible(original,{{nan,15,4},{0,0,1},5}),"nonfinite center");
    check(!cad::circular_lineage::compatible(original,{{12,15,4},{nan,0,1},5}),"nonfinite axis");
    int matches=0;
    for(const Reference r : {Reference{{12,15,4},{0,0,1},4.5},Reference{{12,15,4},{0,0,1},5.5}})
        if(cad::circular_lineage::compatible(original,r))++matches;
    check(matches==2,"ambiguous concentric candidates must both be visible to caller");
    for(int i=1;i<=100;++i){
        const double r=5.0*(1.0+0.002*i);
        check(cad::circular_lineage::compatible(original,{{12,15,4},{0,0,1},r}),"supported increase spectrum");
    }
    std::cout<<"PASS: "<<checks<<" conservative circular-edge lineage checks\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

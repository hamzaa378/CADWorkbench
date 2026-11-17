#include "core/MotionSweptClearance.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace cad::proximity;
int main() {
    int checks=0;
    auto require=[&](bool okay, const char* reason){
        ++checks;if(!okay){std::cerr<<reason<<"\n";std::abort();}
    };
    const Bounds fixture{5,0,0,6,1,1};
    const Bounds moving0{0,0,0,1,1,1};
    const Bounds moving1{10,0,0,11,1,1};
    OrderInvariantSweptTracker t(0.0);std::string error;
    require(t.observe(0.0,{{"slider",moving0},{"fixture",fixture}},&error),"first sample");
    require(t.observe(1.0,{{"fixture",fixture},{"slider",moving1}},&error),"reordered sample");
    require(t.sampleCount()==2,"sample count");
    require(t.pairs().size()==1&&t.pairs().at(0).contactSegments==1,"hidden contact");
    const auto saved=t.pairs().front();
    require(!t.observe(1.0,{{"slider",moving1},{"fixture",fixture}},&error),"reject repeated timestamp");
    require(!t.observe(2.0,{{"slider",moving1},{"slider",fixture}},&error),"reject duplicates");
    require(!t.observe(2.0,{{"slider",moving1},{"UNKNOWN",fixture}},&error),"reject changed identities");
    require(!t.observe(2.0,{{"slider",{NAN,0,0,1,1,1}},{"fixture",fixture}},&error),"reject NaN");
    require(!t.observe(2.0,{{"slider",{10,0,0,12,1,1}},{"fixture",fixture}},&error),"reject changing extents");
    require(t.sampleCount()==2&&t.pairs()[0].contactSegments==saved.contactSegments,"failures atomic");
    std::mt19937 gen(20117);
    for(int iteration=0;iteration<800;++iteration) {
        std::vector<BodyBox> frame0{{"fixture",fixture},{"slider",moving0}};
        std::vector<BodyBox> frame1{{"fixture",fixture},{"slider",moving1}};
        std::shuffle(frame0.begin(),frame0.end(),gen);
        std::shuffle(frame1.begin(),frame1.end(),gen);
        OrderInvariantSweptTracker perm(0.0);
        require(perm.observe(0,frame0,&error),"permuted frame 0");
        require(perm.observe(1,frame1,&error),"permuted frame 1");
        require(perm.pairs().size()==1&&perm.pairs()[0].contactSegments==1,"same pair after permutation");
    }
    std::cout<<checks<<" order-invariant moving-body sweep assertions passed\n";
}

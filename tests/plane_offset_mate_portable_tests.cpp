#include "core/AssemblyMateSolver.h"
#include "core/AssemblyConstraintNetwork.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace cad::mates;
static void check(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
int main(){
    try{
        std::string why;Transform solved;
        const Vec3 x{1,0,0},y{0,1,0};
        std::vector<Constraint> positive={
            {Kind::Point,{0,7,0},{0,0,0},{},{}},
            {Kind::CoaxialAxis,{0,7,0},{},x,x},
            {Kind::CoincidentPlane,{},{},y,y,7.0}
        };
        if(!solve(positive,&solved,&why))throw std::runtime_error(std::string("positive offset: ")+why);
        check(std::abs(solved.translation.y-7.0)<1e-4,"positive separation missing");
        const auto original=solved;
        auto inconsistent=positive;
        inconsistent[2].offsetMm=8;
        check(!solve(inconsistent,&solved,&why),"contradictory plane offset must fail");
        check(solved.translation.y==original.translation.y,"failure mutated output");
        auto invalid=positive;invalid[1].offsetMm=7;
        check(!solve(invalid,&solved,&why),"axis must reject plane offset");
        invalid=positive;invalid[2].offsetMm=std::numeric_limits<double>::infinity();
        check(!solve(invalid,&solved,&why),"nonfinite plane offset must reject");
        auto neg=positive;neg[0].parentPoint={0,-3,0};neg[1].parentPoint={0,-3,0};neg[2].offsetMm=-3;
        check(solve(neg,&solved,&why),"negative offset must solve");
        check(std::abs(solved.translation.y+3)<1e-4,"negative offset wrong sign");
        NetworkSolution graph;
        check(solveRigidNetwork(2,0,{{0,1,positive}},&graph,&why),"offset network solve");
        check(graph.perConnection.size()==1&&graph.worstPointOrAxisGapMm<1e-4,
              "offset must not register as closure gap");
        const auto saved=graph;
        auto contradictory=positive;contradictory[2].offsetMm=3;
        check(!solveRigidNetwork(2,0,{{0,1,contradictory}},&graph,&why),"contradictory network must fail");
        check(graph.perConnection.size()==saved.perConnection.size(),"failed network changed snapshot");
        std::cout<<"PASS: signed plane offsets, solver, network, contradictions, rollback\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

#include "core/AdvancedSketchProfiles.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <algorithm>
using namespace cad;
namespace {
int checks=0;
void expect(bool okay,const char* what){++checks;if(!okay)throw std::runtime_error(what);}
}
int main(){
 try {
    const std::vector<polygon::Point> square={{-2,-2},{2,-2},{2,2},{-2,2}};
    const std::vector<polygon::Point> radial={{4,4},{8,4},{8,16},{4,16}};
    const std::vector<advancedprofile::Point3> straight={{10,10,8},{10,10,25}};
    const std::vector<advancedprofile::Point3> dogleg={{10,10,8},{10,10,18},{10,18,26}};
    const std::vector<advancedprofile::LoftSection> sections={
        {8,{{5,5},{12,5},{12,12},{5,12}}},
        {15,{{3,3},{14,3},{14,14},{3,14}}},
        {24,{{5,5},{12,5},{12,12},{5,12}}}};
    std::string issue;
    expect(advancedprofile::validateSweep(square,straight,&issue),"straight sweep rejected");
    expect(advancedprofile::validateSweep(square,dogleg,&issue),"bent sweep rejected");
    expect(!advancedprofile::validateSweep(square,{{0,0,0}},&issue),"one-point path accepted");
    expect(!advancedprofile::validateSweep(square,{{0,0,0},{0,0,0}},&issue),"zero-length path accepted");
    expect(!advancedprofile::validateSweep(square,{{0,0,0},{0,0,8},{0,0,0}},&issue),"backtrack accepted");
    expect(!advancedprofile::validateSweep(square,{{0,0,0},{0,0,5},{5,0,5},{0,0,0}},&issue),"closed route accepted");
    expect(!advancedprofile::validateSweep(square,{{0,0,0},{std::numeric_limits<double>::quiet_NaN(),0,1}},&issue),"NaN path accepted");
    expect(!advancedprofile::validateSweep(square,{{0,0,0},{1e8,0,0}},&issue),"huge path accepted");
    expect(!advancedprofile::validateSweep({{0,0},{1,1},{0,1},{1,0}},straight,&issue),"crossing profile accepted");
    expect(advancedprofile::validateMultiLoft(sections,&issue),"3-section loft rejected");
    expect(advancedprofile::validateMultiLoft({sections[0],sections[1]},&issue),"2-section loft rejected");
    expect(!advancedprofile::validateMultiLoft({sections[0]},&issue),"one-section loft accepted");
    auto bad=sections;bad[1].z=7;
    expect(!advancedprofile::validateMultiLoft(bad,&issue),"reversed section elevations accepted");
    bad=sections;bad[1].points.pop_back();
    expect(!advancedprofile::validateMultiLoft(bad,&issue),"different vertex counts accepted");
    bad=sections;std::reverse(bad[1].points.begin(),bad[1].points.end());
    expect(!advancedprofile::validateMultiLoft(bad,&issue),"opposite winding accepted");
    bad=sections;bad[0].z=std::numeric_limits<double>::infinity();
    expect(!advancedprofile::validateMultiLoft(bad,&issue),"infinite section height accepted");
    expect(advancedprofile::validateAxisRevolve(radial,{0,0,0},{0,0,1},360,&issue),"Z revolve rejected");
    expect(advancedprofile::validateAxisRevolve(radial,{10,10,10},{1,2,3},120,&issue),"custom axis revolve rejected");
    expect(!advancedprofile::validateAxisRevolve(radial,{0,0,0},{0,0,0},120,&issue),"zero axis accepted");
    expect(!advancedprofile::validateAxisRevolve(radial,{0,0,0},{1,0,0},361,&issue),"oversized revolution accepted");
    expect(!advancedprofile::validateAxisRevolve(radial,{0,0,0},{1,0,0},0,&issue),"zero angle accepted");
    expect(!advancedprofile::validateAxisRevolve(square,{0,0,0},{1,0,0},360,&issue),"negative radial profile accepted");
    // Exercise safe bounds and numerical stability with many valid parameter combinations.
    for(int i=1;i<=100;++i){
        const double z=0.1*i;
        expect(advancedprofile::validateSweep(square,{{10,10,z},{10,10,z+5}},&issue),"sweep range invalid");
        expect(advancedprofile::validateAxisRevolve(radial,{0,0,z},{1,0,z},180,&issue),"axis range invalid");
        auto x=sections;x[0].z=z;x[1].z=z+3;x[2].z=z+6;
        expect(advancedprofile::validateMultiLoft(x,&issue),"loft range invalid");
    }
    std::cout<<"PASS: "<<checks<<" advanced sketch-profile validation assertions\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL #"<<checks<<": "<<e.what()<<"\n";return 1;}
}

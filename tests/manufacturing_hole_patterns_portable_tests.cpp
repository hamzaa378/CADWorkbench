#include "core/ManufacturingHolePatterns.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace {int assertions=0;void check(bool value,const char* message){++assertions;if(!value)throw std::runtime_error(message);} }
int main(){try{
    using namespace cad::hole_patterns;
    std::vector<Point2> result;std::string reason;
    check(grid(0,0,1,2,3,12,8,0,&result,&reason),"2x3 grid");
    check(result.size()==6,"six locations");
    check(std::abs(result.front().u+6)<1e-12&&std::abs(result.front().v+8)<1e-12,"centered grid");
    check(std::abs(result.back().u-6)<1e-12&&std::abs(result.back().v-8)<1e-12,"last station");
    check(grid(4,3,1,2,1,10,0,90,&result,&reason),"rotated grid");
    check(std::abs(result[0].u-4)<1e-9&&std::abs(result[0].v+2)<1e-9,"rotation frame");
    check(std::abs(result[1].v-8)<1e-9,"rotated next hole");
    const auto saved=result;
    check(!grid(0,0,2,2,2,4,10,0,&result,&reason),"touching pattern invalid");
    check(result.size()==saved.size()&&result[0].u==saved[0].u,"grid failure atomic");
    check(!grid(0,0,1,1,1,0,0,0,&result,&reason),"one point not a pattern");
    check(!grid(0,0,1,11,1,4,0,0,&result,&reason),"grid column maximum");
    check(!grid(0,0,1,2,2,4,4,181,&result,&reason),"grid rotation range");
    check(!grid(std::numeric_limits<double>::quiet_NaN(),0,1,2,1,5,0,0,&result,&reason),"grid nonfinite");
    check(boltCircle(0,0,1,4,20,0,&result,&reason),"four bolt circle holes");
    check(result.size()==4,"bolt count");
    check(std::abs(result[0].u-10)<1e-10&&std::abs(result[0].v)<1e-10,"bolt first center");
    check(std::abs(result[1].u)<1e-9&&std::abs(result[1].v-10)<1e-9,"bolt second center");
    check(boltCircle(1,2,1,3,20,30,&result,&reason),"rotated 3 station circle");
    check(std::abs(result.front().u-(1+10*std::cos(std::numbers::pi/6)))<1e-9,"angled bolt center");
    check(!boltCircle(0,0,3,8,12,0,&result,&reason),"overlapping PCD rejected");
    check(result.size()==3,"bolt rejection atomic");
    check(!boltCircle(0,0,1,25,12,0,&result,&reason),"bolt count limit");
    check(!boltCircle(0,0,1,4,-2,0,&result,&reason),"negative PCD");
    check(!boltCircle(0,0,1,4,20,361,&result,&reason),"angle range");
    check(!boltCircle(0,0,1,4,20,0,nullptr,&reason),"null destination");
    std::cout<<assertions<<" manufacturing hole pattern assertions passed\n";
    return 0;
} catch(const std::exception& exception){std::cerr<<"FAIL "<<exception.what()<<'\n';return 1;}}

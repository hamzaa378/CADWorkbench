#include "core/ArticulatedFrameEuler.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace cad::joint_frame;
namespace {
int checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void check(double yaw,double pitch,double roll){
    Euler input{yaw,pitch,roll},result;
    std::array<double,9> matrix,again;
    require(fromEuler(input,&matrix),"valid Euler input rejected");
    require(toEuler(matrix,&result),"valid orthonormal frame rejected");
    require(fromEuler(result,&again),"roundtrip could not be reconstructed");
    for(int k=0;k<9;++k)require(std::abs(again[k]-matrix[k])<1e-8,"roundtrip matrix drift");
}
}
int main(){try{
    const double pi=std::acos(-1.0);
    check(0,0,0);check(pi/2,0,0);check(0,pi/3,-pi/3);
    check(-pi,pi/2,pi/4);check(pi/3,-pi/2,pi/4);
    for(int i=-5;i<=5;++i)check(i*pi/6,i*pi/24,-i*pi/24);
    std::array<double,9> rotation={{1,0,0,0,1,0,0,0,1}};
    Euler target{77,77,77};
    require(!toEuler(rotation,nullptr),"null result accepted");
    rotation[0]=2;require(!toEuler(rotation,&target),"scaled matrix accepted");
    rotation[0]=1;rotation[8]=-1;
    require(!toEuler(rotation,&target),"reflection accepted");
    rotation[8]=1;rotation[0]=std::numeric_limits<double>::quiet_NaN();
    require(!toEuler(rotation,&target),"NaN accepted");
    require(target.yaw==77,"invalid input changed caller result");
    require(!fromEuler({0,pi,0},&rotation),"pitch beyond frame range accepted");
    require(!fromEuler({0,0,0},nullptr),"null frame output accepted");
    std::cout<<checks<<" articulated-frame Euler checks passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

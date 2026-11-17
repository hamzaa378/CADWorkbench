#include "core/FaceMachiningDatum.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace { int checks=0; void require(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);} }
int main(){try{
    using namespace cad::machining_datum;
    Vec3 origin{-1,-1,-1};
    const Vec3 saved{10,10,10};
    // A Boolean extension changes body mass center but does not displace
    // the original coplanar face region. The raw witness takes precedence.
    require(chooseOrigin(saved,{10,0,0},true,true,false,&origin),"merged face accepted");
    require(origin.x==10&&origin.y==10&&origin.z==10,"coplanar merge did not shift machining datum");
    require(chooseOrigin(saved,{10,5,2},true,false,false,&origin),"raw witness preferred");
    require(origin.x==10&&origin.y==10&&origin.z==10,"raw witness fixed");
    require(chooseOrigin(saved,{100,15,3},false,true,false,&origin),"translation accepted");
    require(origin.x==110&&origin.y==25&&origin.z==13,"translated witness follows real displacement");
    // Translation within a face can retain both witnesses; independent
    // equal-area and centroid-motion checks must explicitly prefer the shift.
    require(chooseOrigin(saved,{5,0,0},true,true,true,&origin),"in-plane translation accepted");
    require(origin.x==15&&origin.y==10,"in-plane translation followed");
    const Vec3 previous=origin;
    require(!chooseOrigin(saved,{100,15,3},false,false,false,&origin),"deleted face rejected");
    require(origin.x==previous.x&&origin.y==previous.y&&origin.z==previous.z,"failed choice atomic");
    require(!chooseOrigin({NAN,0,0},{0,0,0},true,false,false,&origin),"nonfinite anchor rejected");
    require(!chooseOrigin(saved,{INFINITY,0,0},false,true,false,&origin),"nonfinite displacement rejected");
    require(!chooseOrigin(saved,{0,0,0},true,false,false,nullptr),"null result rejected");
    require(!chooseOrigin({1e9,0,0},{1,0,0},false,true,false,&origin),"overflowed result rejected");
    require(origin.x==previous.x,"invalid result cannot overwrite destination");
    std::cout<<checks<<" stable face machining datum assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}

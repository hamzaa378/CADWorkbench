#include "core/DirectionalHoleSpan.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace cad::directional_holes;
namespace { int n=0; void check(bool c,const char* m){++n;if(!c)throw std::runtime_error(m);} }
int main(){try{
    const Bounds box{0,0,0,20,30,10};
    Span span;
    check(throughAllSpan(box,{10,15,10},{0,0,-1},&span),"world -Z projection");
    check(span.start<0&&span.start+span.length>10,"world -Z spans full body");
    check(throughAllSpan(box,{20,15,5},{-1,0,0},&span),"face-normal X projection");
    check(span.start<0&&span.start+span.length>20,"X axis spans complete box");
    check(throughAllSpan(box,{20,15,5},{-4,0,0},&span),"nonunit axis accepted");
    check(span.start<0&&span.start+span.length>20,"nonunit direction normalized");
    check(throughAllSpan(box,{10,15,5},{1,1,1},&span),"diagonal axis supported");
    const double fullDiagonal=60/std::sqrt(3.0);
    check(span.length>=fullDiagonal,"diagonal spans all corner projections");
    const auto previous=span;
    check(!throughAllSpan(box,{1,2,3},{0,0,0},&span),"zero axis rejected");
    check(span.start==previous.start&&span.length==previous.length,"zero axis does not overwrite result");
    check(!throughAllSpan({30,0,0,0,10,10},{0,0,0},{0,0,-1},&span),"inverted AABB rejected");
    check(!throughAllSpan(box,{NAN,0,0},{0,0,1},&span),"nonfinite entry rejected");
    check(!throughAllSpan(box,{0,0,0},{0,INFINITY,0},&span),"nonfinite direction rejected");
    check(!throughAllSpan(box,{0,0,0},{0,0,1},nullptr),"null destination rejected");
    check(throughAllSpan(box,{10,15,500},{0,0,-1},&span),"external entry handled");
    check(span.start<0&&span.start+span.length>500,"external entry spans body");
    std::cout<<n<<" directional-cutter interval assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

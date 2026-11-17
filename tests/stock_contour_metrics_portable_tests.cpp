#include "core/StockContourMetrics.h"
#include <cassert>
#include <cmath>
#include <random>
using namespace cad::contour_metrics;
static bool close(double a,double b){return std::abs(a-b)<1e-8;}
int main(){
    Result result;std::string issue;
    assert(measure({"rectangle",60,40,{}},&result,&issue));
    assert(close(result.netAreaMm2,2400)&&close(result.exposedCutLengthMm,200));
    assert(close(result.removedAreaMm2,0));
    // Edge notch introduces two cut sides; the missing edge segment is replaced
    // by the notch's interior edge of equal length.
    assert(measure({"notch",60,40,{{20,0,10,8}}},&result,&issue));
    assert(close(result.netAreaMm2,2320)&&close(result.exposedCutLengthMm,216));
    // A fully enclosed rectangular hole contributes four additional edges.
    assert(measure({"hole",60,40,{{20,10,10,8}}},&result,&issue));
    assert(close(result.exposedCutLengthMm,236));
    assert(close(result.removedAreaMm2,80));
    assert(measure({"corner",60,40,{{40,20,20,20}}},&result,&issue));
    assert(close(result.exposedCutLengthMm,200));
    // Disjoint cutouts and overlapping reliefs use the exposed material
    // boundary, not a sum of raw notch rectangles.
    assert(measure({"overlap",60,40,{{20,0,10,8},{25,0,10,8}}},&result,&issue));
    assert(close(result.netAreaMm2,2280)&&close(result.exposedCutLengthMm,216));
    const Result previous=result;
    assert(!measure({"invalid",60,40,{{58,0,10,8}}},&result,&issue));
    assert(close(previous.netAreaMm2,result.netAreaMm2));
    assert(!measure({"invalid",60,40,{}},nullptr,&issue));
    std::mt19937 rng(1708);
    for(int n=0;n<500;++n){
        const double w=20+rng()%80,h=20+rng()%80;
        std::vector<cad::contour_nesting::Rect> relief;
        for(int i=0;i<5;++i){
            const double nw=1+rng()%8,nd=1+rng()%8;
            const double x=rng()%static_cast<int>(w-nw);
            relief.push_back({x,0,nw,nd});
        }
        assert(measure({std::to_string(n),w,h,relief},&result,&issue));
        assert(result.exposedCutLengthMm>=2*(w+h)-1e-6);
        assert(result.netAreaMm2<=w*h+1e-6);
        assert(result.removedAreaMm2>=-1e-6);
    }
}

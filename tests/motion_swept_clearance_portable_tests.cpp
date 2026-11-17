#include "core/MotionSweptClearance.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
using namespace cad::proximity;
static int n=0;
static void check(bool yes,const char* why){++n;if(!yes)throw std::runtime_error(why);}
static Bounds box(double x,double y,double z,double width=1){return {x,y,z,x+width,y+width,z+width};}
static Bounds lerp(const Bounds& a,const Bounds& b,double t){
    return {a.x0+(b.x0-a.x0)*t,a.y0+(b.y0-a.y0)*t,a.z0+(b.z0-a.z0)*t,
            a.x1+(b.x1-a.x1)*t,a.y1+(b.y1-a.y1)*t,a.z1+(b.z1-a.z1)*t};
}
int main(){try{
    std::string error;LinearSweepResult report;
    auto a0=box(0,0,0),a1=box(10,0,0);
    auto b0=box(10,0,0),b1=box(0,0,0);
    check(analyzeLinearBoxSweep(a0,a1,b0,b1,0,&report,&error),"crossing sweep valid");
    check(report.everAabbContact&&report.belowSpecifiedClearance,"between-pose collision detected");
    check(std::abs(report.firstContactFraction-.45)<1e-12,"exact entry at .45");
    check(std::abs(report.lastContactFraction-.55)<1e-12,"exact exit at .55");
    check(report.minimumGapMm==0,"zero swept gap");
    SweptProximityTracker stream(.5);
    check(stream.observe(0,{{"A",a0},{"B",b0}},&error),"initial pose");
    check(stream.observe(2,{{"A",a1},{"B",b1}},&error),"moving pose");
    check(stream.pairs().size()==1&&stream.pairs()[0].contactSegments==1,"transient collision in history");
    check(stream.pairs()[0].closestAbsoluteTime>=.9&&stream.pairs()[0].closestAbsoluteTime<=1.1,"absolute contact time");
    auto oldCount=stream.sampleCount(),oldContact=stream.pairs()[0].contactSegments;
    check(!stream.observe(2,{{"A",a1},{"B",b1}},&error),"duplicate timestamp rejected");
    check(!stream.observe(3,{{"B",b1},{"A",a1}},&error),"identity order error rejected");
    check(!stream.observe(3,{{"A",box(11,0,0,2)},{"B",b1}},&error),"deformed AABB rejected");
    check(stream.sampleCount()==oldCount&&stream.pairs()[0].contactSegments==oldContact,"transactional failure");
    check(analyzeLinearBoxSweep(box(0,0,0),box(0,0,0),box(10,0,0),box(10,0,0),10,&report,&error),"stationary sweep");
    check(!report.everAabbContact&&report.minimumGapMm==9,"stationary gap");
    check(report.belowSpecifiedClearance,"threshold violation without contact");
    check(analyzeLinearBoxSweep(box(0,0,0),box(0,0,0),box(10,0,0),box(10,0,0),8,&report,&error),"stationary below threshold");
    check(!report.belowSpecifiedClearance,"stationary safe clearance");
    auto previous=report;
    check(!analyzeLinearBoxSweep(box(0,0,0),box(0,0,0),box(5,0,0),box(5,0,0,2),0,&report,&error),"non-rigid endpoints rejected");
    check(previous.minimumGapMm==report.minimumGapMm&&previous.belowSpecifiedClearance==report.belowSpecifiedClearance,"invalid unchanged");
    std::mt19937 generator(431);
    std::uniform_real_distribution<double> d(-5,5);
    for(int k=0;k<2500;++k){
        const auto x0=box(d(generator),d(generator),d(generator));
        const auto y0=box(d(generator),d(generator),d(generator));
        const auto x1=box(x0.x0+d(generator),x0.y0+d(generator),x0.z0+d(generator));
        const auto y1=box(y0.x0+d(generator),y0.y0+d(generator),y0.z0+d(generator));
        check(analyzeLinearBoxSweep(x0,x1,y0,y1,.5,&report,&error),"random sweep accepted");
        for(int j=0;j<=10;++j){
            const double t=j/10.0;
            const auto witness=aabbGap(lerp(x0,x1,t),lerp(y0,y1,t));
            check(report.minimumGapMm<=witness+1e-8,"continuous minimum no worse than a sample");
        }
        if(report.everAabbContact){
            check(report.firstContactFraction<=report.lastContactFraction,"ordered contact interval");
            check(report.firstContactFraction>=0&&report.lastContactFraction<=1,"interval within segment");
            check(aabbGap(lerp(x0,x1,(report.firstContactFraction+report.lastContactFraction)*.5),
                           lerp(y0,y1,(report.firstContactFraction+report.lastContactFraction)*.5))<1e-7,
                  "contact interval witness");
        }
    }
    std::cout<<n<<" continuous-motion broad-phase checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

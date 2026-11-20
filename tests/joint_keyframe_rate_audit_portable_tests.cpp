#include "../src/core/JointKeyframeRateAudit.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <limits>
static int count=0;
static void verify(bool value,const char* message){++count;if(!value)throw std::runtime_error(message);}
int main(){try{
    using cad::motion::RateKeyframe;
    using cad::motion::RateAudit;
    using cad::motion::auditKeyframeRates;
    using cad::motion::maximumRateBetween;
    RateAudit result;result.maximumAbsoluteRatePerSecond=812;
    std::string error;
    const std::vector<RateKeyframe> keys{{0,0},{1,90},{3,30},{4,30},{6,60}};
    verify(auditKeyframeRates(keys,-180,180,&result,&error),"valid schedule rejected");
    verify(result.segments.size()==4,"segment inventory");
    verify(result.maximumAbsoluteRatePerSecond==90&&result.maximumRateSegment==0,"peak rate");
    verify(result.directionReversals==2,"direction reversal count");
    verify(result.velocityDiscontinuities==3&&!result.boundedAcceleration,"velocity discontinuity");
    verify(result.totalAbsoluteTravel==180,"absolute travel");
    double scoped=777;
    verify(maximumRateBetween(result,1.5,3.5,&scoped,&error)&&scoped==30,"subinterval spanning pause");
    verify(maximumRateBetween(result,3.5,5,&scoped,&error)&&scoped==15,"subinterval resumption");
    verify(!maximumRateBetween(result,-1,1,&scoped,&error)&&scoped==15,"invalid window mutated result");
    const RateAudit retained=result;
    verify(!auditKeyframeRates({{0,0},{0,1}},-180,180,&result,&error),"duplicate time accepted");
    verify(result.segments.size()==retained.segments.size()&&
           result.maximumAbsoluteRatePerSecond==retained.maximumAbsoluteRatePerSecond,"failure changed report");
    verify(!auditKeyframeRates({{0,0},{1,181}},-180,180,&result,&error),"travel limit ignored");
    verify(!auditKeyframeRates({{0,0},{1,std::numeric_limits<double>::quiet_NaN()}},-180,180,&result,&error),"NaN accepted");
    verify(!auditKeyframeRates({{0,0},{1,0}},0,0,nullptr,&error),"null report accepted");
    verify(!auditKeyframeRates({{0,0},{1,0}},2,1,&result,&error),"inverted limits accepted");
    // Compare exact per-segment linear slope against the audit across a large
    // deterministic spectrum, including fractional times and negative angles.
    for(int i=1;i<=6000;++i){
        const double t=i/100.0;
        const double a=(i%311)-155, b=(i%257)-128, c=(i%193)-96;
        const std::vector<RateKeyframe> track{{0,a},{t,b},{t*2,c}};
        RateAudit audited;
        verify(auditKeyframeRates(track,-200,200,&audited,&error),"randomized valid input");
        const double first=std::abs(b-a)/t,second=std::abs(c-b)/t;
        verify(std::abs(audited.maximumAbsoluteRatePerSecond-std::max(first,second))<1e-8,
               "joint-rate maximum differs from exact linear derivative");
        verify(maximumRateBetween(audited,t,t*2,&scoped,&error)&&std::abs(scoped-second)<1e-8,
               "single segment bound incorrect");
    }
    std::cout<<count<<" keyframe-rate audit assertions passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

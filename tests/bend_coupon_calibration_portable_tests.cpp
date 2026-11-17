#include "core/BendCouponCalibration.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {int n=0;void check(bool ok,const char* what){++n;if(!ok)throw std::runtime_error(what);}
double flat(double straight,double thickness,double radius,double degrees,double k){
 return straight+degrees*3.14159265358979323846/180*(radius+k*thickness);
}}
int main(){try{using namespace cad::sheet;std::string error;
 std::vector<BendCouponFactor> factors;
 std::vector<MeasuredBendCoupon> samples={{2,2,90,100,flat(100,2,2,90,.32)},
                                            {2,6,90,100,flat(100,2,6,90,.42)}};
 check(inferBendCouponFactors(samples,&factors,&error),"valid coupon set");
 check(factors.size()==2,"two measured coupons");
 check(std::abs(factors[0].kFactor-.32)<1e-10,"K1 formula");
 check(std::abs(factors[1].kFactor-.42)<1e-10,"K2 formula");
 std::vector<CalibratedBendFactor> plan;
 check(interpolateCouponKFactors(factors,{{2,4,90},{2,2,80},{2,6,105}},&plan,&error),"interpolate supported ratios and angles");
 check(plan.size()==3,"three target bends");
 check(std::abs(plan[0].kFactor-.37)<1e-10,"intermediate radius ratio");
 check(std::abs(plan[1].kFactor-.32)<1e-10,"exact endpoint");
 check(std::abs(plan[2].kFactor-.42)<1e-10,"angle limit inclusive");
 auto saved=plan;
 check(!interpolateCouponKFactors(factors,{{2,8,90}},&plan,&error),"no upper extrapolation");
 check(plan.size()==saved.size(),"failed plan atomic");
 check(!interpolateCouponKFactors(factors,{{2,1,90}},&plan,&error),"no lower extrapolation");
 check(!interpolateCouponKFactors(factors,{{2,4,110}},&plan,&error),"angle mismatch fails");
 check(!interpolateCouponKFactors(factors,{{-2,4,90}},&plan,&error),"invalid thickness rejected");
 check(!interpolateCouponKFactors(factors,{{2,4,90}},nullptr,&error),"null output rejected");
 check(!inferBendCouponFactors({{2,2,90,100,99}},&factors,&error),"blank shorter than straight rejected");
 check(!inferBendCouponFactors({{2,2,90,100,flat(100,2,2,90,1.1)}},&factors,&error),"impossible high K rejected");
 check(!inferBendCouponFactors({{2,2,90,100,flat(100,2,2,90,-.1)}},&factors,&error),"impossible low K rejected");
 check(!inferBendCouponFactors({{2,2,0,100,105}},&factors,&error),"invalid angle rejected");
 check(!inferBendCouponFactors({{2,2,90,100,INFINITY}},&factors,&error),"infinite blank rejected");
 check(!inferBendCouponFactors({},&factors,&error),"empty dataset rejected");
 check(!inferBendCouponFactors(samples,nullptr,&error),"null measured output rejected");
 check(inferBendCouponFactors(samples,&factors,&error),"rebuild measured dataset");
 factors.push_back({1,90,.40}); // same ratio but conflicting measurement
 check(!interpolateCouponKFactors(factors,{{2,2,90}},&plan,&error),"duplicate contradictory measurements rejected");
 factors.back().kFactor=.34;
 check(interpolateCouponKFactors(factors,{{2,2,90}},&plan,&error),"duplicate agreeing measurements supported");
 check(std::abs(plan[0].kFactor-.33)<1e-10,"duplicate calibrated average");
 factors.back().kFactor=NAN;
 check(!interpolateCouponKFactors(factors,{{2,2,90}},&plan,&error),"corrupted sample rejected");
 check(plan.size()==1&&std::abs(plan[0].kFactor-.33)<1e-10,"failed calibration preserves result");
 std::cout<<n<<" measured-bend-coupon assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

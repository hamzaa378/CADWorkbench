#include "core/ToleranceRescuePlan.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace cad::workshop;
static int n=0;
static void check(bool v,const char* msg){++n;if(!v)throw std::runtime_error(msg);}
int main(){try{
 // Deterministic grid of 400 adversarial combinations, including opposite
 // signs, one-sided zero deviations, exact boundaries, and shop limits.
 std::string err;
 for(int sign:{-1,1})for(int lower=0;lower<10;++lower)
 for(int upper=0;upper<10;++upper)for(int shop=0;shop<2;++shop){
   const double lo=lower*.0125,hi=upper*.0175;
   std::vector<ToleranceTerm> chain={{"offset",100,-lo,hi,sign},{"datum",97,-.2,.1,-sign}};
   ToleranceResult stack;
   check(calculateToleranceStack(chain,&stack,&err),"valid reference chain");
   const double allowedMin=stack.nominalMm-.12;
   const double allowedMax=stack.nominalMm+.08;
   const double floor=shop?.8:0;
   ToleranceRescuePlan plan;
   check(planUniformToleranceRescue(chain,allowedMin,allowedMax,floor,&plan,&err),"plan always completes");
   check(plan.proposedScale>=floor-1e-12&&plan.proposedScale<=1,"honor shop bounds");
   check(plan.proposedTerms.size()==chain.size(),"same bill of dimensions");
   for(std::size_t i=0;i<chain.size();++i){
     check(plan.proposedTerms[i].nominalMm==chain[i].nominalMm,"nominal exact");
     check(plan.proposedTerms[i].direction==chain[i].direction,"direction exact");
     check(std::abs(plan.proposedTerms[i].upperDeviationMm-
        chain[i].upperDeviationMm*plan.proposedScale)<1e-12,"upper limit scaled");
   }
   if(plan.feasibleWithinShopFloor)
     check(plan.after.minimumMarginMm>=-1e-9&&plan.after.maximumMarginMm>=-1e-9,"reported success really fits");
 }
 std::cout<<n<<" deterministic tolerance property assertions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

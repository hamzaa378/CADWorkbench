#include "core/ToleranceRescuePlan.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace cad::workshop;
static int checks=0;
static void expect(bool v,const char* msg){++checks;if(!v)throw std::runtime_error(msg);}
static bool close(double a,double b){return std::abs(a-b)<1e-9;}
int main(){try{
 std::string error;
 std::vector<ToleranceTerm> terms={{"housing",50,-.15,.10,1},{"bearing",20,-.05,.08,-1},{"shim",29.5,-.02,.03,-1}};
 ToleranceRescuePlan plan;
 expect(planUniformToleranceRescue(terms,.30,.60,0,&plan,&error),"rescue calculation valid");
 expect(error.empty(),"success clears error");
 expect(plan.feasibleWithinShopFloor,"unrestricted shop tolerances allow rescue");
 expect(close(plan.originalNominalMm,.5),"nominal preserved");
 expect(close(plan.requiredMaximumScale,.1/.17),"tight side drives reduction");
 expect(close(plan.proposedScale,plan.requiredMaximumScale),"minimum necessary uniform tightening");
 expect(close(plan.after.maximumMarginMm,0),"tight limit exactly reached");
 expect(plan.after.minimumMarginMm>0,"other limit satisfied");
 expect(plan.proposedTerms[0].nominalMm==50,"component nominal must never change");
 expect(plan.proposedTerms[1].direction==-1,"sign of component must never change");
 expect(close(plan.proposedTerms[0].lowerDeviationMm,-.15*plan.proposedScale),"signed down tolerance scaled");
 expect(planUniformToleranceRescue(terms,.30,.60,.6,&plan,&error),"shop limit still yields report");
 expect(!plan.feasibleWithinShopFloor,"minimum shop scale prevents rescue");
 expect(close(plan.proposedScale,.6),"shop floor honored");
 expect(plan.after.maximumMarginMm<0,"remaining deviation reported");
 expect(planUniformToleranceRescue(terms,.1,1,.9,&plan,&error),"already feasible should be retained");
 expect(plan.feasibleWithinShopFloor&&close(plan.proposedScale,1),"do not alter satisfactory fit");
 expect(planUniformToleranceRescue(terms,.7,1,0,&plan,&error),"infeasible nominal reported");
 expect(!plan.feasibleWithinShopFloor&&plan.nominalOutsideEnvelope,"nominal shift required, tightening cannot fix");
 expect(close(plan.proposedScale,1),"never fake a nominal shift");
 auto previous=plan;
 expect(!planUniformToleranceRescue(terms,0,1,-1,&plan,&error),"invalid negative shop floor");
 expect(close(plan.proposedScale,previous.proposedScale),"rollback on invalid shop floor");
 expect(!planUniformToleranceRescue(terms,0,1,NAN,&plan,&error),"nan shop floor");
 expect(!planUniformToleranceRescue(terms,1,0,0,&plan,&error),"inverted fit limits");
 expect(!planUniformToleranceRescue(terms,0,1,0,nullptr,&error),"null output");
 auto shifted=terms;shifted[0].lowerDeviationMm=0.1;
 expect(!planUniformToleranceRescue(shifted,0,1,0,&plan,&error),"deviation not around nominal rejected");
 shifted=terms;shifted[1].upperDeviationMm=-.01;
 expect(!planUniformToleranceRescue(shifted,0,1,0,&plan,&error),"other one-sided interval rejected");
 expect(plan.proposedTerms.size()==previous.proposedTerms.size(),"failure leaves prior report unchanged");
 std::cout<<checks<<" tolerance rescue checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
